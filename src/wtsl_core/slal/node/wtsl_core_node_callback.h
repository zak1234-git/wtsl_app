#ifndef __WTSL_CORE_NODE_CALLBACK_H
#define __WTSL_CORE_NODE_CALLBACK_H
#include "wtsl_user_manager.h"
// #include "wtsl_core_node_manager.h"

// ------------------------------
// 默认接口实现函数声明
// ------------------------------
void* wtsl_core_get_all_nodes_info(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_node_basicinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_node_basicinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_time_sync(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_node_conninfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_start_scan(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_connect(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_upload(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_upgrade(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_reboot(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_hw_resources_usage(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_auto_join_net(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_show_bss(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_node_advinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_node_advinfo(void* phandle, void *data, unsigned int size, UserContext *ctx);

int check_permission_user(UserContext *ctx, PermissionLevel required);

void* wtsl_core_do_disconnect(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_restore(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_node_traffic(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_aifh(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_chswitch(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_adaptivemcsinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_traceinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_secalg(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_adaptivemcs(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_clearschmcs(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_mcsboundinfo(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_mcsbound(void* pNode, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_throughput(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_shortrange(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_remoterange(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_lowpow(void *ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_sle_start_scan(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_sle_show_bss(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_sle_connect(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_sle_conninfo(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_sle_basicinfo(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_get_sle_basicinfo(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_do_lowlatency(void* ph, void *data, unsigned int size, UserContext *ctx);

void* wtsl_core_set_announce_id(void* ph, void *data, unsigned int size, UserContext *ctx);
// ------------------------------
// X宏定义：集中管理所有函数配置
// ------------------------------
#if 0
#define NODE_FUNC_LIST \
    X(get_all_nodes_info, PERMISSION_READ, wtsl_core_get_all_nodes_info, 0) \
    X(get_node_basicinfo, PERMISSION_READ, wtsl_core_get_node_basicinfo, 1) \
    X(set_node_basicinfo, PERMISSION_WRITE, wtsl_core_set_node_basicinfo, 2) \
    X(do_time_sync, PERMISSION_EXECUTE, wtsl_core_do_time_sync, 3) \
    X(get_node_conninfo, PERMISSION_READ, wtsl_core_get_node_conninfo, 4) \
    X(start_scan, PERMISSION_EXECUTE, wtsl_core_start_scan, 5) \
    X(do_connect, PERMISSION_EXECUTE, wtsl_core_do_connect, 6) \
    X(do_upload, PERMISSION_WRITE, wtsl_core_do_upload, 7) \
    X(do_upgrade, PERMISSION_ADMIN, wtsl_core_do_upgrade, 8) \
    X(do_reboot, PERMISSION_ADMIN, wtsl_core_do_reboot, 9)

// ------------------------------
// 使用X宏生成接口表结构体
// ------------------------------


typedef struct {
#define X(name, perm, func, idx) WTSLNodeCallBack *name;
    NODE_FUNC_LIST
#undef X
} WTSLNodeInterface;

// ------------------------------
// 使用X宏生成函数索引枚举
// ------------------------------
typedef enum {
#define X(name, perm, func, idx) FUNC_##name = idx,
    NODE_FUNC_LIST
#undef X
    FUNC_COUNT
} FuncIndex;


// ------------------------------
// 使用X宏生成全局权限映射表
// ------------------------------
PermissionMap global_perm_map[] = {
#define X(name, perm, func, idx) {#name, perm, idx},
    NODE_FUNC_LIST
#undef X
};

// ------------------------------
// 使用X宏生成默认接口表
// ------------------------------
WTSLNodeInterface default_interface = {
#define X(name, perm, func, idx) func,
    NODE_FUNC_LIST
#undef X
};

// 通用的接口调用包装器
void* interface_wrapper(WTSLNode *node, 
                       WTSLNodeCallBack *real_func, 
                       int func_index, 
                       void *data, 
                       unsigned int size, 
                       UserContext *ctx);

 #define CALL_INTERFACE(node, func_name, func_idx, data, size, ctx) \
 interface_wrapper(node, node->interface->func_name, func_idx, data, size, ctx)


#endif
#endif