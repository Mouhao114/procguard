#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <algorithm>
#include <csignal>
#include <cerrno>

volatile sig_atomic_t g_stop = 0;

void on_signal(int);

int main(){
    struct sigaction sa;
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    int delay=1;
    while (!g_stop){
    pid_t pid =fork();
    int startTime=time(nullptr);
    int status=0;
        if(pid<0){
            std::cout<<"未能创建子进程\n";
            return 1;
        }else if(pid==0){
            setpgid(0, 0);
                execl("/bin/sh", "sh", "-c", "sleep 5; exit 3", nullptr);
            std::cout<<"会打印吗?"<<" "<<std::endl;
            _exit(127);
        }else{
            std::cout<<"父进程,id:"<<getpid()<<"\n"<<"子id:"<<pid<<std::endl;
        }
        pid_t w= waitpid(pid,&status,0);
        if (w < 0 && errno == EINTR){
            if(g_stop==1){
                kill(-pid, SIGTERM);          //← 负号，整组
                waitpid(pid, &status, 0);     //← 收尸，别留僵尸
                break;
            }else continue;
        }
        int ran= time(nullptr)-startTime;
        if(WIFEXITED(status)){
            std::cout<<"子进程正常退出，退出码="<<WEXITSTATUS(status)<<std::endl;
        }else{
        std::cout<<"子进程被信号 "<<WTERMSIG(status)<<"kill"<<std::endl;
        }
        sleep(delay);
        if(ran>60){
            delay=1;
        }
        else {
            delay=std::min(delay*2,30);
        }
        std::cout<<"[delay] 本轮子进程存活 "<<ran<<" 秒，下一轮等待 "<<delay<<" 秒"<<std::endl;
    }
    std::cout << "[main] 收到停止信号，正常退出" << std::endl;
    return 0;
}

void on_signal(int) {
    g_stop = 1;                   // ← 处理函数改它
}