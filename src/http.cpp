#include "http.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr int    kBacklog      = 16;
constexpr size_t kReqMax       = 2048;  // 一个 GET 请求几百字节，2K 足够，超了直接丢
constexpr int    kIoTimeoutSec = 2;     // 慢客户端最多占住 2 秒

// 面板页面，从**当前工作目录**读。必须是固定文件名——不接受请求里的路径，
// 所以不存在目录穿越。读不到就返回 404，提示在仓库根目录启动。
constexpr const char* kWebFile = "Watcher.html";

bool read_file(const char* path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// JSON 字符串转义。进程名理论上不该有引号，但这里是唯一一处
// "用户输入"进入响应体的地方，不做就是给自己埋雷。
std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    std::snprintf(esc, sizeof esc, "\\u%04x", c);
                    out += esc;
                } else {
                    out += static_cast<char>(c);  // UTF-8 高位字节原样放过
                }
        }
    }
    return out;
}

void set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void set_io_timeout(int fd, int sec) {
    struct timeval tv;
    tv.tv_sec  = sec;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

std::string build_json(const std::vector<ServiceStatus>& snap, long long now) {
    std::string out = "{\"now\":" + std::to_string(now) + ",\"services\":[";
    for (size_t i = 0; i < snap.size(); ++i) {
        const ServiceStatus& s = snap[i];
        if (i) out += ',';

        out += "{\"name\":\"" + json_escape(s.name) + "\"";
        out += ",\"pid\":";
        out += (s.pid > 0) ? std::to_string(s.pid) : "null";
        out += ",\"state\":\"" + json_escape(s.state) + "\"";
        out += ",\"uptime_sec\":" + std::to_string(s.uptime_sec);
        out += ",\"restarts\":" + std::to_string(s.restarts);

        if (s.last_exit_at == 0) {
            out += ",\"last_exit\":null";
        } else {
            out += ",\"last_exit\":{\"code\":";
            out += (s.last_exit_code >= 0) ? std::to_string(s.last_exit_code) : "null";
            out += ",\"signal\":";
            out += (s.last_exit_signal > 0) ? std::to_string(s.last_exit_signal) : "null";
            out += ",\"at\":" + std::to_string(s.last_exit_at) + "}";
        }

        // 这两项还没实现。给 null 而不是缺字段，前端已经能处理 null。
        out += ",\"cpu_pct\":null,\"rss_kb\":null";
        out += '}';
    }
    out += "]}";
    return out;
}

// send 可能只写出去一部分（内核缓冲区满了），必须循环到写完。
// MSG_NOSIGNAL：对端提前关了，不要用 SIGPIPE 把整个进程打死。
void send_all(int fd, const std::string& s) {
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (n > 0) {
            off += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        break;  // 对端断了或者超时，这个连接救不回来了
    }
}

void reply(int fd, int code, const char* reason, const char* type, const std::string& body) {
    std::string head =
        "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n" +
        "Content-Type: " + type + "\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Cache-Control: no-store\r\n" +
        "Connection: close\r\n" +
        "\r\n";
    send_all(fd, head + body);
}

}  // namespace

int http_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int on = 1;
    // 不加这条，进程重启后端口会在 TIME_WAIT 里躺一分钟，bind 直接失败。
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 部署时应改成 INADDR_LOOPBACK

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, kBacklog) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    // poll 说可读之后，accept 仍可能因为并发或 EINTR 返回 EAGAIN，
    // 非阻塞能保证它立刻返回而不是把主循环卡住。
    set_nonblock(fd);
    return fd;
}

void http_handle(int client_fd, const std::vector<ServiceStatus>& snap) {
    set_io_timeout(client_fd, kIoTimeoutSec);

    // 只读一次。HTTP 请求头可能有多个包，但这里不需要读完整个请求——
    // 第一行就够判断方法和路径，剩下的头一律忽略。
    char buf[kReqMax];
    ssize_t n = recv(client_fd, buf, sizeof buf - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    std::string req(buf, static_cast<size_t>(n));
    size_t eol = req.find("\r\n");
    const std::string first = req.substr(0, eol == std::string::npos ? req.size() : eol);

    // 请求行格式：METHOD SP PATH SP VERSION
    size_t sp1 = first.find(' ');
    if (sp1 == std::string::npos) {
        reply(client_fd, 400, "Bad Request", "text/plain; charset=utf-8", "bad request\n");
        close(client_fd);
        return;
    }
    size_t sp2 = first.find(' ', sp1 + 1);

    const std::string method = first.substr(0, sp1);
    std::string path = first.substr(sp1 + 1,
                                    sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1);

    // 砍掉 query string：/api/status?x=1 也当作 /api/status
    size_t q = path.find('?');
    if (q != std::string::npos) path.resize(q);

    if (method != "GET") {
        reply(client_fd, 405, "Method Not Allowed", "text/plain; charset=utf-8", "only GET\n");
    } else if (path == "/api/status") {
        long long now = static_cast<long long>(std::time(nullptr));
        reply(client_fd, 200, "OK", "application/json", build_json(snap, now));
    } else if (path == "/" || path == "/Watcher.html" || path == "/index.html") {
        // 页面和接口同一个源，浏览器才不会以跨域为由拦掉 fetch('/api/status')。
        std::string page;
        if (read_file(kWebFile, page)) {
            reply(client_fd, 200, "OK", "text/html; charset=utf-8", page);
        } else {
            reply(client_fd, 404, "Not Found", "text/plain; charset=utf-8",
                  std::string("cannot open ") + kWebFile + " — 请在仓库根目录启动\n");
        }
    } else {
        reply(client_fd, 404, "Not Found", "text/plain; charset=utf-8", "not found\n");
    }

    close(client_fd);
}
