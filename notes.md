# procguard 开发笔记


## v0.3 配置格式

[service.demo]
command=/bin/sleep 30
max_delay=30

- command 必填，按空格切 argv
- max_delay 可选，默认 30
- 未知 key 警告不退出
- 错误必须带行号

## 系统调用（`man 2 xxx`，不是第 3 节）

| 调用 | 一句话 | 容易踩的坑 |
|---|---|---|
| `fork` | 复制当前进程 | 返回值：父得子 pid、子得 0、失败 -1 |
| `waitpid` | 回收子进程 | 被信号打断返回 -1 + `EINTR`；不回收就是僵尸 `Z` |
| `execl` 家族 | 换掉进程映像 | 成功不返回；后面必须 `_exit(127)` 兜底，否则 fork 炸弹 |
| `setpgid(0,0)` | 子进程自立进程组 | 是 `kill(-pid)` 能打全组的前提 |
| `kill(pid)` / `kill(-pid)` | 单个进程 / 整个组 | **负号才是整组**，少一个负号就留孤儿 |
| `sigaction` | 注册信号处理 | `sigemptyset(&sa.sa_mask)` 不能省，不清空是未定义行为 |

## 信号

- 处理函数里**只能设标志位**（`volatile sig_atomic_t`），不能 `cout`/`printf`——内部加锁会死锁
- 标志位必须 `volatile`，否则编译器缓存进寄存器，循环永远读到旧值，且不报警告
- `SIGTERM` 可捕获，`SIGKILL` 不可捕获
- 处于 stopped 状态的进程**不响应 SIGTERM**，只有 SIGKILL 能杀掉
- `sa_flags = SA_RESTART` 会让被中断的 `waitpid` 自动重启，`EINTR` 分支永远进不去

## 调试

- **改完代码先确认跑的是新二进制**：`stat -c "%y" src/procguard.cpp procguard`，二进制时间必须更晚
- 僵尸清不掉：父进程死掉后由 init 回收
- 孤儿：父死子活，`PPID=1`

## git / 网络

- 本机卡巴斯基的**行为防护**会拦截高危行为（实测表现是 SSH 出站连不上），不是按端口黑名单拦的，所以从端口表象看不出原因。
- `git status --short` 两个字母：`AM` = 暂存后又改过，要再 `git add`
- commit 前确认二进制、`*.o` 没被加进去
