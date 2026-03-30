#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "wtsl_log_manager.h"
#include "wtsl_core_api.h"
#include "wtsl_core_node_manager.h"
#include "wtsl_core_node_list.h"

#include "wtsl_core_slb_interface.h"


SPLINK_INFO global_node_info;
extern char g_is_run;

#define MODULE_NAME "node_state"

extern void *wtsl_core_splink_gnode_service(void *args);
pthread_mutex_t auto_join_net_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t auto_join_net_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t auto_join_net_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t auto_join_net_flag_cond = PTHREAD_COND_INITIALIZER;
extern void wtsl_core_start_dhcpc_client();
extern void wtsl_core_start_dhcpd_server();
extern int wtsl_core_splink_start_node_service_thread(void *args);
extern void* wtsl_core_sle(void * args);



void *wtsl_core_splink_node_service(void *args){

    pthread_mutex_lock(&global_node_info.mutex);
    SPLINK_INFO *pInfo = (SPLINK_INFO *)args;
    WTSLNodeList *pListNodes = create_wtsl_node_list();
    WTSLNode *node = create_new_wtsl_node();
    node->id = 0;
    node->type = (pInfo->node_type == SP_GNODE ? NODE_TYPE_SLB_G : NODE_TYPE_SLB_T);
    pInfo->node_info.basic_info.type = node->type;
    memcpy(&node->info,&pInfo->node_info,sizeof(WTSLNodeInfo));
    add_wtsl_node_to_list_tail(pListNodes,node);
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] add node info,pInfo->user_num:%d,node->type:%d,ni_type:%d,interface:%p",__FUNCTION__,__LINE__,pInfo->user_num,node->type, node->info.basic_info.type,node->interface);
    pthread_mutex_unlock(&global_node_info.mutex);

    while(g_is_run){
        if(pInfo->user_num <= 0){
            usleep(500000);
            continue;
        }
        if(pInfo->node_type == SP_GNODE){
            //连接状态的G节点处理业务
            wtsl_core_start_dhcpd_server();
            // wtsl_core_gnode_business(pInfo);
        }else{
            //连接状态的T节点处理业务
            wtsl_core_start_dhcpc_client();
            // wtsl_core_tnode_business(pInfo);
        }
        WTSL_LOG_INFO(MODULE_NAME,"####### wtsl_core_splink_start_node_service_thread ########");
        //创建通信服务
        wtsl_core_splink_start_node_service_thread(args);
    }
    WTSL_LOG_WARNING(MODULE_NAME, "[%s][%d] should not be here",__FUNCTION__,__LINE__);
    return NULL;
}
void *wtsl_core_splink_state_watcher(void * args){
    (void)args;
    int ret = -1;
    WTSLNodeList *plist;
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] ",__FUNCTION__,__LINE__);
    while(g_is_run){
        pthread_mutex_lock(&global_node_info.mutex);
        wtsl_core_get_splink_info(&global_node_info);
        if(global_node_info.user_num > 0){
           //
        }else{
            // fprintf(stderr,"Warning: no user connected state\n");
        }
        plist = get_wtsl_core_node_list();
        if(plist != NULL){
            ret = update_wtsl_node_basicinfo(plist, &global_node_info.node_info.basic_info);
            if(ret != 0){
                WTSL_LOG_WARNING(MODULE_NAME,"[%s][%d] update basicinfo error",__FUNCTION__,__LINE__);
            }
        }
        pthread_mutex_unlock(&global_node_info.mutex);
        usleep(500000);
    }
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d] ",__FUNCTION__,__LINE__);
    return NULL;
}

void *wtsl_core_splink_auto_join_net(void * args){
    (void)args;
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d] ",__FUNCTION__,__LINE__);
    int idx = 0;     	
		
    while(g_is_run && global_node_info.node_info.basic_info.type == NODE_TYPE_SLB_T)
    {
    	pthread_mutex_lock(&auto_join_net_flag_mutex);
    	while(global_node_info.node_info.basic_info.auto_join_net_flag == 0)
    	{
			pthread_cond_wait(&auto_join_net_flag_cond, &auto_join_net_flag_mutex);		
    	}
    	pthread_mutex_unlock(&auto_join_net_flag_mutex);
    	
    	pthread_mutex_lock(&auto_join_net_mutex);
		while(global_node_info.link_state == 1 && g_is_run)
		{
			pthread_cond_wait(&auto_join_net_cond, &auto_join_net_mutex);
			idx = 0;
		}
        slb_t_node_scan("vap0");
		sleep(4);
		WTSL_LOG_INFO(MODULE_NAME, "start join_net: %s", global_node_info.node_info.basic_info.auto_join_net_mac[idx]);
		slb_t_node_join_bssid("vap0", global_node_info.node_info.basic_info.auto_join_net_mac[idx]);
		
		if(idx == 2)
			idx = 0;
		else
			idx++;
		pthread_mutex_unlock(&auto_join_net_mutex);
		sleep(1); //睡眠1S是为了判断是否入网
    }
    
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] ",__FUNCTION__,__LINE__);
    return NULL;
}

int create_splink_state_watcher(){
    pthread_t watcher_thread,node_service_thread,gnode_service_thread,auto_join_net_thread,sle_thread;
    pthread_attr_t attr;
    void *arg = &global_node_info;
    void *ret = 0;
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d] ",__FUNCTION__,__LINE__);
    //memset(&global_node_info,0,sizeof(global_node_info));
    pthread_attr_init(&attr);
    pthread_mutex_init(&global_node_info.mutex, NULL);
    pthread_mutex_init(&global_node_info.recv_mutex,NULL);

    pthread_create(&watcher_thread, &attr, wtsl_core_splink_state_watcher, arg);
    // fprintf(stderr,"[%s][%d] \n",__FUNCTION__,__LINE__);
    //节点同步线程，用于G下面的内部同步
    pthread_create(&node_service_thread, &attr, wtsl_core_splink_node_service, arg);
    //G节点服务线程，用于多G之间同步处理
    pthread_create(&gnode_service_thread, &attr, wtsl_core_splink_gnode_service, arg);

    pthread_create(&auto_join_net_thread, &attr, wtsl_core_splink_auto_join_net, arg);

    pthread_create(&sle_thread, &attr, wtsl_core_sle, arg);

    pthread_attr_destroy(&attr);
    pthread_join(node_service_thread,&ret);
    pthread_join(watcher_thread,&ret);
    pthread_mutex_destroy(&global_node_info.mutex);
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] ",__FUNCTION__,__LINE__);
    return 0;
}