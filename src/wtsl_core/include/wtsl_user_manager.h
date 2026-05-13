#ifndef _WTSL_USER_MANAGER_H__
#define _WTSL_USER_MANAGER_H__
// #include "wtsl_core_node_manager.h"

// 权限级别定义
typedef enum {
    PERMISSION_NONE = 0,
    PERMISSION_READ = 1,
    PERMISSION_WRITE = 2,
    PERMISSION_EXECUTE = 4,
    PERMISSION_ADMIN = 7  // 1+2+4
} PermissionLevel;

// 用户上下文（包含权限信息）
typedef struct {
    int user_id;
    PermissionLevel permission;
} UserContext;

// 权限映射表（定义每个接口需要的权限）
#if 0
typedef struct {
    const char *func_name;
    PermissionLevel required_permission;
} PermissionMap;
#else
// ------------------------------
// 使用X宏生成权限映射表结构体
// ------------------------------
typedef struct {
    const char *func_name;
    PermissionLevel required_permission;
    int func_index;
} PermissionMap;
#endif


// int check_permission(WTSLNode *pNode,void *data,unsigned int size);


int check_user_has_permission(char *user,char *token,char *timestr);

extern UserContext g_user_ctx;
#endif