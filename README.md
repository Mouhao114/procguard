# procguard

进程守护：命令退出就自动重启，指数退避，Ctrl+C 时清理整个进程组。

当前版本 v0.3

## 用法

```bash
procguard <command> [args...]      # 命令写在命令行
procguard -c <config>              # 命令从配置文件读
```

```bash
./procguard ./crasher 3
./procguard /usr/bin/python3 -m http.server
./procguard -c test/demo.conf
```

不带参数时打印用法并退出，退出码 1；`-c` 后面没跟文件路径同样报用法并退出。

## 配置文件

```
# 以 # 开头的行和空行会被忽略
[service.demo]
command=/bin/sleep 5
max_delay=10
```

| 字段 | 必填 | 说明 |
|---|---|---|
| `command` | 是 | 按空格切成 argv，交给 `execvp` |
| `max_delay` | 否 | 退避上限（秒），默认 30，必须 ≥ 1 |

`command` 只按空格切分，**不解析引号**。所以 `command=/bin/echo "hello world"` 得到的是三个参数（`/bin/echo`、`"hello`、`world"`），不是两个。

要传带空格的参数或需要 shell 特性，把逻辑写进脚本，用不含空格的路径调起来：

```
command=/bin/sh /home/user/run.sh
```

写错时打印行号并以 1 退出；未知字段只警告、继续执行。整份文件读完仍没有 `command` 时，报 `missing required field: command`。多服务支持将在未来版本提供。

## 构建

需要 g++（C++17）和 make。

```bash
make          # 生成 ./procguard
make debug    # -g -O0
make release  # -O2 -DNDEBUG
```

## 行为

- **重启**：被守护的命令退出后自动重启
- **退避**：1 → 2 → 4 → … → `max_delay` 封顶。子进程单次存活超过 60 秒时，退避重置回 1 秒——崩得越频繁等得越久，跑得住的不惩罚
- **停止**：收到 `SIGINT` 或 `SIGTERM` 后向子进程组发 `SIGTERM`、回收、退出，退出码 0。不把收到的停止信号原样透传下去，怎么收尾由自己决定

## 测试

```bash
make crasher        # 只编译靶子
./crasher           # 随机挑一种死法
./crasher 3         # 指定第 3 种，方便复现
make demo           # 编译并直接守护它，Ctrl+C 停止
```

靶子的六种死法：退出码 0 / 1 / 3、`SIGSEGV`、`SIGABRT`、长睡不醒。

## 已知限制

- 被守护的命令不存在或没有执行权限时，`execvp` 失败（退出码 127），procguard 当作普通退出处理，继续按退避重启，不会放弃。区别只体现在日志里的退出码
- 没有健康检查：进程卡住不退出（死锁）时发现不了
- 没有指标端点，看不到历史重启记录
- 日志只往 stdout 走，不落盘也不轮转；长期挂着得自己重定向
- 改配置要重启进程，没有 reload
- 停止时向子进程组发 `SIGTERM` 后阻塞等待，没有超时也没有后续的 `SIGKILL`。子进程不理 `SIGTERM`（或进程组里还卡着孙进程）会把 procguard 挂住，只能手动结束它和残留的子进程
- procguard 自己挂了没人拉起——真要部署得交给 systemd
