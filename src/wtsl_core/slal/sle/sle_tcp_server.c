#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 9981
#define BUFFER_SIZE 200

extern uint8_t g_running;
static int g_server_fd = -1;
static int g_client_socket = -1;
extern void sle_client_senddata(uint8_t *data, uint16_t data_len);

// 线程函数：接收客户端消息
void *recv_msg(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    printf("recv msg client_socket:%d,g_client_socket: %d\n",client_socket,g_client_socket);
    while (g_running) {
        // 清空缓冲区
        memset(buffer, 0, BUFFER_SIZE);
        // 接收客户端消息
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read < 0) perror("recv failed");
            printf("客户端已断开连接\n");
            break;
        }
        printf("client recv bytes:%d,buffer:%s\n",bytes_read, buffer);
        sle_client_senddata((uint8_t*)buffer, strlen(buffer));
    }
    close(client_socket);
    pthread_exit(NULL);
}

int create_tcp_server() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t recv_thread;

    // 创建TCP套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    g_server_fd = server_fd;
    // 设置服务器地址和端口
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
    address.sin_port = htons(PORT);        // 转换为网络字节序

    // 绑定套接字到指定端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 开始监听连接（最大等待队列长度为5）
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("服务器启动，监听端口 %d...\n", PORT);
    printf("等待客户端连接...\n");

    while(g_running){
        // 接受客户端连接
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        g_client_socket = client_socket;
        printf("客户端 %s:%d 已连接\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        printf("开始聊天（输入exit退出）...\n");

        // 创建线程用于接收客户端消息
        if (pthread_create(&recv_thread, NULL, recv_msg, &client_socket) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }

        // // 主线程用于发送消息给客户端
        // char buffer[BUFFER_SIZE];
        // while (1) {
        //     // 清空缓冲区并读取输入
        //     memset(buffer, 0, BUFFER_SIZE);
        //     if (fgets(buffer, BUFFER_SIZE - 1, stdin) == NULL) {
        //         perror("fgets failed");
        //         break;
        //     }
        //     // 发送消息到客户端
        //     if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
        //         perror("send failed");
        //         break;
        //     }
        //     // 如果输入"exit"，则关闭连接
        //     if (strncmp(buffer, "exit", 4) == 0) {
        //         printf("服务器请求断开连接\n");
        //         break;
        //     }
        // }

        // 等待接收线程结束
        pthread_join(recv_thread, NULL);
    }
    close(server_fd);
    printf("服务器已关闭\n");

    return 0;
}

int sle_tcp_server_send(const char *buffer, size_t len){
    printf("[%s][%d],buffer:%s,len:%d\n",__FUNCTION__,__LINE__,buffer,len);
    send(g_client_socket, buffer, len, 0);
    // fflush();
    return 0;
}

void sle_stop_tcp_server(){
    close(g_server_fd);
    close(g_client_socket);
    printf("sle stop tcp_server\n");
}