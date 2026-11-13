# procguard

部署 bot 时它经常半夜无声退出，日志里什么都没有，只能每隔几小时 `ps` 一次。
这个工具是为了让那句话消失。

**当前版本 v0.3** —— 守护任意命令，命令可以写在命令行，也可以从配置文件读。

## 用法

```
procguard <command> [args...]      # 命令写在命令行
procguard -c <config>              # 命令从配置文件读
```

```bash
./procguard ./crasher 3                        # 守护一个会随机崩的测试进程
./procguard /usr/bin/python3 -m http.server    # 参数原样传给子进程
./procguard -c test/demo.conf                  # 从配置文件读命令
```

不带参数时打印用法并退出，退出码 1。

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

`[section]` 目前只做跳过，v0.3.1 支持多服务时会用上。

配置写错时打印行号并以 1 退出：

```
line 1: missing '='
line 2: command is empty
line 3: max_delay is not a number
line 3: max_delay must be >= 1
missing required field: command        # 整个文件没有 command 字段
```

未知字段只警告，不退出。

## 构建

需要 g++（C++17）和 make。

```bash
make          # 生成 ./procguard
make debug    # 带 -g -O0，能进 gdb
make release  # -O2
make clean
```

## 行为

- **拉起**：`fork` + `execvp`，被守护的命令退出后自动重启
- **进程组**：子进程用 `setpgid` 独立成组，停止时向整组发 `SIGTERM`，不留孤儿
- **退避**：1 → 2 → 4 → 8 → … → `max_delay` 封顶（默认 30）；稳定运行 60 秒后重置为 1
- **停止**：收到 `SIGINT` 后杀掉子进程组、回收、退出，退出码 0
- **exec 失败**：打印 `execvp: <原因>`，子进程以 127 退出
- **waitpid**：被信号打断（EINTR）时重试，但用户已要求停止就不再等

## 测试

```bash
make crasher    # 编译测试靶子
make demo       # 编译后直接守护它
```

靶子每次启动随机挑一种死法：正常退出（0 / 1 / 3）、`SIGSEGV`、`SIGABRT`、长睡不醒。
也可以指定：`./crasher 3`。

`test/demo.conf` 是最小配置示例。

## 版本

| 版本 | 日期 | 内容 |
|---|---|---|
| v0.1 | 2026-10-29 | `fork`/`execl` 拉起、指数退避、SIGINT 清理进程组 |
| v0.1.1 | 2026-11-05 | `waitpid` EINTR 重试、错误状态拦截、父进程同步 `setpgid` |
| v0.2 | 2026-11-08 | 守护任意命令（argv 透传）、用法提示与退出码约定、Makefile |
| v0.2.1 | 2026-11-08 | 修复：退避等待中按 Ctrl+C 时仍会多打印一行「下一轮等待」 |
| v0.3 | 2026-11-13 | 配置文件（`command` / `max_delay`）、带行号的错误提示、退避改为先等待再翻倍 |

## 已知限制

- 只守护一个服务，多服务在 v0.3.1
- 没有健康检查：进程卡住不退出（死锁）时发现不了
- 没有指标端点，看不到历史重启记录
- procguard 自己挂了没人拉起——真要部署得交给 systemd
