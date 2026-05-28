#ifndef _WTSL_USER_MANAGER_H__
#define _WTSL_USER_MANAGER_H__

#include <cjson/cJSON.h>

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
    int user_id;        //id
    int group_id;       //gid
    int expire;
    int last_login_time;
    char username[64];
    char token[256];
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



//extern UserContext g_user_ctx;
typedef enum {
    RET_OK,                             // 成功（无数据）
    RET_ERR_PARAM,                      // 参数异常
    RET_ERR_NO_MEMORY,                  // 内存不足
    RET_ERR_USER_IS_NOT_EXISTED,        // 用户不存在
    RET_ERR_USER_IS_EXISTED,            // 用户已存在
    RET_ERR_OTHER                       //其他错误
} RetErrorCode;

typedef void* (ApiHandler)(char *url, const void *args,int size);

typedef struct {
    const char *uri;        // 接口路径 /api/login
    ApiHandler *handler;    // 处理函数
} RouteItem;


UserContext wtsl_core_user_get_context();
void wtsl_core_user_set_context(UserContext ctx);

void wtsl_core_user_get_routeitem(RouteItem **item,int *count);

// void* wtsl_core_user_login(const char *username,const char *password);
void* wtsl_core_user_login(char *url,void *args,int size);
void* wtsl_core_user_register(char *url,void *args,int size);
void* wtsl_core_user_user_set_info();
void* wtsl_core_user_get_info();
void* wtsl_core_user_delete();
void* wtsl_core_user_change_pwd(char *url,void *args,int size);


#endif