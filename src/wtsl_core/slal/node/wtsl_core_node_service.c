#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include "cJSON.h"  // 包含cJSON头文件
#include "wtsl_core_node_manager.h"
#include "wtsl_core_api.h"
#include "wtsl_log_manager.h"

#include "wtsl_core_node_list.h"
#include "wtsl_core_net_utils.h"

// 配置参数
#define BUF_SIZE 2048                  // 缓冲区大小
#define HEARTBEAT_INTERVAL 1           // 心跳发送间隔（秒）
#define HEARTBEAT_TIMEOUT 60           // 心跳超时时间（秒）
#define MAX_NODES 32                   // 最大节点数（可通信的其他节点）

const char *udhcpd_config=
"interface "NET_BRIDGE_NAME":1\n"
"start 192.168.100.10\n"
"end 192.168.100.200\n"
"netmask 255.255.255.0\n"
"router 192.168.100.1\n"
"lease_file /tmp/udhcpd_leases.txt\n"
"lease 86400\n"
"max_lease 604800\n";

const char *udhcpc_script=
"#!/bin/sh\n"
"INTERFACE="NET_BRIDGE_NAME":1\n"
"echo $1\n"
"case \"$1\" in\n"
"	bound|renew)\n"
"		ifconfig $INTERFACE $ip\n"
"		# route add default gw $router dev $INTERFACE\n"
"		;;\n"
"	deconfig)\n"
"		ifconfig $INTERFACE 0.0.0.0\n"
"		;;\n"
"esac\n";



static unsigned short node_service_port = 8081;
extern char g_is_run;
extern SPLINK_INFO global_node_info;

extern char *get_ipstr_by_ifname(char *ifname,char *ipstr);  

#define MODULE_NAME "node_service"

// 消息类型枚举
typedef enum {
    MSG_HEARTBEAT_REQ = 0,  // 心跳请求
    MSG_HEARTBEAT_RSP,      // 心跳响应
    MSG_COMMAND,            // 业务命令
    MSG_RESPONSE,           // 命令响应
    MSG_UNKNOWN             // 未知类型
} MsgType;

typedef struct {
    char TAG[8];
    MsgType type;
    time_t time;
    int reserv;
} NodeDataHeader;


// 全局共享资源
int udp_socket;              // UDP socket
pthread_mutex_t node_mutex;  // 节点列表互斥锁

// 函数声明
void *recv_thread(void *arg);        // 接收消息线程
void *heartbeat_thread(void *arg);   // 心跳检测线程
void update_heartbeat(const struct sockaddr_in *addr,WTSLNodeInfo *pni);  // 更新节点心跳时间
int is_node_not_exist(SPLINK_INFO *pinfo);  // 检查节点是否存在
char *get_time_str(void);  // 获取当前时间字符串
int send_data_to_addr(struct sockaddr_in *to_addr,MsgType type,void *data,unsigned int len);





void wtsl_core_start_dhcpd_server(){
    char cmd1[64];
    snprintf(cmd1, sizeof(cmd1), "ifconfig %s:1 192.168.100.1", NET_BRIDGE_NAME);
    int ret = system(cmd1);

    char cmd2[64];
    snprintf(cmd2, sizeof(cmd2), "ifconfig %s:2 192.168.98.1", NET_BRIDGE_NAME);
    ret = system(cmd2);

	const char* temp_conf_file = "/tmp/udhcpd.conf";
	FILE* fp = fopen(temp_conf_file, "w");
	if(fp == NULL)
	{
		perror("打开udhcpd配置文件失败");
		return;
	}
	fprintf(fp, "%s", udhcpd_config);
	fclose(fp);
	
	WTSL_LOG_INFO(MODULE_NAME, "配置文件%s已生成\n", udhcpd_config);
	
	ret |= system("chmod +x /tmp/udhcpd.conf");

	if(global_node_info.node_info.basic_info.dhcp_enable)
    	ret |= system("udhcpd /tmp/udhcpd.conf");
    
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
    }
}

void wtsl_core_start_dhcpc_client(){
    char cmd1[64];
    snprintf(cmd1, sizeof(cmd1), "ifconfig %s:1 192.168.100.9", NET_BRIDGE_NAME);
    int ret = system(cmd1);

    char cmd2[64];
    snprintf(cmd2, sizeof(cmd2), "ifconfig %s:2 192.168.98.1", NET_BRIDGE_NAME);
    ret |= system(cmd2);

	const char* temp_script_file = "/tmp/udhcpc_script";
	FILE* fp = fopen(temp_script_file, "w");
	if(fp == NULL)
	{
		perror("打开udhcpc脚本文件失败");
		return;
	}
	fprintf(fp, "%s", udhcpc_script);
	fclose(fp);
	
	WTSL_LOG_INFO(MODULE_NAME, "脚本文件%s已生成\n", udhcpc_script);
	
	ret |= system("chmod +x /tmp/udhcpc_script");

	if(global_node_info.node_info.basic_info.dhcp_enable)
	{
		char cmd3[64];
    	snprintf(cmd3, sizeof(cmd3), "udhcpc -i %s:1 -s /tmp/udhcpc_script -q", NET_BRIDGE_NAME);
    	ret |= system(cmd3);
	}

    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
    }
}



int send_data_to_addr(struct sockaddr_in *to_addr,MsgType type,void *data,unsigned int len){
    int total_len = 0;
    char send_buf[2048]={0};
    NodeDataHeader header = {0};
    strcpy(header.TAG,"WTSL");
    header.type = type;
    header.time = time(NULL);
    memcpy(send_buf,&header,sizeof(NodeDataHeader));
    if(len >= (2048 - sizeof(NodeDataHeader)-1)){return -1;}
    memcpy(&send_buf[sizeof(NodeDataHeader)+1],data,len);
    total_len = sizeof(NodeDataHeader)+1+len;
    int send_len = sendto(udp_socket, send_buf, total_len, 0,(struct sockaddr *)to_addr, sizeof(*to_addr));
    if (send_len < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "send heartbeat error");
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "[%s] To %s:%d send message: %s",get_time_str(),inet_ntoa(to_addr->sin_addr),ntohs(to_addr->sin_port),send_buf);
    }
    return 0;
}


int send_heartbeat_req(struct sockaddr_in *to_addr,void *data,unsigned int size){
    if (to_addr == NULL || data == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "send msg error param is null");
        return -1;
    }
    return send_data_to_addr(to_addr,MSG_HEARTBEAT_REQ,data,size);
}

int send_heartbeat_rsp(struct sockaddr_in *to_addr,void *data,unsigned int size){
    if (to_addr == NULL || data == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "send msg error param is null");
        return -1;
    }
    return send_data_to_addr(to_addr,MSG_HEARTBEAT_RSP,data,size);
}


int send_command(struct sockaddr_in *to_addr,void *data,unsigned int size){
    if (to_addr == NULL || data == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "send msg error param is null");
        return -1;
    }
    return send_data_to_addr(to_addr,MSG_COMMAND,data,size);
}


int wtsl_core_splink_start_node_service_thread(void *args) {
    
    char ipstr[32]={0};
    struct sockaddr_in local_addr;
    pthread_t recv_tid, heartbeat_tid;
    // pthread_t input_tid;
    int ret;

    // SPLINK_INFO *pInfo = (SPLINK_INFO *)args;
    // 1. 创建UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "socket create failed");
        return -1;
    }

    // 2. 绑定本地端口（作为服务端监听）
    char ifname_br[64];
    snprintf(ifname_br, sizeof(ifname_br), "%s:1", NET_BRIDGE_NAME);
    char *ipval = get_ipstr_by_ifname(ifname_br,ipstr);
    WTSL_LOG_INFO(MODULE_NAME, "ipval:%s,ipstr:%s",ipval,ipstr);
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    // local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听所有网卡
    local_addr.sin_addr.s_addr = inet_addr(ipstr);

    local_addr.sin_port = htons(node_service_port);

    if (bind(udp_socket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "bind failed");
        close(udp_socket);
        return -1;
    }

    WTSLNodeList *list = get_wtsl_core_node_list();
     WTSLNode *cur_wtsl_node = find_wtsl_node_by_id(list,0);
    cur_wtsl_node->addr = local_addr;

    // 3. 初始化互斥锁
    pthread_mutex_init(&node_mutex, NULL);

    // 4. 创建接收数据线程
    WTSL_LOG_INFO(MODULE_NAME, "\n#########recv thread###########");
    ret = pthread_create(&recv_tid, NULL, recv_thread, args);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "create recv thread error");
        return -1;
    }

    WTSL_LOG_INFO(MODULE_NAME, "\n#########  heart thread ###########");
    ret = pthread_create(&heartbeat_tid, NULL, heartbeat_thread, args);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "create heartbeat thread failed");
        return -1;
    }
    while (g_is_run) {
        sleep(1);
    }

    // 5. 资源清理
    WTSL_LOG_INFO(MODULE_NAME, "\nProgram exit in progress...");
    pthread_cancel(recv_tid);
    pthread_cancel(heartbeat_tid);

    pthread_join(recv_tid, NULL);
    pthread_join(heartbeat_tid, NULL);

    pthread_mutex_destroy(&node_mutex);
    close(udp_socket);
    return 0;
}


int parse_data_heartbeat(const struct sockaddr_in *addr,char *data,unsigned int len){
    (void)len;
    pthread_mutex_lock(&node_mutex);
    SPLINK_INFO *pinfo = (SPLINK_INFO *)data;
    if(pinfo == NULL){
        WTSL_LOG_INFO(MODULE_NAME, "pinfo is null");
        pthread_mutex_unlock(&node_mutex);
        return -1;
    }

    WTSLNodeList *pList = get_wtsl_core_node_list();
    if (is_node_not_exist(pinfo)) {
        //节点不存在则添加节点
        WTSL_LOG_DEBUG(MODULE_NAME,"node is not exist, so add node");
        WTSLNode *pNode =  create_new_wtsl_node();
        pNode->id = pList->node_count;
        pNode->active = 1;
        pNode->last_heartbeat = time(NULL);
        memcpy(&pNode->addr,addr,sizeof(struct sockaddr_in));
        memcpy(&pNode->info,&pinfo->node_info,sizeof(WTSLNodeInfo));
        int ret = add_wtsl_node_to_list_tail(pList,pNode);
        WTSL_LOG_INFO(MODULE_NAME,"ADD Node[%d]:0x%x, info:(name:%s,mac:%s,ip:%s) ret:%d",pNode->id,pNode,pNode->info.basic_info.name,pNode->info.basic_info.mac,pNode->info.basic_info.ip,ret);
    }else{
        //节点存在则更新心跳状态
        WTSL_LOG_DEBUG(MODULE_NAME,"node is exist, so update node info");
        update_heartbeat(addr,&pinfo->node_info);
    }
    
    pthread_mutex_unlock(&node_mutex);
    
    return 0;
}

static int parse_data(const struct sockaddr_in *addr,char *data,unsigned int len){
    int ret = -1;
    char *pHB = NULL;
    NodeDataHeader header;
    memcpy(&header,data,sizeof(NodeDataHeader));
    WTSL_LOG_INFO(MODULE_NAME, "TAG:%s,type:%d,time:%lld\n",header.TAG,header.type,(long long)header.time);

    switch(header.type){
        case MSG_HEARTBEAT_REQ:
            WTSL_LOG_DEBUG(MODULE_NAME,"HEARTBEAT REQUEST");
            pHB = &data[sizeof(NodeDataHeader)+1];
            ret = parse_data_heartbeat(addr,pHB,len);
            if(ret !=0 ){
                WTSL_LOG_ERROR(MODULE_NAME,"parse heartbeat data error");
            }
            send_heartbeat_rsp((struct sockaddr_in *)addr,&global_node_info,sizeof(global_node_info));
        break;
        case MSG_HEARTBEAT_RSP:
            WTSL_LOG_DEBUG(MODULE_NAME,"HEARTBEAT RESPONSE");
            pHB = &data[sizeof(NodeDataHeader)+1];
            ret = parse_data_heartbeat(addr,pHB,len);
            if(ret !=0 ){
                WTSL_LOG_ERROR(MODULE_NAME,"parse heartbeat data error");
            }
            WTSL_LOG_DEBUG(MODULE_NAME,"HEARTBEAT RESPONSE OVER");
        break;
        case MSG_COMMAND:
            WTSL_LOG_DEBUG(MODULE_NAME,"REMOTE COMMAND EXECUTE");

        break;
        case MSG_RESPONSE:
            WTSL_LOG_DEBUG(MODULE_NAME,"REMOTE COMMAND RESPONSE");
        break;
        default:
            WTSL_LOG_ERROR(MODULE_NAME,"no support type:%d",header.type);
        break;
    }
    return 0;
}
/**
 * 接收线程：监听本地端口，处理收到的消息
 */
void *recv_thread(void *arg) {
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    char buf[BUF_SIZE];
    int recv_len;

    (void)arg;
    while (g_is_run) {
        memset(buf, 0, BUF_SIZE);
        memset(&from_addr, 0, sizeof(from_addr));

        // 设置接收超时
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // 接收数据
        recv_len = recvfrom(udp_socket, buf, BUF_SIZE - 1, 0,(struct sockaddr *)&from_addr, &from_len);
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // 超时，继续等待
            }
            WTSL_LOG_ERROR(MODULE_NAME, "####  recvfrom error ####");
            break;
        }
        // 打印接收日志
        WTSL_LOG_INFO(MODULE_NAME, "\n[%s] len:%d,recvfrom %s:%d message: %s",get_time_str(),recv_len,inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),buf);
        parse_data(&from_addr,buf,recv_len);

    }
    return NULL;
}

/**
 * 心跳线程：T节点定期向G节点发送心跳
 */
void *heartbeat_thread(void *arg) {
    char ip[32]={0};
    char *gwip = NULL;
    struct sockaddr_in to_addr;
    SPLINK_INFO *pInfo = (SPLINK_INFO *)arg;
    // WTSL_Core_ListNodes *list = wtsl_core_get_node_list();
    while (g_is_run) {
        if(pInfo == NULL)continue;

        if(pInfo->node_info.basic_info.type != NODE_TYPE_SLB_G){
            // 构造目标地址
            char ifname_br[64];
            snprintf(ifname_br, sizeof(ifname_br), "%s:1", NET_BRIDGE_NAME);
            get_ipstr_by_ifname(ifname_br,ip);
            gwip = get_default_gw_ipstr_by_curip(ip);
            memset(&to_addr, 0, sizeof(to_addr));
            to_addr.sin_family = AF_INET;
            to_addr.sin_port = htons(node_service_port);
            if (inet_pton(AF_INET, gwip, &to_addr.sin_addr) <= 0) {
                WTSL_LOG_ERROR(MODULE_NAME,"invalid ip:%s", ip);
                continue;
            }
        }else{
            sleep(1);
            continue;
        }

        pthread_mutex_lock(&global_node_info.mutex);
        send_heartbeat_req(&to_addr,&global_node_info,sizeof(global_node_info));
        pthread_mutex_unlock(&global_node_info.mutex);
        WTSL_LOG_INFO(MODULE_NAME, "[%s] to %s:%d heartbeat at time:%lld",get_time_str(),gwip,node_service_port,(long long)time(NULL));
    
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}

/**
 * 更新节点心跳时间
 */
void update_heartbeat(const struct sockaddr_in *addr,WTSLNodeInfo *pni) {
    WTSLNodeList *pList = get_wtsl_core_node_list();
    if(pList == NULL)return;
    for (int i = 0; i < pList->node_count; i++) {
        WTSLNode *pNode = find_wtsl_node_by_id(pList,i);
        if(pNode == NULL)continue;
        if (memcmp(&pNode->addr, addr, sizeof(struct sockaddr_in)) == 0) {
            pNode->last_heartbeat = time(NULL);
            pNode->active = 1;
            memcpy(&pNode->info,pni,sizeof(WTSLNodeInfo));
            WTSL_LOG_INFO(MODULE_NAME, "node: %s:%d update heartbeat",inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));
            break;
        }
    }
}

/**
 * 检查节点是否存在
 */
int is_node_not_exist(SPLINK_INFO *pinfo) {
    WTSL_LOG_DEBUG(MODULE_NAME, "is node exist in");
    WTSLNodeList *pList = get_wtsl_core_node_list();
    if(pList == NULL){
        WTSL_LOG_DEBUG(MODULE_NAME, "is node exist in, null error");
        return -1;
    }
    for (int i = 0; i < pList->node_count; i++) {
        WTSL_LOG_INFO(MODULE_NAME, "node[%d],listsize:%d",i,pList->node_count);
        WTSLNode *psl_node = find_wtsl_node_by_id(pList,i);
        if(psl_node == NULL){
            WTSL_LOG_ERROR(MODULE_NAME,"####[%s][%d]##### node[%d] is null",__FUNCTION__,__LINE__,i);
            continue;
        }
       WTSL_LOG_INFO(MODULE_NAME,"######### node[%d]: (name:%s,mac:%s,ip:%s) ~ (name:%s,mac:%s,ip:%s)#########",i,psl_node->info.basic_info.name,psl_node->info.basic_info.mac,psl_node->info.basic_info.ip,pinfo->node_info.basic_info.name,pinfo->node_info.basic_info.mac,pinfo->node_info.basic_info.ip); 

        // if (memcmp(&psl_node->addr, addr, sizeof(struct sockaddr_in)) == 0) {
        //     WTSL_LOG_DEBUG(MODULE_NAME,"########## match sockaddr $$$$$$$$$$$$$");
        //     return 0;
        // }else if(memcmp(&psl_node->info.basic_info.mac,pinfo->node_info.basic_info.mac,sizeof(pinfo->node_info.basic_info.mac)) == 0){
        //     WTSL_LOG_DEBUG(MODULE_NAME,"########## match vap mac $$$$$$$$$$$$$");
        //     return 0;
        if(memcmp(&psl_node->info.basic_info.mac,pinfo->node_info.basic_info.mac,sizeof(pinfo->node_info.basic_info.mac)) == 0){
                WTSL_LOG_DEBUG(MODULE_NAME,"########## match vap mac $$$$$$$$$$$$$");
                return 0;    
        }//else if (memcmp(&psl_node->addr, addr, sizeof(struct sockaddr_in)) == 0) {
            //WTSL_LOG_DEBUG(MODULE_NAME,"########## match sockaddr $$$$$$$$$$$$$");
            //return 0;
        //}
        else{
            WTSL_LOG_DEBUG(MODULE_NAME,"can't find the addr at idx:%d",i);
        }
    }
    return -1;
}



void send_node_data(WTSLNode *p_src_node,WTSLNode *p_dst_node, MsgType type, void *content){

    WTSL_LOG_DEBUG(MODULE_NAME,"frome (id:%d,name:%s) to (id:%d,name:%s),type:%d,content:%p",p_src_node->id,p_src_node->info.basic_info.name,p_dst_node->id,p_dst_node->info.basic_info.name,type,content);
}

/**
 * 获取当前时间字符串（格式：YYYY-MM-DD HH:MM:SS）
 */
char *get_time_str(void) {
    static char time_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    return time_str;
}