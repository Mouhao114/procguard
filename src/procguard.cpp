#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

volatile sig_atomic_t g_stop = 0;

void on_signal(int);

int main(int argc, char* argv[]) {
    struct sigaction sa;
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    int delay = 1;
    std::vector<std::string> cmd;
    if (argc < 2) {
        std::cerr << "usage: procguard <command> [args...]\n";
        return 1;
    }
    if (std::string(argv[1]) == "-c") {
        if (argc < 3) {
            std::cerr << "usage: procguard -c <config>\n";
            return 1;
        }
        std::ifstream in(argv[2]);
        if (!in) {
            std::cerr << "cannot open: " << argv[2] << "\n";
            return 1;
        }
        std::string line;
        int lineno = 0;
        while (std::getline(in, line)) {
            ++lineno;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;
            // [xxx] → 认下 section，continue
            if(line.front() == '[' && line.back() == ']'){
                continue;
            }   
            // 走到这里，这行应该是 key=value
            auto eq = line.find('=');
            if (eq == std::string::npos) {
                std::cerr << "line " << lineno << ": missing '='\n";
                return 1; 
            }
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            if (key == "command") {
                if (value.empty()) {
                    std::cerr << "line " << lineno << ": command is empty\n";
                    return 1;
                }
                std::istringstream iss(value);
                std::string w;
                while (iss >> w) cmd.push_back(w);
            } else if (key == "max_delay") {
                continue;
                // 先存着，暂时不用
            } else {
                std::cerr << "Warning:unknown keys: " << key << "\n";
                continue;
            }
        }
        if (cmd.empty()) {
            std::cerr << "missing required field: command\n";
            return 1;
        }
    } else {
        for (int i = 1; i < argc; ++i)
            cmd.push_back(argv[i]);
    }
    std::vector<char*> cargv;
    for (auto& s : cmd)
        cargv.push_back(s.data());
    cargv.push_back(nullptr);
    while (!g_stop) {
        pid_t pid = fork();
        int startTime = time(nullptr);
        int status = 0;
        if (pid < 0) {
            std::cout << "未能创建子进程\n";
            return 1;
        } else if (pid == 0) {
            setpgid(0, 0);
            execvp(cargv[0], cargv.data());
            perror("execvp");
            _exit(127);
        } else {
            setpgid(pid, pid);
            std::cout << "父进程,id:" << getpid() << "\n"
                      << "子id:" << pid << std::endl;
        }
        pid_t w;
        do {
            w = waitpid(pid, &status, 0);
            if (w < 0 && errno == EINTR) {
                if (g_stop == 1) {
                    kill(-pid, SIGTERM);
                    waitpid(pid, &status, 0);
                    break;
                }
            }
        } while (w < 0 && errno == EINTR && !g_stop);
        int ran = time(nullptr) - startTime;
        if (w < 0 && errno != EINTR) {
            std::cout << strerror(errno) << std::endl;
            break;
        }
        if (WIFEXITED(status)) {
            std::cout << "子进程正常退出，退出码=" << WEXITSTATUS(status) << std::endl;
        } else {
            std::cout << "子进程被信号 " << WTERMSIG(status) << " kill" << std::endl;
        }
        if (g_stop) {
            break;
        }

        if (ran > 60) {
            delay = 1;
        } else {
            delay = std::min(delay * 2, 30);
        }
        std::cout << "[delay] 本轮子进程存活 " << ran << " 秒，下一轮等待 " << delay << " 秒"
                  << std::endl;
        sleep(delay);
    }
    std::cout << "[main] 收到停止信号，正常退出" << std::endl;
    return 0;
}

void on_signal(int) {
    g_stop = 1;
}
