#ifndef _WTSL_COREAPI_H
#define _WTSL_COREAPI_H

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include "wtsl_core_slb_interface.h"
#include "version.h"
#include <wtsl_core_list.h>
#include "wtsl_core_node_manager.h"

/*
	config
*/
#define HTTP_RESTFUL_API_SUPPORT 1

typedef enum{
    SPLINK_UNKNOWN,
    SPLINK_LINKING,
    SPLINK_LINKED,
    SPLINK_DISCONNECTING,
    SPLINK_DISCONNECTED
}E_SPLINK_STATE;

typedef enum{
    SP_GNODE,
    SP_TNODE,
}E_SP_NODE_TYPE;


typedef struct _SPLINK_INFO{

	WTSLNodeInfo node_info;
	
    E_SP_NODE_TYPE node_type;
    E_SPLINK_STATE link_state;
    int user_num;
    int uid;
    char mac[18];
    char need_refresh;
    pthread_mutex_t mutex;
    pthread_mutex_t recv_mutex;

    
}SPLINK_INFO;

// 客户端信息结构体
typedef struct {
    char id[32];                   // 客户端ID
    struct sockaddr_in addr;       // 客户端地址
    time_t last_heartbeat;         // 最后一次心跳时间
    int is_connected;              // 连接状态(1:在线,0:离线)
    SPLINK_INFO cliInfo;
} Client,*pClient;

extern SPLINK_INFO global_node_info;

int wtsl_log_manager(int level);

int wtsl_core_get_splink_info(SPLINK_INFO *info);

int wtsl_core_tnode_business(SPLINK_INFO *info);

int wtsl_core_gnode_business(SPLINK_INFO *info);
/*
	
	commitidstr: 8字节字符串数组，数组长度大于9
*/
int wtsl_core_get_version(int *vercode,char *commitidstr);

#ifdef HTTP_RESTFUL_API_SUPPORT	
	int wtsl_http_main(void);
#endif

#ifdef MQTT_API_SUPPORT	
	int wtsl_mqtt_main(void);
#endif



#endif