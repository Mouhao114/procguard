#ifndef PROCGUARD_STATUS_H
#define PROCGUARD_STATUS_H

#include <string>
#include <sys/types.h>

// 一个服务的状态快照。
//
// 这是 http 和主循环之间唯一的耦合：主循环负责填，http 负责读，
// 两边都不需要知道对方的任何东西。所以它能单独成一个头文件。
//
// 字段名必须和 Watcher.html 里的 mockStatus() 一模一样，
// 否则前端接上后端那天会突然缺字段。
struct ServiceStatus {
    std::string name;
    pid_t       pid = 0;                // 0 表示当前没有活着的进程
    std::string state;                  // running | sleeping | zombie | stopped | unknown
    long long   uptime_sec = 0;         // 本进程已存活秒数
    int         restarts = 0;           // 累计重启次数
    int         last_exit_code = -1;    // -1 表示没有退出码（被信号杀的，或从没退出过）
    int         last_exit_signal = 0;   // 0 表示不是被信号杀的
    long long   last_exit_at = 0;       // unix 秒，0 表示从没退出过
};

#endif
