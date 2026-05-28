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
        WTSL_LOG_ERROR(MODULE_NAME,"Permission denied: User %d groupid: %d,need %d for %s, but only has %d",ctx->user_id,ctx->group_id, required, node->perm_map[func_index].func_name, ctx->permission);
        return NULL;
    }
    
    WTSL_LOG_DEBUG(MODULE_NAME,"Permission granted: User %d,group_id:%d execute %s", ctx->user_id,ctx->group_id,node->perm_map[func_index].func_name);
    
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




void *wtsl_core_nodes_get_node_by_id(WTSLNode* pNode,void*data,unsigned int size){
    (void)data;
    (void)size;
    int id = pNode->id;
    return (void *)id; // 返回指向节点信息的指针
}   


int check_is_remote_node(WTSLNode* pNode,void*data,unsigned int size){
    WTSL_LOG_DEBUG(MODULE_NAME,"check_is_remote_node,mac:%s,localmac:%s,data:%s,size:%d",pNode->info.mac,global_node_info.node_info.mac,data,size);
    if(strcmp(pNode->info.mac,global_node_info.node_info.mac) == 0)
    {
        return 0;
    }
    return -1;
}
void *call_remote_node_cmd(WTSLNode* pNode,void*data,unsigned int size){
    WTSL_LOG_DEBUG(MODULE_NAME,"do call_remote_note_cmd,mac:%s,data:%s,size:%d",pNode->info.mac,data,size);

    return NULL;
}

static void *warp_callback(WTSLNodeCallBack cb,WTSLNode* pNode,void*data,unsigned int size){
    //检查用户权限
    int ret = check_permission(pNode,data,size);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"no permission");
        return NULL;
    }
    //检查是否是远程节点
    ret = check_is_remote_node(pNode,data,size);
    if(ret == 0){
        return call_remote_node_cmd(pNode,data,size);
    }
    return cb(pNode,data,size);
}

void *_wtsl_core_nodes_get_all_nodes_info(WTSLNode* pNode,void *data,unsigned int size){
    (void)pNode;
    WTSL_LOG_DEBUG(MODULE_NAME, "Getting all nodes info,size:%d...",size);


    WTSL_Core_ListNodes *pListNodes = wtsl_core_get_node_list();
    if(pListNodes == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node manager is not initialized");
        return NULL;
    }
    if(pListNodes->list_nodes == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node list is NULL");
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "Getting all nodes info from node manager gsize:%d ###########...",pListNodes->list_nodes->size);

    cJSON *root = cJSON_CreateObject();
    cJSON *response = cJSON_CreateArray();
    for(int i=0;i<pListNodes->list_nodes->size;i++){
        WTSLNode *node = (WTSLNode *)list_get(pListNodes->list_nodes,i);
        if(node == NULL){
            WTSL_LOG_ERROR(MODULE_NAME, "No GNode found at index %d", i);
            continue;
        }
        WTSL_LOG_DEBUG(MODULE_NAME,"id:%d,name:%s,mac:%s",node->id,node->info.name,node->info.mac);
        cJSON* item = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "id", cJSON_CreateNumber(node->id));
        cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(node->type));
        cJSON_AddItemToObject(item, "name", cJSON_CreateString(node->info.name));
        cJSON_AddItemToObject(item, "mac", cJSON_CreateString(node->info.mac));
        cJSON_AddItemToArray(response, item);
    }
    cJSON_AddItemToObject(root, "nodes", response);
    const char *sample_info = cJSON_Print(root);

    size_t info_len = strlen(sample_info);

    if (size < info_len + 1) {
        WTSL_LOG_ERROR(MODULE_NAME, "Provided buffer is too small to hold node info");
        cJSON_Delete(root);
        return NULL;
    }

    // 将节点信息复制到提供的缓冲区
    strncpy(data, sample_info, size);
    cJSON_Delete(root);

    return data; // 返回指向节点信息的指针
}

void *wtsl_core_nodes_get_all_nodes_info(WTSLNode* pNode,void *data,unsigned int size){
    return warp_callback(_wtsl_core_nodes_get_all_nodes_info,pNode,data,size);
}

static void *wtsl_core_nodes_do_connect(WTSLNode *node,void *data,unsigned int size){
    int ret = -1;
    (void)size;
    WTSL_LOG_INFO(MODULE_NAME, "Connecting to node id:%d, name:%s, mac:%s...", node->id, node->info.name, node->info.mac);
    const char *sample_info = "{\"status\":\"connected\"}";
    ret = wtsl_core_parse_json_tnode_join_net(data);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to connect to node id:%d, name:%s, mac:%s", node->id, node->info.name, node->info.mac);
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "Successfully connected to node id:%d, name:%s, mac:%s", node->id, node->info.name, node->info.mac);
    return (void *)sample_info; // 返回指向节点信息的指针
}

static cJSON *create_node_info_json(const WTSLNode *node) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddItemToObject(item, "id", cJSON_CreateNumber(node->id));
    cJSON_AddItemToObject(item, "name", cJSON_CreateString(node->info.name));
    cJSON_AddItemToObject(item, "mac", cJSON_CreateString(node->info.mac));
    cJSON_AddItemToObject(item, "bw", cJSON_CreateNumber(node->info.bw));
    cJSON_AddItemToObject(item, "tfc_bw", cJSON_CreateNumber(node->info.tfc_bw));
    cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(node->info.type));
    cJSON_AddItemToObject(item, "channel", cJSON_CreateNumber(node->info.channel));
    cJSON_AddItemToObject(item, "rssi", cJSON_CreateNumber(node->info.rssi));
    cJSON_AddItemToObject(item, "ip", cJSON_CreateString(node->info.ip));
    cJSON_AddItemToObject(item, "version", cJSON_CreateString(node->info.version));
    return item;
}

void *wtsl_core_nodes_get_node_basicinfo(WTSLNode *pNode,void *data,unsigned int size){
    
    WTSL_LOG_INFO(MODULE_NAME, "Getting node basic info...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    fprintf(stderr,"pNode:%s,mac:%s\n",pNode->info.name,pNode->info.mac);
    cJSON *root = create_node_info_json(pNode);
    const char *sample_info = cJSON_Print(root);
    size_t info_len = strlen(sample_info);

    if (size < info_len + 1) {
        // 提供的缓冲区太小，无法存放节点信息
        return NULL;
    }

    // 将节点信息复制到提供的缓冲区
    strncpy(data, sample_info, size);

    
    return data; // 返回指向节点信息的指针
}

void *wtsl_core_nodes_get_node_conninfo(WTSLNode* pNode,void *data,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "Getting node connection info...");
   
    (void)size;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    
    WTSL_Core_ListNodes *pList = wtsl_core_get_node_list();
    if(pList == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node manager is not initialized");
        return NULL;
    }
    if(pList->list_nodes == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node list is NULL");
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "Getting node connection info from node manager gsize:%d ###########...",pList->list_nodes->size);

	int ret = wtsl_core_parse_json_view_users(data);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]connect info error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)data;
}



/**
 * 检测时间字符串的合法性
 * @param time_str 待检测的时间字符串（如 "2023-12-31 23:59:59"）
 * @param format 时间格式（如 "%Y-%m-%d %H:%M:%S"）
 * @return 0 合法，其他 非法
 */
int is_valid_time(const char *time_str, const char *format) {
    struct tm tm = {0};  // 初始化时间结构体
    char *ret;

    // 解析时间字符串到 struct tm
    ret = strptime(time_str, format, &tm);
    if (ret == NULL) {
        // 解析失败（格式不匹配）
        return -1;
    }

    // 保存解析后的原始值（用于后续对比）
    int original_year = tm.tm_year;
    int original_mon = tm.tm_mon;
    int original_mday = tm.tm_mday;
    int original_hour = tm.tm_hour;
    int original_min = tm.tm_min;
    int original_sec = tm.tm_sec;

    // 转换为时间戳，同时验证时间有效性
    time_t t = mktime(&tm);
    if (t == (time_t)-1) {
        // 时间无效（如月份 13）
        return -1;
    }

    // 检查 mktime 是否自动调整了时间（如 2023-02-30 会被调整为 2023-03-02）
    if (tm.tm_year != original_year || tm.tm_mon != original_mon ||
        tm.tm_mday != original_mday || tm.tm_hour != original_hour ||
        tm.tm_min != original_min || tm.tm_sec != original_sec) {
        return -1;
    }

    return 0;
}

void *wtsl_core_do_time_sync(WTSLNode* pNode,void *data,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "Performing time synchronization...");
    const char *format = "%Y-%m-%d %H:%M:%S";
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    fprintf(stderr,"[%s][%d],set (ip:%s,mac:%s),local:(ip:%s,mac:%s),data:%s,size:%d\n",__FUNCTION__,__LINE__,pNode->info.ip,pNode->info.mac,global_node_info.node_info.ip,global_node_info.node_info.mac,(char *)data,size);

    cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}

    char *timestr =  cJSON_GetObjectItem(root,"time")->valuestring;
	WTSL_LOG_INFO(MODULE_NAME, "timestr:%s",timestr);

    int valid = is_valid_time(timestr, format);
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] set time sync valid:%d",__FUNCTION__,__LINE__,valid);
    if(valid == 0){
        char time_buf[32]={0};
        sprintf(time_buf,"date -s \"%s\"",timestr);
        system(time_buf);
        return (void *)"set time success";
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] set time sync error",__FUNCTION__,__LINE__);
    return NULL;
}


static int check_user_has_permission(char *username,char *tokenstr,char *timestr){
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d],user:%s,token:%s,time:%s",__FUNCTION__,__LINE__,username,tokenstr,timestr);
    return -1;
}

void *wtsl_core_nodes_do_reboot(WTSLNode* pNode,void *data,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "##############   do_reboot ############...");
    const char *format = "%Y-%m-%d %H:%M:%S";
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    fprintf(stderr,"[%s][%d],data:%s,size:%d\n",__FUNCTION__,__LINE__,(char *)data,size);

    cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
    char *username = cJSON_GetObjectItem(root,"username")->valuestring;
    char *timestr =  cJSON_GetObjectItem(root,"time")->valuestring;
    char *tokenstr =  cJSON_GetObjectItem(root,"token")->valuestring;
	WTSL_LOG_INFO(MODULE_NAME, "username:%s,timestr:%s,tokenstr:%s",username,timestr,tokenstr);
    
    int valid = is_valid_time(timestr, format);
    if(valid != 0){
        WTSL_LOG_DEBUG(MODULE_NAME,"Do not right to reboot");
        return NULL;
    }
    int ret = check_user_has_permission(username,tokenstr,timestr);
    if(ret == 0){
        WTSL_LOG_WARNING(MODULE_NAME,"user:%s,time:%s do reboot",username,timestr);
        system("reboot");
    }else{
        WTSL_LOG_ERROR(MODULE_NAME,"user:%s has not right to reboot",username);
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] do reboot error",__FUNCTION__,__LINE__);
    return NULL;
}


void *wtsl_core_nodes_set_node_basicinfo(WTSLNode* pNode,void *data,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "Setting node basic info...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
		return NULL;
	}
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	int ret = wtsl_core_parse_json_setnode(data);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	WTSL_LOG_INFO(MODULE_NAME, "执行获取节点操作,wtsl_core_setnode");
	return (void *)"setnode_success";
}


void *wtsl_core_nodes_start_scan(WTSLNode* pNode,void *data,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "Starting scan for T-Nodes...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(pNode->id == 0){
        char ret = -1;
	    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	    ret = wtsl_core_parse_json_tnode_show_bss(data);
        if(ret != 0){
            WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
            return NULL;
        }
        return (void *)data;
    }
    const char *sample_info = "{\"basic_info\":{\"id\":0,\"name\":\"Node1\",\"mac\":\"01:10:20:30:40:50\"}}";
    size_t info_len = strlen(sample_info);
    if (size < info_len + 1) {
        return NULL;
    }
    strncpy(data, sample_info, size);
    return data; // 返回指向节点信息的指针
}

void *wtsl_core_nodes_do_upload(WTSLNode *p_node,void *data,unsigned int size){
    (void)p_node;
    WTSL_LOG_INFO(MODULE_NAME, "Starting firmware upload...");

    FILE *fp = fopen("/tmp/upgrade.bin","wb");
	fwrite(data,size,1,fp);
	fclose(fp);

    char recv_file_path[256] = "/tmp/upgrade.bin";
    return wtsl_core_parse_json_extract_file_header((void *)recv_file_path);
}

void *wtsl_core_nodes_do_upgrade(WTSLNode *p_node,void *args,unsigned int size){
    WTSL_LOG_INFO(MODULE_NAME, "Starting firmware upgrade...");
    (void)args;
    (void)p_node;
    (void)size;
	char recv_file_path[256] = "/tmp/upgrade.bin";
    return wtsl_core_firmware_upgrade((void *)recv_file_path);
}

// static void node_callback_init(WTSLNode *p_node){
//     p_node->get_all_nodes_info = wtsl_core_nodes_get_all_nodes_info;
//     // p_node->get_node_by_id = wtsl_core_nodes_get_node_by_id;

//     p_node->get_node_basicinfo = wtsl_core_nodes_get_node_basicinfo; 
//     WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d] p_node->get_node_basicinfo:0x%x\n",__FUNCTION__,__LINE__,p_node->get_node_basicinfo);
//     p_node->get_node_conninfo = wtsl_core_nodes_get_node_conninfo; 
//     p_node->do_time_sync = wtsl_core_do_time_sync;
//     p_node->set_node_basicinfo = wtsl_core_nodes_set_node_basicinfo;
//     p_node->start_scan = wtsl_core_nodes_start_scan;
//     p_node->do_connect = wtsl_core_nodes_do_connect;
//     p_node->do_upload = wtsl_core_nodes_do_upload;
//     p_node->do_upgrade = wtsl_core_nodes_do_upgrade;
//     p_node->do_reboot = wtsl_core_nodes_do_reboot;
// }

static WTSL_Core_ListNodes * wtsl_core_node_manager_init(WTSL_Core_ListNodes *node_manager){
    WTSLNode *p_node = (WTSLNode *)malloc(sizeof(WTSLNode));
    if (p_node == NULL) {
        WTSL_LOG_INFO(MODULE_NAME, "Failed to allocate memory for Node nodes");
        return NULL;
    }
    node_manager->list_nodes = list_create(sizeof(WTSLNode));
    if (node_manager->list_nodes == NULL) {
        free(p_node);
        WTSL_LOG_INFO(MODULE_NAME, "Failed to create list for GNodes");
        return NULL;
    }
    node_callback_init(p_node);
    list_insert_tail(node_manager->list_nodes, (void *)p_node);

    // 初始化节点管理器的成员变量
    gs_pListNodes = node_manager;
    return node_manager; // 初始化成功
}

WTSL_Core_ListNodes *wtsl_core_get_node_list(){
    return gs_pListNodes;
}

WTSLNode *wtsl_core_new_node(){
    WTSLNode *node = (WTSLNode *)malloc(sizeof(WTSLNode));
    if (node == NULL) {
        WTSL_LOG_INFO(MODULE_NAME, "Failed to allocate memory for Node");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d] node:0x%x,node->get_node_basicinfo:0x%x",__FUNCTION__,__LINE__,node,node->get_node_basicinfo);
    memset(node, 0, sizeof(WTSLNode));
    node_callback_init(node);
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d] node:0x%x,node->get_node_basicinfo:0x%x",__FUNCTION__,__LINE__,node,node->get_node_basicinfo);
    return node;
}

int wtsl_core_free_node(WTSLNode *node){
    if(node){
        free(node);
        node = NULL;
    }
    return 0;
}

WTSL_Core_ListNodes * wtsl_core_node_init(){
    WTSL_Core_ListNodes *node_manager = (WTSL_Core_ListNodes *)malloc(sizeof(WTSL_Core_ListNodes));
    if (node_manager == NULL) {
        fprintf(stderr, "Failed to allocate memory for node manager\n");
        return NULL;
    }
    return wtsl_core_node_manager_init(node_manager);
}

int wtsl_core_node_update(void *data,unsigned int size){
    (void)size;
    SPLINK_INFO *pInfo = (SPLINK_INFO *)data;
    if(pInfo == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "SPLINK_INFO pointer is NULL");
        return -1;
    }
    WTSL_Core_ListNodes *pListNodes = wtsl_core_get_node_list();
    if(pListNodes == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node manager is not initialized");
        return -1;
    }
    if(pListNodes->list_nodes == NULL || pListNodes->list_nodes->size < 1 ){
        WTSL_LOG_ERROR(MODULE_NAME, "Node list is NULL");
        return -1;
    }

    WTSLNode *node = (WTSLNode *)list_get(pListNodes->list_nodes,0);
    if(node == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "No Node found in the list");
        return -1;
    }
    memcpy(&node->info, &pInfo->node_info, sizeof(WTSLNodeInfo));
    return 0;
}

int wtsl_core_add_node(WTSL_Core_ListNodes *list, WTSLNode *node){
    if (list == NULL || node == NULL) {
        return -1; // 参数无效
    }
    list_insert_tail(list->list_nodes, node);
    return 0;
}


static void free_wtsl_node(void *data){
    (void)data;
    // WTSLNode *node = (WTSLNode *)data;
    // if (node == NULL) {
    //     return;
    // }
    // if(node->info.ip){
    //     free(node->info.ip);
    // }
    // if(node->info.mac){
    //     free(node->info.mac);
    // }
    // if(node->info.version){
    //     free(node->info.version);
    // }
    // if(node->info.name){
    //     free(node->info.name);
    // }
}

int wtsl_core_del_node(WTSL_Core_ListNodes *list, WTSLNode *node){
    if (list == NULL || node == NULL) {
        return -1; // 参数无效
    }
    return list_remove_at(list->list_nodes, node->id, free_wtsl_node);
}


WTSLNode *wtsl_core_get_node_by_id(WTSL_Core_ListNodes *list, int node_id){
    if (list == NULL) {
        return NULL; // 参数无效
    }
    for (int i = 0; i < list->list_nodes->size; i++) {
        WTSLNode *node = (WTSLNode *)list_get(list->list_nodes, i);
        if (node != NULL && node->id == node_id) {
            WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]idx:%d,node[%d]:0x%x\n",__FUNCTION__,__LINE__,i,node_id,node);
            return node; // 找到匹配的节点
        }
    }
    return NULL; // 未找到节点
}


WTSLNode *wtsl_core_get_node_by_mac(WTSL_Core_ListNodes *list, char *macstr){
    if (list == NULL) {
        WTSL_LOG_ERROR("node_manager","get node by mac list is null\n");
        return NULL; // 参数无效
    }
    WTSL_LOG_DEBUG("node_manager","get node by mac list size:%d",list->list_nodes->size);
    for (int i = 0; i < list->list_nodes->size; i++) {
        WTSLNode *node = (WTSLNode *)list_get(list->list_nodes, i);
        if(node == NULL){
            WTSL_LOG_WARNING("node_manager","get node by mac node[%d] is NULL",i);
            continue;
        }
        WTSL_LOG_DEBUG("node_manager","get node by mac find macstr:%s,node[%d]'s mac:%s",macstr,i,node->info.mac);
        if (node != NULL && (strcasecmp(node->info.mac,macstr) == 0)) {
            WTSL_LOG_DEBUG("node_manager","get node by mac find node");
            return node; // 找到匹配的节点
        }
    }
    WTSL_LOG_ERROR("node_manager","get node by mac can't find node\n");
    return NULL; // 未找到节点
}


int wtsl_core_set_node_info(WTSL_Core_ListNodes *list, int index, WTSLNodeInfo *info){
    if (list == NULL || info == NULL) {
        return -1; // 参数无效
    }
    WTSLNode *node = (WTSLNode *)list_get(list->list_nodes, index);
    if (node == NULL) {
        return -2; // 未找到节点
    }
    node->info = *info;
    return 0;
}

int wtsl_core_get_node(WTSL_Core_ListNodes *list, int index, WTSLNode **out_node){
    if (list == NULL || out_node == NULL) {
        return -1; // 参数无效
    }
    WTSLNode *node = (WTSLNode *)list_get(list->list_nodes, index);
    if (node == NULL) {
        return -2; // 未找到节点
    }
    *out_node = node;
    return 0;
}

int wtsl_core_node_manager_deinit(WTSL_Core_ListNodes *node_manager){
    if (node_manager == NULL) {
        return -1; // 参数无效
    }
    if(node_manager->list_nodes){
        free(node_manager->list_nodes);
    }
    node_manager->list_nodes = NULL;
    
    return 0;
}


#endif
