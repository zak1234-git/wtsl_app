//由node service替换，此代码弃用
#if 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <pthread.h>
#include "wtsl_core_api.h"
#include "wtsl_core_slb_interface.h"
#include "wtsl_log_manager.h"
#include <wtsl_core_node_manager.h>

#define PORT 8998                  // 监听端口
#define BUFFER_SIZE 2048           // 缓冲区大小
#define HEARTBEAT_TIMEOUT 10       // 心跳超时时间(秒)
#define MAX_CLIENTS 32             // 最大客户端数量,1G/32T
#define G_T_CONNECTD_IP "192.168.100.1"
#define SERVER_IP G_T_CONNECTD_IP
#define SERVER_PORT PORT
#define HEARTBEAT_INTERVAL 3       // 心跳发送间隔(秒)
#define ACK_TIMEOUT 5              // 心跳回复超时时间(秒)

Client clients[MAX_CLIENTS];       // 客户端列表
int client_count = 0;              // 客户端数量


extern WTSLNodeInfo g_server_node_info;
extern SPLINK_INFO global_node_info;
extern char g_is_run;
// 查找客户端索引
int find_client_index(const char* id) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

// 添加新客户端
void add_client(const char* id, struct sockaddr_in* addr) {
    if (client_count < MAX_CLIENTS) {
        strcpy(clients[client_count].id, id);
        clients[client_count].addr = *addr;
        clients[client_count].last_heartbeat = time(NULL);
        clients[client_count].is_connected = 1;
        client_count++;
        WTSL_LOG_INFO("SYSTEM", "new client: %s (%s:%d)",id, inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    } else {
        WTSL_LOG_INFO("SYSTEM", "client number is full,now just support 32T");
    }
}

// 检查客户端心跳超时
void check_timeout() {
    time_t now = time(NULL);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].is_connected && (now - clients[i].last_heartbeat) > HEARTBEAT_TIMEOUT) {
            clients[i].is_connected = 0;
            WTSL_LOG_INFO("SYSTEM", "client %s timeout", clients[i].id);
        }
    }
}


typedef struct {
    char is_dhcpd_inited:1;
    char is_dhcpc_inited:1;
    char is_heartbeat_server_inited:1;
    char is_heartbeat_client_inited:1;
    char gnode_business_runing:1;
    char tnode_business_runing:1;
}FLAG_BIT;

static FLAG_BIT flag = {0};



int create_heartbeat_server(void * args){
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    struct timeval tv;
    int opt = 1;
    unsigned int reply_idx = 0;
    
    SPLINK_INFO *info = args;
    fprintf(stderr,"[%s][%d] \n",__FUNCTION__,__LINE__);

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket create failed");
        return -1;
    }

    // 新增：允许端口复用
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return -1;
    }
    // 初始化服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(G_T_CONNECTD_IP);
    server_addr.sin_port = htons(PORT);

    // 绑定端口
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        // close(sockfd);
        // return -1;
    }

    WTSL_LOG_INFO("SYSTEM", "server started,port: %d...", PORT);
    WTSL_LOG_INFO("SYSTEM", "heartbeart timeout: %d second", HEARTBEAT_TIMEOUT);

    while (g_is_run) {
        if(info->user_num <= 0){
            sleep(1);
            continue;
        }
        // 设置超时检测（1秒轮询一次）
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // 等待数据到来或超时
        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("select error");
            break;
        }

        // 有数据到来
        if (FD_ISSET(sockfd, &read_fds)) {
            pthread_mutex_lock(&global_node_info.recv_mutex);

            memset(buffer, 0, BUFFER_SIZE);
            ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
            if (n < 0) {
                perror("recv data failed");
                pthread_mutex_unlock(&global_node_info.recv_mutex);
                continue;
            }

            char client_id[20];
            time_t timestamp;
            if (sscanf(buffer, "%[^:]:%lld", client_id, &timestamp) != 2) {
                WTSL_LOG_INFO("SYSTEM", "warning recv heartbeat data: %s", buffer);
                // continue;
            }
            SPLINK_INFO *pinfo = (SPLINK_INFO *)(buffer + strlen(buffer) + 1);
            // fprintf(stderr,"[%s][%d] user num:%d, bw:%d, tfc_bw:%d\n",__FUNCTION__,__LINE__,pinfo->user_num,pinfo->bw,pinfo->tfc_bw);
            // 更新客户端状态
            int idx = find_client_index(client_id);
            if (idx >= 0) {
                clients[idx].last_heartbeat = time(NULL);
                clients[idx].is_connected = 1;
                memcpy(&clients[idx].cliInfo,pinfo,sizeof(SPLINK_INFO));
                // WTSL_LOG_INFO("SYSTEM", "recv client idx:%d %s heartbeat (time: %lld);rssi:%d", idx,client_id, timestamp,pinfo->node_info.rssi);
            } else {
                WTSL_LOG_DEBUG("SYSTEM", "add client %s,type:%d,essid:%s,mac:%s", client_id,pinfo->node_info.type,pinfo->node_info.name,pinfo->node_info.mac);
                add_client(client_id, &client_addr);
                WTSLNode *node = wtsl_core_new_node();
                node->id = wtsl_core_get_node_list()->list_nodes->size;
                node->type = pinfo->node_type == SP_GNODE ? NODE_TYPE_SLB_G : NODE_TYPE_SLB_T;
                memcpy(&node->info,&pinfo->node_info,sizeof(WTSLNodeInfo));
                memcpy(&clients[node->id].cliInfo,pinfo,sizeof(SPLINK_INFO));
                WTSL_LOG_DEBUG("SYSTEM", "add node info: ip:%s,name:%s,mac:%s",pinfo->node_info.ip,pinfo->node_info.name,pinfo->node_info.mac);
                wtsl_core_add_node(wtsl_core_get_node_list(), node);
            }
            pthread_mutex_unlock(&global_node_info.recv_mutex);
            
            pthread_mutex_lock(&global_node_info.mutex);
            // 回复心跳确认
            char reply[BUFFER_SIZE];
            snprintf(reply, BUFFER_SIZE, "ACK %d:%s", reply_idx,client_id);
            reply[strlen(reply)]='\0';
            memcpy(&reply[strlen(reply)+1],&global_node_info,sizeof(SPLINK_INFO));
            int send_total_len = strlen(reply)+1+sizeof(SPLINK_INFO);
            
            // fprintf(stderr,"reply strlen:%d,send_total_len:%d,baseaddr:%x\n",strlen(reply),send_total_len,reply[strlen(reply)+1]);
            sendto(sockfd, reply, send_total_len, 0, (struct sockaddr*)&client_addr, addr_len);

            pthread_mutex_unlock(&global_node_info.mutex);
        }

        // 检查超时客户端
        check_timeout();
    }
    close(sockfd);
    return 0;
}

int create_heartbeat_client(void *args){
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    struct timeval tv;
    
    
    SPLINK_INFO *info = args;
    info->node_type = SP_TNODE;
    fprintf(stderr,"[%s][%d] \n",__FUNCTION__,__LINE__);
    char *client_id = global_node_info.node_info.name;
    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket create failed");
        return -1;
    }

    // 初始化服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid server ip addr");
        close(sockfd);
        return -1;
    }

    WTSL_LOG_INFO("SYSTEM", "client %s start,serverip: %s:%d", client_id, SERVER_IP, SERVER_PORT);
    WTSL_LOG_INFO("SYSTEM", "heartbeat interval: %d second,timeout: %d second", HEARTBEAT_INTERVAL, ACK_TIMEOUT);

    while (g_is_run) {
        if(info->user_num <= 0){
            sleep(1);
            continue;
        }
        // 构建心跳包（格式: "CLIENT_ID:时间戳"）
        time_t now = time(NULL);
        snprintf(buffer, BUFFER_SIZE, "%s:%lld", client_id, now);
        int buflen = strlen(buffer);
        buffer[buflen] = '\0';
        memcpy(buffer + buflen + 1, info,sizeof(SPLINK_INFO));
        
        int total_len = strlen(buffer)+sizeof(SPLINK_INFO)+1;
        // fprintf(stderr,"[%s][%d] ################# total_len:%d ######################\n",__FUNCTION__,__LINE__,total_len);
        // WTSL_LOG_INFO("SYSTEM", "\nsend heartbeat: [%s],rssi:%d,bw:%d,tfc_bw:%d,total_len:%d", buffer,info->rssi,info->bw,info->tfc_bw,strlen(buffer)+sizeof(SPLINK_INFO)+1);

        // 发送心跳包到服务器
        if (sendto(sockfd, buffer, total_len, 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
            perror("send heartbeat failed");
            sleep(HEARTBEAT_INTERVAL);
            continue;
        }

        // 等待服务器回复（设置超时）
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        tv.tv_sec = ACK_TIMEOUT;
        tv.tv_usec = 0;

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("select error");
        } else if (activity == 0) {
            // 超时未收到回复
            WTSL_LOG_INFO("SYSTEM", "no server reply");
        } else {
            // 收到回复
            if (FD_ISSET(sockfd, &read_fds)) {
                
                memset(buffer, 0, BUFFER_SIZE);
                ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &addr_len);
                if (n < 0) {
                    perror("recvfrom failed");
                } else {
                    SPLINK_INFO *psi = (SPLINK_INFO *)&buffer[strlen(buffer)+1];
                    
                    // WTSL_LOG_INFO("SYSTEM", "recv num:%d, data addr:%p,data:%s,strlen(buffer):%d,baseaddr:%x",n,buffer,buffer,strlen(buffer),buffer[strlen(buffer)+1]);
                    if(psi != NULL){
                        // WTSL_LOG_INFO("SYSTEM", "recv name:%s,ip:%s",psi->node_info.name,psi->node_info.ip);
                        pthread_mutex_lock(&global_node_info.mutex);
                        memcpy(&g_server_node_info,&psi->node_info,sizeof(psi->node_info));
                        pthread_mutex_unlock(&global_node_info.mutex);
                    }else{
                        WTSL_LOG_WARNING("SYSTEM", "psi is null\n");
                    }

                }
            }
        }
        // 等待下一次心跳间隔
        sleep(HEARTBEAT_INTERVAL);
    }
    close(sockfd);
    return 0;
}

static void init_dhcpd_server(){
    if(flag.is_dhcpd_inited == 0){
        fprintf(stderr,"[%s][%d] initing\n",__FUNCTION__,__LINE__);
        system("ifconfig br0:1 192.168.100.1");
        system("ifconfig br0:2 192.168.98.1");
        system("udhcpd /etc/udhcpd.conf");
        flag.is_dhcpd_inited = 1;
    }else{
        fprintf(stderr,"[%s][%d] dhcpd already inited",__FUNCTION__,__LINE__);
    }
}

static void init_dhcpc_client(){
    if(flag.is_dhcpc_inited == 0){
        fprintf(stderr,"[%s][%d] initing\n",__FUNCTION__,__LINE__);
        system("ifconfig br0:1 192.168.100.9");
        system("ifconfig br0:2 192.168.98.1");
        system("udhcpc -i br0:1 -s /etc/udhcpc_script -q");
        flag.is_dhcpc_inited = 1;
    }else{
        fprintf(stderr,"[%s][%d] dhcpc already inited\n",__FUNCTION__,__LINE__);
    }
}
int wtsl_core_gnode_business(SPLINK_INFO *info){
    //GNode 开启dhcp分配IP服务
    WTSL_LOG_INFO("SYSTEM", "[%s][%d] In",__FUNCTION__,__LINE__);
    init_dhcpd_server();
    create_heartbeat_server(info);
    WTSL_LOG_INFO("SYSTEM", "[%s][%d] Out",__FUNCTION__,__LINE__);
    return 0;
}

int wtsl_core_tnode_business(SPLINK_INFO *info){
    WTSL_LOG_INFO("SYSTEM", "[%s][%d] In",__FUNCTION__,__LINE__);
    //TNode 获取dhcp分配IP
    init_dhcpc_client();
    create_heartbeat_client(info);
    WTSL_LOG_INFO("SYSTEM", "[%s][%d] Out",__FUNCTION__,__LINE__);
    return 0;
}


#endif