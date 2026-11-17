#ifndef PROCGUARD_HTTP_H
#define PROCGUARD_HTTP_H

#include <vector>

#include "status.h"

// 起一个监听 socket 并返回 fd；失败返回 -1（错误已打到 stderr）。
//
// 返回的 fd 是**非阻塞**的：主循环用 poll 等它，accept 不会把循环卡住。
// 端口被占用、没权限绑低端口，都在这里失败。
int http_listen(int port);

// 处理一个已经 accept 的连接：读一次请求、写一个响应、关掉 fd。
//
// 约定：
//   - 不抛异常，任何错误都只是"把这个连接关掉"
//   - 读写都设了超时，慢客户端最多占住 2 秒
//   - GET /api/status  → JSON 快照
//   - GET / 或 /Watcher.html → 返回当前目录下的 Watcher.html
//     （页面和接口同源，前端 fetch('/api/status') 才不会被跨域拦掉）
//   - 其余返回 404 / 405
void http_handle(int client_fd, const std::vector<ServiceStatus>& snap);

#endif
