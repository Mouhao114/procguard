#include <algorithm>
#include <chrono>
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
#include <stdexcept>

volatile sig_atomic_t g_stop = 0;

void on_signal(int);

int main(int argc, char* argv[]) {
    struct sigaction sa;
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    int delay = 1;
    int max_delay = 30;
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
            if (line.front() == '[' && line.back() == ']') {
                continue;
            }
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
                try {
                    max_delay = std::stoi(value);
                } catch (const std::invalid_argument&) {
                    std::cerr << "line " << lineno << ": max_delay is not a number\n";
                    return 1;
                } catch (const std::out_of_range&) {
                    std::cerr << "line " << lineno << ": max_delay is out of range\n";
                    return 1;
                }
                if (max_delay < 1) {
                    std::cerr << "line " << lineno << ": max_delay must be >= 1\n";
                    return 1;
                }
                if (max_delay > 3600) {
                    std::cerr << "line " << lineno << ": max_delay must be <= 3600\n";
                    return 1;
                }
            } else {
                std::cerr << "warning: unknown key '" << key << "'\n";
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

    int exit_code = 0;
    while (!g_stop) {
        pid_t pid = fork();
        auto startTime = std::chrono::steady_clock::now();
        int status = 0;
        if (pid < 0) {
            std::cerr << "fork failed\n";
            return 1;
        } else if (pid == 0) {
            setpgid(0, 0);
            execvp(cargv[0], cargv.data());
            perror("execvp");
            _exit(127);
        } else {
            setpgid(pid, pid);
            std::cout << "parent pid: " << getpid() << "\n"
                      << "child pid: " << pid << "\n";
        }
        pid_t w;
        do {
            w = waitpid(pid, &status, 0);
            if (w < 0 && errno == EINTR && g_stop == 1) {
                if (kill(-pid, SIGTERM) < 0 && errno != ESRCH) {
                    std::cerr << "kill: " << strerror(errno) << "\n";
                }
                pid_t wr;
                do {
                    wr = waitpid(pid, &status, 0);
                } while (wr < 0 && errno == EINTR);
                if (wr < 0) {
                    std::cerr << "waitpid (cleanup): " << strerror(errno) << "\n";
                    exit_code = 1;
                }
                break;
            }
        } while (w < 0 && errno == EINTR && !g_stop);

        auto ran = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - startTime).count();

        if (w < 0 && errno != EINTR) {
            std::cerr << "waitpid: " << strerror(errno) << "\n";
            exit_code = 1;
            break;
        }
        if (WIFEXITED(status)) {
            std::cout << "child exited with code " << WEXITSTATUS(status) << "\n";
        } else {
            std::cout << "child killed by signal " << WTERMSIG(status) << "\n";
        }
        if (g_stop) {
            break;
        }
        if (ran > 60) {
            delay = 1;
        }
        std::cout << "[delay] ran " << ran << "s, next start in " << delay << "s\n";
        sleep(delay);
        delay = std::min(delay * 2, max_delay);
    }
    if (exit_code == 0) {
        std::cout << "[main] stop signal received, exiting\n";
    } else {
        std::cout << "[main] exiting after error\n";
    }
    return exit_code;
}

void on_signal(int) {
    g_stop = 1;
}
