#ifndef __WTSL_CORE_NODE_MANAGER_H__
#define __WTSL_CORE_NODE_MANAGER_H__
#include <netinet/in.h>
#include <sys/time.h>
// #include "wtsl_core_list.h"
// #include "wtsl_core_dataparser.h"
#include "wtsl_user_manager.h"
#include "wtsl_core_node_callback.h"

// 定义节点类型枚举，明确区分G节点和T节点
typedef enum {
    NODE_TYPE_SLB_G = 0,   // Grant节点
    NODE_TYPE_SLB_T = 1,   // Terminal节点
    NODE_TYPE_SLE_G = 5,
    NODE_TYPE_SLE_T = 6,
    NODE_TYPE_SLE_P = 7,
} WTSLNodeType;

typedef struct _mcs_bound_t{
	char user_mac[18];
	int min_mcs;
	int max_mcs;
}mcs_bound_t;

typedef struct _mib_params_t{
	int cp_type;
	int symbol_type;
	int sysmsg_period;
	int s_cfg_idx;
}mib_params_t;

typedef struct _slb_timestamp_t{
    int slb_cnt;
    int glb_cnt;
}slb_timestamp_t;

typedef struct _slb_wds_mode_t{
    int wds_enable;
    int wds_mode;
}slb_wds_mode_t;

typedef struct _mcs_entry_t{
    int user_idx;
    int ul_mcs;
    int dl_mcs;
}mcs_entry_t;

typedef struct _mcs_data_t{
    int count;
    mcs_entry_t user[256];
}mcs_data_t;

typedef struct _rssi_user_info_t{
    int user_idx;
    int rssi;
    int rsrp;
    int snr;
}rssi_user_info_t;

typedef struct _rssi_data_t{
    int count;
    rssi_user_info_t user[256];
}rssi_data_t;

typedef struct _real_power_t{
    int real_pow;
    int exp_pow;
}real_power_t;

typedef struct _WTSLNodeAdvInfo {
    int devid;
    char password[32];
    int cell_id;
    int power;
	int cc_offset;
    mcs_bound_t mcs_bound_table[32];
    mib_params_t mib_params;
    int sec_exch_cap;
    int sec_sec_cap;
    int lce_mode;
    int pps_enable;
    char fem_check[16];
    int chip_temperature;
    int range_opt;
    slb_timestamp_t timestamp;
    slb_wds_mode_t wds_mode;
    mcs_data_t user_mcs;
    real_power_t real_power;
    rssi_data_t user_rssi;
}WTSLNodeAdvInfo;

#pragma pack(push ,1)
typedef struct _WTSLNodeInfo {
    int bw;
    int tfc_bw;
    int channel;
    int type;
    int rssi;
    int domain_cnt;
    char ip[16];
    char mac[18];
    char version[32];
    char name[128];
    char domain_name[128];
    int log_port;
    char net_manage_ip[16];
    char auto_join_net_mac[3][18];
    int auto_join_net_flag;
    char bridge_interfaces[7][32];
    int bridge_interface_num;
    int dhcp_enable;
    int aifh_enable;
    int sle_type;
    char sle_name[32];
    uint64_t adv_info;
}WTSLNodeBasicInfo;
#pragma pack(pop)
typedef struct {
    int status;
    int rtt;
    int last_sync_time;
} WTSLNodeConnInfo;

typedef struct {
    WTSLNodeBasicInfo basic_info;
    WTSLNodeConnInfo conn_info;
} WTSLNodeInfo;

typedef struct _Node WTSLNode;


typedef void * (WTSLNodeCallBack)(void* pNode,void *data,unsigned int size,UserContext *ctx);


// 节点操作接口表（函数表）
// typedef struct {
//     WTSLNodeCallBack *get_all_nodes_info;  // 获取所有节点信息
//     WTSLNodeCallBack *get_node_basicinfo;  // 获取单个节点基本信息
//     WTSLNodeCallBack *set_node_basicinfo;  // 设置单个节点基本信息
//     WTSLNodeCallBack *do_time_sync;        // 时间同步
//     WTSLNodeCallBack *get_node_conninfo;   // 获取节点连接信息
//     WTSLNodeCallBack *start_scan;          // 启动扫描
//     WTSLNodeCallBack *do_connect;          // 建立连接
//     WTSLNodeCallBack *do_upload;           // 上传数据
//     WTSLNodeCallBack *do_upgrade;          // 固件升级
//     WTSLNodeCallBack *do_reboot;           // 重启节点
// } WTSLNodeInterface;


// // ------------------------------
// // X宏定义：集中管理所有函数配置
// // ------------------------------

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
    X(do_reboot, PERMISSION_ADMIN, wtsl_core_do_reboot, 9) \
    X(get_hw_resources_info,PERMISSION_READ,wtsl_core_get_hw_resources_usage,10)\
    X(do_autojoinNetwork,PERMISSION_READ,wtsl_core_auto_join_net,11)\
	X(show_bss_info,PERMISSION_READ,wtsl_core_show_bss,12)\
    X(get_node_advinfo,PERMISSION_READ,wtsl_core_get_node_advinfo,13)\
    X(set_node_advinfo, PERMISSION_WRITE, wtsl_core_set_node_advinfo,14)\
    X(do_disconnect, PERMISSION_WRITE, wtsl_core_do_disconnect,15)\
    X(do_restore, PERMISSION_WRITE, wtsl_core_do_restore,16)\
    X(get_node_traffic, PERMISSION_WRITE, wtsl_core_get_node_traffic,17)\
    X(do_aifh, PERMISSION_WRITE, wtsl_core_do_aifh,18)\
    X(do_chswitch, PERMISSION_WRITE, wtsl_core_do_chswitch,19)\
    X(get_adaptivemcsinfo, PERMISSION_WRITE, wtsl_core_get_adaptivemcsinfo,20)\
    X(get_traceinfo, PERMISSION_WRITE, wtsl_core_get_traceinfo,21)\
    X(get_secalg, PERMISSION_WRITE, wtsl_core_get_secalg,22)\
    X(set_adaptivemcs, PERMISSION_WRITE, wtsl_core_set_adaptivemcs,23)\
    X(do_clearschmcs, PERMISSION_WRITE, wtsl_core_do_clearschmcs,24)\
    X(get_mcsboundinfo, PERMISSION_WRITE, wtsl_core_get_mcsboundinfo,25)\
    X(set_mcsbound, PERMISSION_WRITE, wtsl_core_set_mcsbound,26)\
    X(do_throughput_test, PERMISSION_WRITE, wtsl_core_do_throughput,27)\
    X(do_shortrange_test, PERMISSION_WRITE, wtsl_core_do_shortrange,28)\
    X(do_remoterange_test, PERMISSION_WRITE, wtsl_core_do_remoterange,29)\
    X(do_sle_scan, PERMISSION_WRITE, wtsl_core_sle_start_scan,30)\
    X(get_sle_scan_result, PERMISSION_WRITE, wtsl_core_sle_show_bss,31)\
    X(do_sle_connect, PERMISSION_WRITE, wtsl_core_sle_connect,32)\
    X(do_lowpow_test, PERMISSION_WRITE, wtsl_core_do_lowpow,33)\
    X(get_sle_basicinfo, PERMISSION_WRITE, wtsl_core_get_sle_basicinfo,34)\
    X(get_sle_conninfo, PERMISSION_WRITE, wtsl_core_sle_conninfo,35)\
    X(set_sle_basicinfo, PERMISSION_WRITE, wtsl_core_set_sle_basicinfo,36)\
    X(do_lowlatency_test, PERMISSION_WRITE, wtsl_core_do_lowlatency,37)\
    X(set_sle_announce_id, PERMISSION_WRITE, wtsl_core_set_announce_id,38)
    
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

typedef struct _Node{
    int id;
    int active;                          // 节点是否活跃（1:活跃，0:离线）
    struct sockaddr_in addr;             // 节点地址（IP+端口）
    time_t last_heartbeat;               // 最后一次收到心跳响应的时间
    
    WTSLNodeType type;
    WTSLNodeInfo info;
    
    WTSLNodeInterface *interface;        // 指向接口表的指针
    PermissionMap *perm_map;             // 权限映射表
} WTSLNode;


// 链表节点结构
typedef struct _ListNode {
    WTSLNode node;
    struct _ListNode *next;
} WTSLListNode;

// 链表头结构
typedef struct {
    WTSLListNode *head;
    WTSLListNode *tail;
    int node_count;
} WTSLNodeList;




// 通用的接口调用包装器
void* interface_wrapper(WTSLNode *node, 
                       WTSLNodeCallBack *real_func, 
                       int func_index, 
                       void *data, 
                       unsigned int size, 
                       UserContext *ctx);

 #define CALL_INTERFACE_IMPL(node, func_name, func_idx, data, size, ctx) \
 interface_wrapper(node, node->interface->func_name, func_idx, data, size, ctx)

 #define CALL_INTERFACE(node, func_name, data, size, ctx) \
 interface_wrapper(node, node->interface->func_name, FUNC_##func_name, data, size, ctx)
#if 0

typedef struct _WTSL_Core_listNode {
    List *list_nodes;
}WTSL_Core_ListNodes;




WTSL_Core_ListNodes *wtsl_core_node_init(void);

int wtsl_core_node_update(void *data,unsigned int size);

//获取全局链表
WTSL_Core_ListNodes *wtsl_core_get_node_list();

WTSLNode *wtsl_core_get_node_by_id(WTSL_Core_ListNodes *list, int node_id);

WTSLNode *wtsl_core_get_node_by_mac(WTSL_Core_ListNodes *list, char *macstr);

//新建节点
WTSLNode *wtsl_core_new_node();

int wtsl_core_free_node(WTSLNode *node);

//添加节点
int wtsl_core_add_node(WTSL_Core_ListNodes *list, WTSLNode *node);

//删除节点
int wtsl_core_del_node(WTSL_Core_ListNodes *list, WTSLNode *node);

int wtsl_core_node_manager_deinit(WTSL_Core_ListNodes *listnodes);

int slb_set_msc_bound(char *dev_name, mcs_bound_t mcs_bound);

int slb_set_mib_params(char *dev_name, mib_params_t mib_params);
// WTSL_Core_ListNodes *wtsl_core_get_global_node_manager(void);

#endif


extern WTSLNodeInterface default_interface;
extern PermissionMap global_perm_map[];

#endif /* __WTSL_CORE_NODE_MANAGER_H__ */