#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <time.h>
#include <wtsl_core_api.h>
#include "wtsl_log_manager.h"

extern char g_is_run;

extern SPLINK_INFO global_node_info;

#define MODULE_NAME "splink_mc"
// 组播地址和端口
#define MULTICAST_GROUP "239.0.0.1"
#define MULTICAST_PORT  9988

//#define INTERFACE "br0"            // 目标接口


// 心跳发送间隔(秒)
#define HEARTBEAT_INTERVAL 60
// 最大节点超时时间(秒)
#define NODE_TIMEOUT 15

// 节点信息结构体
typedef struct {
    char ip[INET_ADDRSTRLEN];
    time_t last_heartbeat;
} MCNodeInfo;

// 全局变量
int running = 1;
MCNodeInfo mc_nodes[1024];
int mc_node_count = 0;


// 初始化组播发送套接字
int init_send_socket() {
    int sockfd;
    struct sockaddr_in multicast_addr;

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置组播地址和端口
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);
    multicast_addr.sin_port = htons(MULTICAST_PORT);

    // 绑定到任意地址(发送端可以不绑定)
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0);  // 随机端口

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// 初始化组播接收套接字
int init_recv_socket() {
    int sockfd;
    struct sockaddr_in local_addr;
    struct ip_mreq mreq;
    struct ifreq ifr;  // 用于获取接口索引

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 允许端口复用，多个程序可以绑定到同一端口
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "setsockopt SO_REUSEADDR failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // struct in_addr in_addr;
	// in_addr.s_addr = global_node_info.node_info.ip;
	// 转换为字符串
	char *ip_str = global_node_info.node_info.basic_info.ip;// inet_ntoa(in_addr);

    // 绑定到组播端口
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(ip_str);    // htonl(INADDR_ANY);
    local_addr.sin_port = htons(MULTICAST_PORT);

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 4. 获取br0的接口索引（关键：通过接口名强制定位）
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, NET_BRIDGE_NAME, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "get io error");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    int if_index = ifr.ifr_ifindex;  // 接口索引

    // 5. 通过索引绑定组播接口
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &if_index, sizeof(if_index)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "IP_MULTICAST_IF failed");
        // close(sockfd);
        // exit(EXIT_FAILURE);
    }
    // 加入组播组
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);

    // mreq.imr_interface.s_addr = htonl(INADDR_ANY);  // 任意网络接口
    // mreq.imr_interface.s_addr = inet_addr(ip_str);

	mreq.imr_interface.s_addr = inet_addr(ip_str);
	
	
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "setsockopt IP_ADD_MEMBERSHIP failed");
        // close(sockfd);
        // exit(EXIT_FAILURE);
    }

    return sockfd;
}

// 发送心跳包
void send_heartbeat(int sockfd) {
    char heartbeat_msg[128];
    struct sockaddr_in multicast_addr;
    time_t now = time(NULL);

    // 构造心跳消息
    snprintf(heartbeat_msg, sizeof(heartbeat_msg),"HEARTBEAT:%lld", (long long)now);

    // 设置组播地址
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);
    multicast_addr.sin_port = htons(MULTICAST_PORT);

    // 发送心跳包
    if (sendto(sockfd, heartbeat_msg, strlen(heartbeat_msg), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        // perror("sendto failed");
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "sendheart message: %s", heartbeat_msg);
    }
}

// 更新节点列表
void update_node_list(const char* ip) {
    time_t now = time(NULL);
    int i;

    // 检查节点是否已存在
    for (i = 0; i < mc_node_count; i++) {
        if (strcmp(mc_nodes[i].ip, ip) == 0) {
            mc_nodes[i].last_heartbeat = now;
            return;
        }
    }

    // 添加新节点
    if (mc_node_count < 1024) {
        strncpy(mc_nodes[mc_node_count].ip, ip, INET_ADDRSTRLEN - 1);
        mc_nodes[mc_node_count].last_heartbeat = now;
        mc_node_count++;
        WTSL_LOG_INFO(MODULE_NAME, "add new node : %s", ip);
    }
}

// 清理超时节点
void clean_timeout_mc_nodes() {
    time_t now = time(NULL);
    int i, j;

    for (i = 0; i < mc_node_count; i++) {
        if (difftime(now, mc_nodes[i].last_heartbeat) > NODE_TIMEOUT) {
            WTSL_LOG_WARNING(MODULE_NAME, "node exit timeout: %s\n", mc_nodes[i].ip);
            
            // 移除节点
            for (j = i; j < mc_node_count - 1; j++) {
                mc_nodes[j] = mc_nodes[j + 1];
            }
            mc_node_count--;
            i--;  // 重新检查当前位置
        }
    }
}

// 打印当前节点列表
void print_node_list() {
    int i;
    time_t now = time(NULL);
    
    WTSL_LOG_INFO(MODULE_NAME, "\ncurrent node list(%d nodes):", mc_node_count);
    for (i = 0; i < mc_node_count; i++) {
        WTSL_LOG_INFO(MODULE_NAME, "IP: %s, last heart: %.0f second",mc_nodes[i].ip, difftime(now, mc_nodes[i].last_heartbeat));
    }
    printf("\n");
}

// 接收心跳包
void recv_heartbeats(int sockfd) {
    char buffer[128];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    ssize_t n;

    // 设置非阻塞模式，避免接收阻塞
    fd_set readfds;
    struct timeval timeout;
    int retval;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // 设置超时时间为1秒
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (retval < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "select failed");
        return;
    } else if (retval == 0) {
        // 超时，没有数据
        return;
    }

    // 读取数据
    n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                 (struct sockaddr*)&sender_addr, &sender_len);
    if (n < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "recvfrom failed");
        return;
    }

    buffer[n] = '\0';
    char* ip_str = inet_ntoa(sender_addr.sin_addr);
    
    // 验证是否为心跳消息
    if (strstr(buffer, "HEARTBEAT:") == buffer) {
        WTSL_LOG_INFO(MODULE_NAME, "recv from %s buffer: %s", ip_str, buffer);
        update_node_list(ip_str);
    }
}

void *wtsl_core_splink_gnode_service(void *args) {
    int send_sock, recv_sock;
    time_t last_send_time = 0;

    (void)args;
    // 初始化套接字
    send_sock = init_send_socket();
    recv_sock = init_recv_socket();

    WTSL_LOG_INFO(MODULE_NAME, "multicast start: %s:%d", MULTICAST_GROUP, MULTICAST_PORT);
    // 主循环
    while (g_is_run) {
        time_t now = time(NULL);

        // 定期发送心跳包
        if (difftime(now, last_send_time) >= HEARTBEAT_INTERVAL) {
            send_heartbeat(send_sock);
            last_send_time = now;
            
            // 清理超时节点并打印节点列表
            clean_timeout_mc_nodes();
            // print_node_list();
        }

        // 接收心跳包
        recv_heartbeats(recv_sock);
    }

    // 清理资源
    close(send_sock);
    
    // 退出组播组
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(recv_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(recv_sock);
    WTSL_LOG_INFO(MODULE_NAME, "multicast heartbeat program exit");

    return 0;
}