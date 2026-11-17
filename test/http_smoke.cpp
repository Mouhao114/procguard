// http.cpp 的独立验靶：不依赖 procguard 的任何东西。
//
// 编译：
//   g++ -std=c++17 -Wall -Wextra -Wshadow -o /tmp/http_smoke src/http.cpp test/http_smoke.cpp
// 运行：
//   /tmp/http_smoke &
//   curl -i http://127.0.0.1:8099/api/status
//
// 它演示的是主循环该怎么接：
//   poll 等 listen fd → accept → http_handle → 回去继续 poll
// 换成 procguard 的真主循环时，timeout 那个参数会从"3 秒"变成
// "距离下一个服务该重启还有多少毫秒"。

#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

#include "../src/http.h"

int main() {
    const int port = 8099;
    int lfd = http_listen(port);
    if (lfd < 0) return 1;
    std::printf("listening on http://127.0.0.1:%d/api/status\n", port);

    // 造两条假数据：一条正常，一条重启超阈值（用来验前端告警）
    std::vector<ServiceStatus> snap(2);

    snap[0].name             = "web";
    snap[0].pid              = 5102;
    snap[0].state            = "sleeping";
    snap[0].uptime_sec       = 187;
    snap[0].restarts         = 0;
    snap[0].last_exit_at     = 0;

    snap[1].name             = "indexer";
    snap[1].pid              = 6140;
    snap[1].state            = "zombie";
    snap[1].uptime_sec       = 0;
    snap[1].restarts         = 63;
    snap[1].last_exit_signal = 11;
    snap[1].last_exit_at     = 1763350000;

    while (true) {
        struct pollfd p;
        p.fd      = lfd;
        p.events  = POLLIN;
        p.revents = 0;

        int r = poll(&p, 1, 3000);
        if (r < 0) {
            if (errno == EINTR) continue;  // 被信号打断，正常，重来
            perror("poll");
            break;
        }
        if (r == 0) continue;  // 三秒没人来，回到循环顶部（真实现里这里该检查重启时间表）

        if (p.revents & POLLIN) {
            int cfd = accept(lfd, nullptr, nullptr);
            if (cfd < 0) continue;
            http_handle(cfd, snap);  // 里面会 close
        }
    }

    close(lfd);
    return 0;
}
