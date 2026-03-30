#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

extern int wtsl_core_main();
extern int wtsl_http_main_stop();
extern int wtsl_core_release();
// 信号处理函数
void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:  // 捕获 Ctrl+C (SIGINT)
            printf("\nrecv SIGINT (Ctrl+C),exiting...\n");
            wtsl_http_main_stop();
            wtsl_core_release();
            break;
        case SIGTERM: // 捕获 kill 命令发送的 SIGTERM 信号
            printf("\nrecv SIGTERM (kill),exiting...\n");
            wtsl_http_main_stop();
            wtsl_core_release();
            break;
        default:
            printf("recv unknown signal: %d\n", signum);
            break;
    }
}

int wteapp_main() {

	//init_log_manager(4);
	//example_main();
    wtsl_core_main();
    return 0;
}


int main(){

	
	// 使用 sigaction 注册信号处理函数（比 signal 更推荐，行为更稳定）
    struct sigaction sa;
    sa.sa_handler = signal_handler;  // 设置处理函数
    sigemptyset(&sa.sa_mask);        // 清空信号掩码（处理信号时不阻塞其他信号）
    sa.sa_flags = 0;                 // 无特殊标志

    // 注册 SIGINT 信号（Ctrl+C）
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT) failed");
    }

    // 注册 SIGTERM 信号（kill 命令默认发送）
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM) failed");
    }
	fprintf(stderr,"[%s][%d] In register sigact ok\n",__FUNCTION__,__LINE__);

	return wteapp_main();
}