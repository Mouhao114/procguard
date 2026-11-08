// crasher.cpp —— 给 procguard 当靶子的测试进程
//
// 每次启动随机挑一种死法，先睡 1~3 秒，然后执行：
//   0  正常退出，退出码 0
//   1  正常退出，退出码 1
//   2  正常退出，退出码 3
//   3  自己给自己发 SIGSEGV（段错误，父进程看到 WTERMSIG == 11）
//   4  自己给自己发 SIGABRT（abort，WTERMSIG == 6）
//   5  一直不退出（测 Ctrl+C，以后测健康检查）
//
// 编译：g++ -std=c++17 -Wall -Wextra -Wshadow -o crasher crasher.cpp
// 用法：./crasher        随机挑一种
//       ./crasher 3      只跑第 3 种，方便复现

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char* argv[]) {
    // 拿 pid 当种子：同一秒里连开好几次也能拿到不同结果。
    // 用 time(nullptr) 的话，同一秒内启动的进程会挑到同一种死法。
    srand(getpid());

    int mode = (argc > 1) ? atoi(argv[1]) : rand() % 6;
    int secs = 1 + rand() % 3;

    // 用 stderr 不用 stdout：stderr 默认不带缓冲，
    // 进程被信号打死时这一行不会跟着缓冲区一起丢掉。
    std::fprintf(stderr, "[crasher %d] mode=%d，%d 秒后动手\n", getpid(), mode, secs);
    sleep(secs);

    switch (mode) {
    case 0:
        std::fprintf(stderr, "[crasher %d] 正常退出，退出码 0\n", getpid());
        return 0;
    case 1:
        std::fprintf(stderr, "[crasher %d] 正常退出，退出码 1\n", getpid());
        return 1;
    case 2:
        std::fprintf(stderr, "[crasher %d] 正常退出，退出码 3\n", getpid());
        return 3;
    case 3:
        std::fprintf(stderr, "[crasher %d] raise(SIGSEGV)\n", getpid());
        raise(SIGSEGV);
        return 0; // 到不了这行
    case 4:
        std::fprintf(stderr, "[crasher %d] raise(SIGABRT)\n", getpid());
        raise(SIGABRT);
        return 0; // 到不了这行
    default:
        std::fprintf(stderr, "[crasher %d] 长睡不醒，等你按 Ctrl+C\n", getpid());
        sleep(3600);
        return 0;
    }
}
