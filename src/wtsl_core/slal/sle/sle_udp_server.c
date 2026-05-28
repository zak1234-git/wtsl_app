#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include "wtsl_core_api.h"

//#define PORT 9981
#define BUFFER_SIZE 200

// 外部全局变量
static uint8_t g_udp_running = 1;
extern SPLINK_INFO global_node_info;
static int g_udp_server_fd = -1;

// 外部数据处理接口
extern void sle_client_senddata(uint8_t *data, uint16_t data_len);

// ===================== 核心：记住唯一客户端地址 =====================
static struct sockaddr_in g_udp_client_addr;  // 保存你的电脑IP+端口
static socklen_t g_udp_client_len = sizeof(g_udp_client_addr);
static uint8_t g_udp_has_client = 0;          // 是否已经记住客户端

/**
 * @brief  UDP数据接收线程（谁发过来，就记住谁）
 */
void *udp_recv_msg(void *arg) {
    int udp_fd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    struct sockaddr_in temp_client;
    socklen_t temp_len = sizeof(temp_client);

    printf("udp recv thread start, udp_fd:%d\n", udp_fd);

    while (g_udp_running) {
        memset(buffer, 0, BUFFER_SIZE);

        // 接收UDP数据
        bytes_read = recvfrom(udp_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0,
                              (struct sockaddr *)&temp_client,
                              &temp_len);

        if (bytes_read <= 0) {
            if (bytes_read < 0 && g_udp_running)
                perror("udp recvfrom failed");
            break;
        }

        // ===================== 关键：自动记住你的电脑客户端 =====================
        memcpy(&g_udp_client_addr, &temp_client, sizeof(temp_client));
        g_udp_client_len = temp_len;
        g_udp_has_client = 1;  // 标记已有客户端

        printf("udp 定点客户端已记录: %s:%d\n",
               inet_ntoa(g_udp_client_addr.sin_addr),
               ntohs(g_udp_client_addr.sin_port));

        // 数据交给上层
        sle_client_senddata((uint8_t *)buffer, (uint16_t)bytes_read);
    }

    printf("UDP接收线程退出\n");
    pthread_exit(NULL);
}

/**
 * @brief  创建UDP服务端（无广播、无风暴）
 */
void *create_udp_server() {
    int udp_fd;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    // 创建UDP套接字
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("udp socket failed");
        exit(EXIT_FAILURE);
    }
    g_udp_server_fd = udp_fd;

    // 绑定端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(global_node_info.node_info.basic_info.trans_udp_port);

    if (bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("udp bind failed");
        close(udp_fd);
        exit(EXIT_FAILURE);
    }

    // 初始化客户端信息
    memset(&g_udp_client_addr, 0, sizeof(g_udp_client_addr));
    g_udp_has_client = 0;

    printf("UDP 定点服务端启动成功，端口:%d，等待你的电脑连接...\n", global_node_info.node_info.basic_info.trans_udp_port);

    // 创建接收线程
    if (pthread_create(&recv_thread, NULL, udp_recv_msg, &udp_fd) != 0) {
        perror("udp pthread_create");
        close(udp_fd);
        exit(EXIT_FAILURE);
    }

    pthread_join(recv_thread, NULL);
    close(udp_fd);
    g_udp_server_fd = -1;
    printf("UDP server closed\n");
    return NULL;
}

/**
 * @brief  定点发送（只发给你的电脑，和TCP一样用法）
 */
int sle_udp_server_send(const char *buffer, size_t len) {
    if (g_udp_server_fd < 0 || !g_udp_has_client || len == 0) {
        printf("无定点客户端，发送失败\n");
        return -1;
    }

    // ===================== 定点发送：只发给你的电脑 =====================
    sendto(g_udp_server_fd,
           buffer,
           len,
           0,
           (struct sockaddr *)&g_udp_client_addr,
           g_udp_client_len);

    printf("UDP 定点发送成功 -> %s:%d\n",
           inet_ntoa(g_udp_client_addr.sin_addr),
           ntohs(g_udp_client_addr.sin_port));
    return 0;
}

/**
 * @brief  停止服务
 */
void sle_stop_udp_server() {
    //if (g_udp_server_fd >= 0) {
        //close(g_udp_server_fd);
        //g_udp_server_fd = -1;
    //}
    g_udp_has_client = 0;
    printf("sle stop udp_server\n");

	g_udp_running = 0;
    shutdown(g_udp_server_fd, SHUT_RD);
}

void udp_init(void)
{
	pthread_t create_udp_server_thread;
	g_udp_running = 1;
    if(pthread_create(&create_udp_server_thread, NULL, create_udp_server, NULL) != 0) 
    {
	    perror("pthread_create failed");
	    exit(EXIT_FAILURE);
	}	
	
	pthread_detach(create_udp_server_thread);
}


