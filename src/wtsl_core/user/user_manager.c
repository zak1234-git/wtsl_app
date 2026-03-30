#include "wtsl_user_manager.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_node_manager.h"


#define MODLUE_NAME "user_manager"

UserContext g_user_ctx = {1001, PERMISSION_ADMIN};


int check_permission(WTSLNode *pNode,void *data,unsigned int size){

    WTSL_LOG_DEBUG(MODLUE_NAME,"check_permission,mac:%s,data:%s,size:%d",pNode->info.basic_info.mac,data,size);
    return 0;
}

// 权限检查函数
int check_permission_user(UserContext *ctx, PermissionLevel required) {
    if (!ctx) {
        WTSL_LOG_ERROR(MODLUE_NAME,"[%s][%d] ctx is null",__FUNCTION__,__LINE__);
        return 0;
    }
    // 检查用户是否有足够的权限（包含关系）
    return (ctx->permission & required) == required;
}

int check_user_has_permission(char *username,char *tokenstr,char *timestr){
    WTSL_LOG_INFO(MODLUE_NAME,"[%s][%d],user:%s,token:%s,time:%s",__FUNCTION__,__LINE__,username,tokenstr,timestr);
    return -1;
}