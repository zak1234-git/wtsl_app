/*
 * Copyright (c) @CompanyNameMagicTag. 2023. All rights reserved.
 * Description: Main function of sle client sample.
 */
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "stdint.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_conn_client.h"
#include "wtsl_core_api.h"

#define SLE_CLIENT_SLEEP_TIME  1

uint8_t g_running = 1;

extern uint16_t g_conn_id;
extern SPLINK_INFO global_node_info;

void signal_handler(int signum)
{
    printf("recv signal %d: %s\n", signum, strsignal(signum));
    g_running = 0;
    sle_stop_tcp_server();
}

void register_signal(void)
{
    // 注册 SIGINT (Ctrl+C) 信号的处理函数
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        printf("register signal SIGINT failed.\n");
    }
    // 注册 SIGTERM 信号的处理函数
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        printf("register signal SIGTERM failed.\n");
    }
    // 注册 SIGTERM 信号的处理函数
    if (signal(SIGSEGV, signal_handler) == SIG_ERR) {
        printf("register signal SIGSEGV failed.\n");
    }
    // 注册 SIGTERM 信号的处理函数
    if (signal(SIGKILL, signal_handler) == SIG_ERR) {
        printf("register signal SIGKILL failed.\n");
    }
    // 注册 SIGTERM 信号的处理函数
    if (signal(SIGILL, signal_handler) == SIG_ERR) {
        printf("register signal SIGILL failed.\n");
    }
}

void* wtsl_core_sle(void * args)
{
	(void)args;
    printf("main start.\n");

/*    if (argc > 1) {
        int cnt = atoi(argv[1]);
        if (cnt < 1 || cnt > 20) { // 支持的服务器数量1-20
            printf("valid range: 1 ~ 20\n");
            cnt = 1;
        }
        sle_conn_set_max_server_cnt(cnt);
    } else {
        printf("need param, valid range: 1 ~ 20\n");
        sle_conn_set_max_server_cnt(1);
        // return;
    }*/
	//sle_conn_set_max_server_cnt(1);

    register_signal();  // 注册信号处理

	if(global_node_info.node_info.basic_info.sle_type == NODE_TYPE_SLE_G)
    	sle_client_init();  // SLE client初始化
	else if(global_node_info.node_info.basic_info.sle_type == NODE_TYPE_SLE_T)
		sle_server_init();  // SLE server初始化
	else
		printf("sle type error: %d\n", global_node_info.node_info.basic_info.sle_type);
    create_tcp_server();

    while (g_running) {  // 收到信号g_running状态改变
        sleep(SLE_CLIENT_SLEEP_TIME);  // 5
        // sle_client_senddata("hello world", strlen("hello world"));
    }
    
    sleep(1);
    sle_client_deinit();  // SLE client去初始化

    printf("main end.\n\n");
}