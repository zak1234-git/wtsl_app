#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "wtsl_core_node_manager.h"
#include <wtsl_core_api.h>
#include "wtsl_log_manager.h"
#include "wtsl_core_node_list.h"
#include "wtsl_user_manager.h"


extern SPLINK_INFO global_node_info;
pthread_mutex_t list_mutex;

#define MODULE_NAME "node_manager"


// ------------------------------
// 使用X宏生成默认接口表
// ------------------------------
WTSLNodeInterface default_interface = {
#define X(name, perm, func, idx) .name = (WTSLNodeCallBack*)func,
    NODE_FUNC_LIST
#undef X
};

// ------------------------------
// 使用X宏生成全局权限映射表
// ------------------------------
PermissionMap global_perm_map[] = {
#define X(name, perm, func, idx) {#name, perm, idx},
    NODE_FUNC_LIST
#undef X
};
// FUNC_COUNT 自动计算
#define FUNC_COUNT (sizeof(global_perm_map) / sizeof(PermissionMap))


#if 1

// static cJSON *create_node_info_json(const WTSLNode *node);


int check_is_remote_node(WTSLNode* pNode,void*data,unsigned int size){
    WTSL_LOG_DEBUG(MODULE_NAME,"check_is_remote_node,mac:%s,localmac:%s,data:%s,size:%d",pNode->info.basic_info.mac,global_node_info.node_info.basic_info.mac,data,size);
    if(!strcasecmp(pNode->info.basic_info.mac,global_node_info.node_info.basic_info.mac) == 0)
    {
        WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]do remote call",__FUNCTION__,__LINE__);
        return 0;
    }
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]do local call",__FUNCTION__,__LINE__);
    return -1;
}
void *call_remote_node_cmd(WTSLNode* pNode,WTSLNodeCallBack *real_func,void*data,unsigned int size,UserContext *ctx){
    WTSL_LOG_DEBUG(MODULE_NAME,"do call_remote_note_cmd,mac:%s,data:%s,size:%d,funptr:%p,permission:%d",pNode->info.basic_info.mac,data,size,real_func,ctx->permission);

    return NULL;
}


// ------------------------------
// 通用的接口调用包装器
// ------------------------------

void* interface_wrapper(WTSLNode *node, 
                       WTSLNodeCallBack *real_func, 
                       int func_index, 
                       void *data, 
                       unsigned int size, 
                       UserContext *ctx){
    int ret = -1;
    if (!node || !real_func || !node->perm_map || !ctx) {
        WTSL_LOG_ERROR(MODULE_NAME,"Error: Invalid parameters,node:0x%x,real_func:0x%x,ctx:0x%x,perm_map:0x%x",node,real_func,ctx,node->perm_map);
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME,"interface_wrapper node[%d]:(name:%s,mac:%s)",node->id,node->info.basic_info.name,node->info.basic_info.mac);
    // 获取该函数需要的权限
    PermissionLevel required = node->perm_map[func_index].required_permission;
    
    // 权限检查
    if (!check_permission_user(ctx, required)) {
        WTSL_LOG_ERROR(MODULE_NAME,"Permission denied: User %d need %d for %s, but only has %d",ctx->user_id, required, node->perm_map[func_index].func_name, ctx->permission);
        return NULL;
    }
    
    WTSL_LOG_DEBUG(MODULE_NAME,"Permission granted: User %d execute %s", ctx->user_id, node->perm_map[func_index].func_name);
    
    ret = check_is_remote_node(node,data,size);
    if(ret == 0){
        WTSL_LOG_DEBUG(MODULE_NAME,"remote node do call remote command");
        return call_remote_node_cmd(node,real_func,data,size,ctx);
    }
    // 执行实际的函数
    return real_func(node, data, size, ctx);
}




// 调用节点接口函数的通用方法
// void* call_node_interface(WTSLNode *node, 
//                          WTSLNodeCallBack *func, 
//                          void *data, 
//                          unsigned int size) {
//     if (node && func) {
//         return func(node, data, size);
//     }
//     return NULL;
// }

















#if 0
#endif





// static int check_user_has_permission(char *username,char *tokenstr,char *timestr){
//     WTSL_LOG_INFO(MODULE_NAME,"[%s][%d],user:%s,token:%s,time:%s",__FUNCTION__,__LINE__,username,tokenstr,timestr);
//     return -1;
// }



#else

略

#endif