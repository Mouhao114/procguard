#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t g_stop = 0;

void on_signal(int);

int main() {
    struct sigaction sa;
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    int delay = 1;
    while (!g_stop) {
        pid_t pid = fork();
        int startTime = time(nullptr);
        int status = 0;
        if (pid < 0) {
            std::cout << "未能创建子进程\n";
            return 1;
        } else if (pid == 0) {
            setpgid(0, 0);
            execl("/bin/sh", "sh", "-c", "sleep 5; exit 3", nullptr);
            std::cout << "会打印吗?" << " " << std::endl;
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
            std::cout << "子进程被信号 " << WTERMSIG(status) << "kill" << std::endl;
        }
        if (g_stop) {
            break;
        }
        sleep(delay);
        if (ran > 60) {
            delay = 1;
        } else {
            delay = std::min(delay * 2, 30);
        }
        std::cout << "[delay] 本轮子进程存活 " << ran << " 秒，下一轮等待 " << delay << " 秒"
                  << std::endl;
    }
    std::cout << "[main] 收到停止信号，正常退出" << std::endl;
    return 0;
}

void on_signal(int) {
    g_stop = 1; // ← 处理函数改它
}
