#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include "wtsl_user_manager.h"
#include "wtsl_core_node_callback.h"
#include "wtsl_core_node_manager.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_node_list.h"
#include "wtsl_core_api.h"
#include "wtsl_core_slb_qos_core.h"
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "wtsl_core_dataparser.h"
#include "wtsl_cfg_manager.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "sle_conn_client.h"
#include "sle_connection_manager.h"

#define MODULE_NAME "node_callback"

extern log_system_t g_log_system;
extern SPLINK_INFO global_node_info;
extern pthread_mutex_t auto_join_net_flag_mutex;
extern pthread_cond_t auto_join_net_flag_cond;
extern void sle_start_scan(void);
extern void sle_create_connection(const sle_addr_t *remote_addr);
extern sle_seek_result_info_t seek_result_info[SLE_MAX_CONN];
extern uint8_t scan_num;
int connected_num = 0;
extern connect_manage_t connect_manage[SLE_MAX_CONN];
int announce_id[20] = {0};
int announce_id_num = 0;


void* wtsl_core_get_all_nodes_info(void* pNode, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    (void)pNode;
    WTSL_LOG_DEBUG(MODULE_NAME, "Getting all nodes info,size:%d...",size);


    WTSLNodeList *pListNodes = get_wtsl_core_node_list();
    if(pListNodes == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node manager is not initialized");
        return NULL;
    }
   
    WTSL_LOG_INFO(MODULE_NAME, "Getting all nodes info from node manager gsize:%d ###########...",pListNodes->node_count);

    cJSON *root = cJSON_CreateObject();
    cJSON *response = cJSON_CreateArray();
    for(int i=0;i<pListNodes->node_count;i++){
        WTSLNode *node = find_wtsl_node_by_id(pListNodes,i);
        if(node == NULL){
            WTSL_LOG_ERROR(MODULE_NAME, "No GNode found at index %d", i);
            continue;
        }
        WTSL_LOG_DEBUG(MODULE_NAME,"id:%d,name:%s,mac:%s",node->id,node->info.basic_info.name,node->info.basic_info.mac);
        cJSON* item = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "id", cJSON_CreateNumber(node->id));
        cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(node->info.basic_info.type));
        cJSON_AddItemToObject(item, "name", cJSON_CreateString(node->info.basic_info.name));
        cJSON_AddItemToObject(item, "mac", cJSON_CreateString(node->info.basic_info.mac));
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

// static void *warp_callback(WTSLNodeCallBack cb,WTSLNode* pNode,void*data,unsigned int size,UserContext *ctx){
//     //检查用户权限
//     int ret = -1;
//     // ret = check_permission(pNode,data,size);
//     // if(ret != 0){
//     //     WTSL_LOG_ERROR(MODULE_NAME,"no permission");
//     //     return NULL;
//     // }else{
//     //     WTSL_LOG_DEBUG(MODULE_NAME,"has permission");
//     // }
//     //检查是否是远程节点
//     ret = check_is_remote_node(pNode,data,size);
//     if(ret == 0){
//         WTSL_LOG_ERROR(MODULE_NAME,"remote node exec command");
//         return call_remote_node_cmd(pNode,data,size);
//     }else{
//         WTSL_LOG_DEBUG(MODULE_NAME,"local node exec command");
//     }
//     return cb(pNode,data,size,ctx);
// }

// void *wtsl_core_nodes_get_all_nodes_info(WTSLNode* pNode,void *data,unsigned int size,UserContext *ctx){
//     return warp_callback(_wtsl_core_get_all_nodes_info,pNode,data,size,ctx);
// }


static cJSON *create_node_info_json(const WTSLNode *node) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddItemToObject(item, "id", cJSON_CreateNumber(node->id));
    cJSON_AddItemToObject(item, "name", cJSON_CreateString(node->info.basic_info.name));
    cJSON_AddItemToObject(item, "mac", cJSON_CreateString(node->info.basic_info.mac));
    cJSON_AddItemToObject(item, "bw", cJSON_CreateNumber(node->info.basic_info.bw));
    cJSON_AddItemToObject(item, "tfc_bw", cJSON_CreateNumber(node->info.basic_info.tfc_bw));
    cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(node->info.basic_info.type));
    cJSON_AddItemToObject(item, "channel", cJSON_CreateNumber(node->info.basic_info.channel));
    cJSON_AddItemToObject(item, "rssi", cJSON_CreateNumber(node->info.basic_info.rssi));
    cJSON_AddItemToObject(item, "ip", cJSON_CreateString(node->info.basic_info.ip));
    cJSON_AddItemToObject(item, "version", cJSON_CreateString(node->info.basic_info.version));
    cJSON_AddItemToObject(item, "net_manage_ip", cJSON_CreateString(node->info.basic_info.net_manage_ip));
    cJSON_AddItemToObject(item, "log_port", cJSON_CreateNumber(node->info.basic_info.log_port));
    cJSON_AddItemToObject(item, "ai_flag", cJSON_CreateNumber(node->info.basic_info.auto_join_net_flag));
    return item;
}

static cJSON *create_node_advinfo_json(const WTSLNode *node) {
    cJSON *item = cJSON_CreateObject();
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;

    // 同步高级信息接口
    sync_advinfo();
    sleep(1);
    cJSON_AddItemToObject(item, "id", cJSON_CreateNumber(node->id));
    cJSON_AddItemToObject(item, "devid", cJSON_CreateNumber(adv_info->devid));
    cJSON_AddItemToObject(item, "cell_id", cJSON_CreateNumber(adv_info->cell_id));
    cJSON_AddItemToObject(item, "cc_spos", cJSON_CreateNumber(adv_info->cc_offset));
    cJSON_AddItemToObject(item, "cp_type", cJSON_CreateNumber(adv_info->mib_params.cp_type));
    cJSON_AddItemToObject(item, "symbol_type", cJSON_CreateNumber(adv_info->mib_params.symbol_type));
    cJSON_AddItemToObject(item, "sysmsg_period", cJSON_CreateNumber(adv_info->mib_params.sysmsg_period));
    cJSON_AddItemToObject(item, "s_cfg_idx", cJSON_CreateNumber(adv_info->mib_params.s_cfg_idx));
    cJSON_AddItemToObject(item, "sec_exch_cap", cJSON_CreateNumber(adv_info->sec_exch_cap));
    cJSON_AddItemToObject(item, "sec_sec_cap", cJSON_CreateNumber(adv_info->sec_sec_cap));
    cJSON_AddItemToObject(item, "fem_check", cJSON_CreateString(adv_info->fem_check));
	cJSON_AddItemToObject(item, "chip_temperature", cJSON_CreateNumber(adv_info->chip_temperature));
	cJSON_AddItemToObject(item, "DHCP_enable", cJSON_CreateNumber(node->info.basic_info.dhcp_enable));
	cJSON_AddItemToObject(item, "range_opt", cJSON_CreateNumber(adv_info->range_opt));

    if (node->info.basic_info.type == 0)
    {
        cJSON_AddItemToObject(item, "lce_mode", cJSON_CreateNumber(adv_info->lce_mode));
		cJSON_AddItemToObject(item, "acs_enable", cJSON_CreateNumber(node->info.basic_info.aifh_enable));
    }
    cJSON_AddItemToObject(item, "pps_switch", cJSON_CreateNumber(adv_info->pps_enable));

    cJSON *timestamp = cJSON_CreateObject();
    cJSON_AddItemToObject(timestamp, "slb_cnt", cJSON_CreateNumber(adv_info->timestamp.slb_cnt));
    cJSON_AddItemToObject(timestamp, "glb_cnt", cJSON_CreateNumber(adv_info->timestamp.glb_cnt));
    cJSON_AddItemToObject(item, "slb_timestamp", timestamp);

    cJSON *wds_mode = cJSON_CreateObject();
    cJSON_AddItemToObject(wds_mode, "wds_enable", cJSON_CreateNumber(adv_info->wds_mode.wds_enable));
    cJSON_AddItemToObject(wds_mode, "wds_mode", cJSON_CreateNumber(adv_info->wds_mode.wds_mode));
    cJSON_AddItemToObject(item, "wds", wds_mode);
    
    cJSON *view_mcs_obj = cJSON_CreateObject();
    cJSON *mcs_data_array = cJSON_CreateArray();
    for (int i = 0; i < adv_info->user_mcs.count; i++) {
        cJSON *mcs_user_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(mcs_user_obj, "user_idx", adv_info->user_mcs.user[i].user_idx);
        cJSON_AddNumberToObject(mcs_user_obj, "ul_mcs", adv_info->user_mcs.user[i].ul_mcs);
        cJSON_AddNumberToObject(mcs_user_obj, "dl_mcs", adv_info->user_mcs.user[i].dl_mcs);
        cJSON_AddItemToArray(mcs_data_array, mcs_user_obj);
    }
    cJSON_AddItemToObject(view_mcs_obj, "data", mcs_data_array);
    cJSON_AddItemToObject(item, "view_mcs", view_mcs_obj);

    cJSON *real_power = cJSON_CreateObject();
    cJSON_AddItemToObject(real_power, "real_pow", cJSON_CreateNumber(adv_info->real_power.real_pow));
    cJSON_AddItemToObject(real_power, "exp_pow", cJSON_CreateNumber(adv_info->real_power.exp_pow));
    cJSON_AddItemToObject(item, "real_power", real_power);

    cJSON *rssi_info_obj = cJSON_CreateObject();
    cJSON *rssi_data_array = cJSON_CreateArray();
    for (int i = 0; i < adv_info->user_rssi.count; i++) {
        cJSON *rssi_user_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(rssi_user_obj, "user_idx", adv_info->user_rssi.user[i].user_idx);
        cJSON_AddNumberToObject(rssi_user_obj, "rssi", adv_info->user_rssi.user[i].rssi);
        cJSON_AddNumberToObject(rssi_user_obj, "rsrp", adv_info->user_rssi.user[i].rsrp);
        cJSON_AddNumberToObject(rssi_user_obj, "snr", adv_info->user_rssi.user[i].snr);
        cJSON_AddItemToArray(rssi_data_array, rssi_user_obj);
    }
    cJSON_AddItemToObject(rssi_info_obj, "data", rssi_data_array);
    cJSON_AddItemToObject(item, "rssi", rssi_info_obj);
    
    return item;
}


//
void* wtsl_core_get_node_basicinfo(void* phandle, void *data, unsigned int size, UserContext *ctx){

    (void)ctx;
    WTSLNode *pNode = (WTSLNode *)phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    cJSON *root = create_node_info_json(pNode);
    const char *sample_info = cJSON_Print(root);
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d],response json:%s",__FUNCTION__,__LINE__,sample_info);
    size_t info_len = strlen(sample_info);
    if (size < info_len + 1) {
        // 提供的缓冲区太小，无法存放节点信息
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] size is too samall",__FUNCTION__,__LINE__);
        return NULL;
    }
    // 将节点信息复制到提供的缓冲区
    strncpy(data, sample_info, size);
    return data; // 返回指向节点信息的指针
}

void* wtsl_core_qos_get_status(void* ph, void *data, unsigned int size, UserContext *ctx){
	(void)ctx;
	WTSLNode *pNode = (WTSLNode *)ph;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	cJSON *resp = cJSON_CreateObject();

	cJSON_AddItemToObject(resp, "enabled", cJSON_CreateBool(g_state.enabled));
	cJSON_AddItemToObject(resp, "snapshot_count", cJSON_CreateNumber(g_state.snapshot_count));
	cJSON_AddItemToObject(resp, "device", cJSON_CreateString(g_state.default_device));

	const char *qos_info = cJSON_Print(resp);
	WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d],response json:%s",__FUNCTION__,__LINE__,qos_info);
	size_t info_len = strlen(qos_info);
	if (size < info_len + 1) {
		WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] size is too samall",__FUNCTION__,__LINE__);
		return NULL;
	}
	strncpy(data, qos_info, size);
	return data;
}

//
void* wtsl_core_get_node_advinfo(void* phandle, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    WTSLNode *pNode = (WTSLNode *)phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }

    cJSON *root = create_node_advinfo_json(pNode);
    const char *sample_advinfo = cJSON_Print(root);
    WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d],response json:%s",__FUNCTION__,__LINE__,sample_advinfo);
    size_t advinfo_len = strlen(sample_advinfo);
    if (size < advinfo_len + 1) {
        // 提供的缓冲区太小，无法存放节点信息
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] size is too samall",__FUNCTION__,__LINE__);
        return NULL;
    }
    // 将节点高级信息复制到提供的缓冲区
    strncpy(data, sample_advinfo, size);
    return data;
}

void* wtsl_core_set_node_advinfo(void* phandle, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "Setting node Advanced information...");
    WTSLNode* pNode = (WTSLNode*)phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

    ret = wtsl_core_parse_json_set_node_advinfo(data);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
        return NULL;
    }

    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);

    return (void *)"set_advinfo_success";
}

//
void* wtsl_core_set_node_basicinfo(void* phandle, void *data, unsigned int size, UserContext *ctx){

    (void)ctx;
    int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "Setting node basic info...");
    WTSLNode* pNode = (WTSLNode* )phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
		return NULL;
	}
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	ret = wtsl_core_parse_json_setnode(data);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	} else {
        char *filename = generate_file_name();
        if (rename(g_log_system.log_file_path, filename) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "failed to rename log file");
        } else {
            strcpy(g_log_system.log_file_path, filename);
            free(filename);
        }
    }
    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);
	return (void *)"setnode_success";
}

//
void* wtsl_core_qos_switch(void* phandle, void *data, unsigned int size, UserContext *ctx){
	(void)ctx;
	int ret = -1;
	WTSL_LOG_INFO(MODULE_NAME, "switch node qos on/off...");
    WTSLNode* pNode = (WTSLNode* )phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
		return NULL;
	}
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* type = cJSON_GetObjectItemCaseSensitive(root,"enabled");
	if(type != NULL && cJSON_IsBool(type)) {
		g_state.enabled = type->valueint;
		ret = qos_toggle_switch(g_state.enabled);
		if (ret != 0) {
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
			return NULL;
		}
	}
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);
	return (void *)"qos_switch_success";
}



/**
 * 检测时间字符串的合法性
 * @param time_str 待检测的时间字符串（如 "2023-12-31 23:59:59"）
 * @param format 时间格式（如 "%Y-%m-%d %H:%M:%S"）
 * @return 0 合法，其他 非法
 */
int is_valid_time(const char *time_str, const char *format) {
    struct tm tm = {0};  // 初始化时间结构体
    char *ret = NULL;

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

//
void* wtsl_core_do_time_sync(void* phandle, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    int ret = -1;
    WTSLNode* pNode = (WTSLNode* )phandle;
    WTSL_LOG_INFO(MODULE_NAME, "Performing time synchronization...");
    const char *format = "%Y-%m-%d %H:%M:%S";
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],set (ip:%s,mac:%s),local:(ip:%s,mac:%s),data:%s,size:%d",__FUNCTION__,__LINE__,pNode->info.basic_info.ip,pNode->info.basic_info.mac,global_node_info.node_info.basic_info.ip,global_node_info.node_info.basic_info.mac,(char *)data,size);

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
        ret = system(time_buf);
        if(ret != 0){
            WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
        } else {
            char *filename = generate_file_name();
            if (rename(g_log_system.log_file_path, filename) != 0) {
                WTSL_LOG_ERROR(MODULE_NAME, "failed to rename log file");
            } else {
                strcpy(g_log_system.log_file_path, filename);
                free(filename);
            }
        }
        return (void *)"set time success";
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] set time sync error",__FUNCTION__,__LINE__);
    return NULL;
}


//
void* wtsl_core_get_node_conninfo(void* phandle, void *data, unsigned int size, UserContext *ctx){
    
    (void)size;
    (void)ctx;
    WTSLNode* pNode =(WTSLNode* )phandle; 
    WTSL_LOG_INFO(MODULE_NAME, "Getting node connection info...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    
    WTSLNodeList *pList = get_wtsl_core_node_list();
    if(pList == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node list is NULL");
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "Getting node connection info from node manager gsize:%d ###########...",pList->node_count);

	int ret = wtsl_core_parse_json_view_users(data);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]connect info error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)data;
}
void* wtsl_core_start_scan(void* ph, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "Starting scan for T-Nodes...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(pNode->id == 0){
        char ret = -1;
	    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	    //关闭自动入网
	    global_node_info.node_info.basic_info.auto_join_net_flag = 0;
	    //断开入网，已经入网状态下不能进行扫描
	    slb_ifconfig(NET_VAP_NAME, 0);
	    slb_ifconfig(NET_VAP_NAME, 1);
	    wtsl_core_parse_json_scan(NULL);
	    sleep(5);
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


//
void* wtsl_core_do_connect(void* pNode, void *data, unsigned int size, UserContext *ctx){
    int ret = -1;
    (void)size;
    (void)ctx;
    WTSLNode *node = (WTSLNode *)pNode;
    WTSL_LOG_INFO(MODULE_NAME, "Connecting to node id:%d, name:%s, mac:%s...", node->id, node->info.basic_info.name, node->info.basic_info.mac);
    const char *sample_info = "{\"status\":\"connected\"}";
    ret = wtsl_core_parse_json_tnode_join_net(data);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to connect to node id:%d, name:%s, mac:%s", node->id, node->info.basic_info.name, node->info.basic_info.mac);
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "Successfully connected to node id:%d, name:%s, mac:%s", node->id, node->info.basic_info.name, node->info.basic_info.mac);
    return (void *)sample_info; // 返回指向节点信息的指针
}


//
void* wtsl_core_do_upload(void* pNode, void *data, unsigned int size, UserContext *ctx){
    (void)pNode;
    (void)ctx;
    WTSL_LOG_INFO(MODULE_NAME, "Starting firmware upload...");

    FILE *fp = fopen("/tmp/upgrade.bin","wb");
	fwrite(data,size,1,fp);
	fclose(fp);

    char recv_file_path[256] = "/tmp/upgrade.bin";
    return wtsl_core_parse_json_extract_file_header((void *)recv_file_path);
}


//
void* wtsl_core_do_upgrade(void* pNode, void *data, unsigned int size, UserContext *ctx){
    (void)data;
    (void)pNode;
    (void)size;
    (void)ctx;
    WTSL_LOG_INFO(MODULE_NAME, "Starting firmware upgrade...");
	char recv_file_path[256] = "/tmp/upgrade.bin";
    return wtsl_core_firmware_upgrade((void *)recv_file_path);
}


//
void* wtsl_core_do_reboot(void* phandle, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    WTSLNode* pNode = (WTSLNode* )phandle;
    WTSL_LOG_INFO(MODULE_NAME, "##############   do_reboot ############...");
    const char *format = "%Y-%m-%d %H:%M:%S";
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],data:%s,size:%d",__FUNCTION__,__LINE__,(char *)data,size);

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
        ret = system("reboot");
        if(ret != 0){
            WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]user:%s,time:%s do reboot",__FUNCTION__,__LINE__,username,timestr);
        }
    }else{
        WTSL_LOG_ERROR(MODULE_NAME,"user:%s has not right to reboot",username);
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] do reboot error",__FUNCTION__,__LINE__);
    return NULL;
}

void* wtsl_core_auto_join_net(void* phandle, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    WTSL_LOG_INFO(MODULE_NAME, "set auto join net flag....");
    WTSLNode* pNode = (WTSLNode* )phandle;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
		return NULL;
	}
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	WTSLNodeBasicInfo* node = &global_node_info.node_info.basic_info;
	
	pthread_mutex_lock(&auto_join_net_flag_mutex);
	node->auto_join_net_flag = cJSON_GetObjectItem(root,"aj_flag")->valueint;
	pthread_cond_signal(&auto_join_net_flag_cond);
	pthread_mutex_unlock(&auto_join_net_flag_mutex);

	config_set_int("AUTO_JOIN_NET", node->auto_join_net_flag);
	cJSON_Delete(root);
	WTSL_LOG_INFO(MODULE_NAME, "执行设置自动入网开关操作,wtsl_core_auto_join_net");
	return (void *)"success";
}

void* wtsl_core_show_bss(void* ph, void *data, unsigned int size, UserContext *ctx){
    (void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "show bss...");
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

void* wtsl_core_do_disconnect(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "do disconnect...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char disconnect_mac[18] = {0};
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* mac = cJSON_GetObjectItem(root,"mac");
	if(mac != NULL)
		strcpy(disconnect_mac, mac->valuestring);
		
	
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"kick_user %s\"", NET_VAP_NAME, disconnect_mac);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		if(sscanf(buffer,"%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "kick_user status:%s\n", status);
			break;
		}	
	}
	pclose(fp);
	
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec disconnect %s.......",__FUNCTION__,__LINE__, disconnect_mac);	
	
	cJSON_Delete(root);

	cJSON* tmp_root = cJSON_CreateObject();
	if(strcmp(status, "SUCC") == 0)
		cJSON_AddItemToObject(tmp_root, "status", cJSON_CreateString("success"));		
	else
		cJSON_AddItemToObject(tmp_root, "status", cJSON_CreateString("failed"));
	char* json_str = cJSON_Print(tmp_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
        cJSON_Delete(tmp_root);
        return NULL;
    }

    cJSON_Delete(tmp_root);
    return json_str;
}

void* wtsl_core_do_restore(void* pNode, void *data, unsigned int size, UserContext *ctx)
{
	(void)pNode;
	(void)data;
	(void)size;
	(void)ctx;
	
	int ret = system("rm /home/wt/env_config");
	if(ret != 0)
	{
		return NULL;
	}
	WTSL_LOG_INFO(MODULE_NAME, "rm /home/wt/env_config and reboot");
	system("reboot");
	
	return (void *)"success";
}

void* wtsl_core_get_node_traffic(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "do traffic...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char interface[64] ={0};
	char cmd[64] ={0};
	char buffer[128] ={0};
	unsigned long long int rx_bytes = 0;
	unsigned long long int tx_bytes = 0;
	unsigned long long int timestamp = 0;
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_interface = cJSON_GetObjectItem(root,"interface");
	if(cjson_interface != NULL)			
		strcpy(interface, cjson_interface->valuestring);
		
	cJSON_Delete(root);
	
	snprintf(cmd, sizeof(cmd), "ifconfig %s", interface);
	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "exec command failed: %s", cmd);
		return NULL;
	}
	while(fgets(buffer, sizeof(buffer), fp))
	{
		int num = sscanf(buffer, "           RX bytes:%llu %*s %*s  TX bytes:%llu %*s %*s", &rx_bytes, &tx_bytes);
		if(num == 2)
		{
			WTSL_LOG_INFO(MODULE_NAME, "get %s rx_bytes: %llu, tx_bytes: %llu", interface, rx_bytes, tx_bytes);
			break;
		}
	}
	pclose(fp);
	fp = popen("date +%s", "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "exec command failed: %s", "date +%s");
		return NULL;
	}	
	fgets(buffer, sizeof(buffer), fp);
	pclose(fp);
	timestamp = atoi(buffer);
	cJSON* reply_root = cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "id", cJSON_CreateNumber(0));		
	cJSON_AddItemToObject(reply_root, "timestamp", cJSON_CreateNumber(timestamp));
	cJSON_AddItemToObject(reply_root, "rx_bytes", cJSON_CreateNumber(rx_bytes));
	cJSON_AddItemToObject(reply_root, "tx_bytes", cJSON_CreateNumber(tx_bytes));
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	
	cJSON_Delete(reply_root);
	return json_str;	
}

void* wtsl_core_do_aifh(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "do aifh...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char cmd[64] ={0};
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_aifh_sw = cJSON_GetObjectItem(root,"aifh_sw");
	if(cjson_aifh_sw != NULL)
		global_node_info.node_info.basic_info.aifh_enable = cjson_aifh_sw->valueint;

	cJSON_Delete(root);
		
	int aifh_enable = global_node_info.node_info.basic_info.aifh_enable;
	if(aifh_enable != 0 && aifh_enable != 1)
		return NULL;

	config_set_int("AIFH_ENABLE", aifh_enable);
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_acs_enable %d\"", NET_VAP_NAME, aifh_enable);
	int ret = system(cmd);
	if(ret != 0)
	{
		return NULL;
	}
	
	return (void *)"success";
}

void* wtsl_core_do_chswitch(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "do chswitch...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char cmd[64] ={0};
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_channel = cJSON_GetObjectItem(root,"channel");
	if(cjson_channel != NULL)
		global_node_info.node_info.basic_info.channel = cjson_channel->valueint;

	cJSON_Delete(root);	
		
	int channel = global_node_info.node_info.basic_info.channel;
	if(channel != 41 && channel != 125 && channel != 209 && channel != 291 && channel != 375 && channel != 459 && channel != 541 && channel != 625
	 && channel != 709 && channel != 791 && channel != 1375 && channel != 1459 && channel != 1541 && channel != 1625 && channel != 1709 && channel != 1791
	  && channel != 1875 && channel != 1959 && channel != 2041 && channel != 2125 && channel != 2209 && channel != 2291 && channel != 2479 && channel != 2563
	   && channel != 2645 && channel != 2729 && channel != 2813)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]Error: Invalid channel", __func__,__LINE__);
		return NULL;
	}

	config_set_int("CHANNEL", channel);
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"bss_chan_switch %d\"", NET_VAP_NAME, channel);
	int ret = system(cmd);
	if(ret != 0)
	{
		return NULL;
	}
	
	return (void *)"success";	
}

void* wtsl_core_get_secalg(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "get secalg...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char accsee_layer_mac[18] = {0};
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	char algorithm[20] = {0};
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(accsee_layer_mac, cjson_mac->valuestring);
		
	
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"get_sec_alg %s\"", NET_VAP_NAME, accsee_layer_mac);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]%*[^[][%[^]]", status, algorithm);		
	WTSL_LOG_INFO(MODULE_NAME, "status:%s, algorithm: %s", status, algorithm);
	pclose(fp);
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec get_secalg %s.......",__FUNCTION__,__LINE__, accsee_layer_mac);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
	{
		return NULL;
	}

	cJSON* tmp_root = cJSON_CreateObject();
	cJSON_AddItemToObject(tmp_root, "algorithm", cJSON_CreateString(algorithm));	
	char* json_str = cJSON_Print(tmp_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(tmp_root);
		return NULL;
	}	
	
	cJSON_Delete(tmp_root);
	return json_str;

}

void* wtsl_core_set_adaptivemcs(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "set adaptivemcs...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char res_type[18] = {0};
	char dir[18] = {0};
	char buffer[128] = {0};
	char cmd[128] ={0};
	char status[8] = {0};
	int mcs = 0;
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);

	cJSON* cjson_res_type = cJSON_GetObjectItem(root,"res_type");
	if(cjson_res_type != NULL)
		strcpy(res_type, cjson_res_type->valuestring);	

	cJSON* cjson_dir = cJSON_GetObjectItem(root,"dir");
	if(cjson_dir != NULL)
		strcpy(dir, cjson_dir->valuestring);		

	cJSON* cjson_mcs = cJSON_GetObjectItem(root,"mcs");
	if(cjson_mcs != NULL)
		mcs = cjson_mcs->valueint;
		
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"sch_res_type_mcs %s %s %s %d\"", NET_VAP_NAME, user_mac, res_type, dir, mcs);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]", status);		
	WTSL_LOG_INFO(MODULE_NAME, "status:%s", status);
	pclose(fp);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
		return NULL;
	else
		return (void*)"success";
}


void* wtsl_core_do_clearschmcs(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "set clearschmcs...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);

		
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"clear_sch_mcs %s\"", NET_VAP_NAME, user_mac);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]", status);		
	WTSL_LOG_INFO(MODULE_NAME, "status:%s", status);
	pclose(fp);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
		return NULL;
	else
		return (void*)"success";
}


void* wtsl_core_get_adaptivemcsinfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "get adaptivemcs...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char res_type[18] = {0};
	char dir[18] = {0};
	char buffer[128] = {0};
	char cmd[128] ={0};
	char status[8] = {0};
	int mcs = 0;
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);

	cJSON* cjson_res_type = cJSON_GetObjectItem(root,"res_type");
	if(cjson_res_type != NULL)
		strcpy(res_type, cjson_res_type->valuestring);	

	cJSON* cjson_dir = cJSON_GetObjectItem(root,"dir");
	if(cjson_dir != NULL)
		strcpy(dir, cjson_dir->valuestring);		
		
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"get_sch_res_type_mcs %s %s %s\"", NET_VAP_NAME, user_mac, res_type, dir);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]]mcs=%d", status, &mcs);		
	WTSL_LOG_INFO(MODULE_NAME, "status:%s, mcs:%d", status, &mcs);
	pclose(fp);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
		return NULL;

	cJSON* reply_root = cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));		
	cJSON_AddItemToObject(reply_root, "res_type", cJSON_CreateString(res_type));
	cJSON_AddItemToObject(reply_root, "dir", cJSON_CreateString(dir));
	cJSON_AddItemToObject(reply_root, "mcs", cJSON_CreateNumber(mcs));
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	
	cJSON_Delete(reply_root);
	return json_str;
}

void* wtsl_core_get_mcsboundinfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "get mcsboundinfo...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	int low_bound = 0;
	int up_bound = 0;
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);	
		
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"get_mcs_bound %s\"", NET_VAP_NAME, user_mac);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]]low_bound=%d up_bound=%d", status, &low_bound, &up_bound);		
	WTSL_LOG_INFO(MODULE_NAME, "status:%s, low_bound:%d, up_bound:%d", status, low_bound, up_bound);
	pclose(fp);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
		return NULL;

	cJSON* reply_root = cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "low_bound", cJSON_CreateNumber(low_bound));		
	cJSON_AddItemToObject(reply_root, "up_bound", cJSON_CreateNumber(up_bound));
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	
	cJSON_Delete(reply_root);
	return json_str;
}

void* wtsl_core_set_mcsbound(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "set mcsbound...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	int low_bound = 0;
	int up_bound = 0;
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);

	cJSON* cjson_low_bound = cJSON_GetObjectItem(root,"low_bound");
	if(cjson_low_bound != NULL)
		low_bound = cjson_low_bound->valueint;	

	cJSON* cjson_up_bound = cJSON_GetObjectItem(root,"up_bound");
	if(cjson_up_bound != NULL)
		up_bound = cjson_up_bound->valueint;
		
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"set_mcs_bound %s %d %d\"", NET_VAP_NAME, user_mac, low_bound, up_bound);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	FILE* fp = popen(cmd, "r");
	fgets(buffer, sizeof(buffer), fp);
	sscanf(buffer,"%*[^[][%[^]]", status);	
	WTSL_LOG_INFO(MODULE_NAME, "status:%s", status);
    pclose(fp);	
	cJSON_Delete(root);

	if(strcmp(status, "SUCC") != 0)
		return NULL;

	return (void*)"success";
}

void* wtsl_core_do_throughput(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)size;
	(void)data;

	const char *script =
	"#!/bin/sh\n"
        "\n"
        "set_cpu_affinity() {\n"
        "    thread=$1\n"
        "    cpu_id=$2\n"
        "\n"
        "    pids=$(ps -o \"comm,pid\" | grep -e \"^$thread \" | awk '{print $2}')\n"
        "\n"
        "    if [ \"$cpu_id\" -gt 0 ]; then\n"
        "        cpu_mask=$((0x1 << cpu_id))\n"
        "    else\n"
        "        cpu_mask=$((0xF))\n"
        "    fi\n"
        "\n"
        "    for pid in $pids; do\n"
        "        taskset -p $cpu_mask $pid >/dev/null 2>&1\n"
        "    done\n"
        "}\n"
        "\n"
        "set_cpu_affinity \"hisi_rxdata\" 0\n"
        "set_cpu_affinity \"hisi_frw\" 1\n";
	const char* tmp_script = "/tmp/set_cpu_affinity.sh";
    FILE* fp = fopen(tmp_script, "w");
    if (!fp) {
        perror("fopen");
        return 1;
    }
    fwrite(script, strlen(script), 1, fp);
    fclose(fp);

    // 添加可执行权限
    chmod(tmp_script, 0755);

    // 执行脚本
    int ret = system(tmp_script);
    if (ret == -1) {
        perror("system");
    }

	system("sysctl -w net.core.rmem_max=2097152");
	system("sysctl -w net.core.wmem_max=2097152");
	system("sysctl -w net.core.netdev_max_backlog=4096");
	char cmd[128] = {0};
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"rx_thread_switch on\"", NET_VAP_NAME);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "iwpriv wt_vap0 alg \"set_mcs_bound 00:00:00:00:00:00 6 30\"");
	system(cmd);

	slb_set_tx_power(NET_VAP_NAME, 250);

	slb_ifconfig(NET_VAP_NAME, 0);
	
	slb_set_channel(NET_VAP_NAME, 1875);

	ret = slb_set_bw(NET_VAP_NAME, 80);
	if (ret == 0) {
		WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
		node_param->bw = 80;
	}
	ret = slb_set_tfc_bw(NET_VAP_NAME, 80);
	if (ret == 0) {
		WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
		node_param->tfc_bw = 80;
	}

	mib_params_t mib_params;
	mib_params.cp_type = 0;
	mib_params.symbol_type = 1;
	mib_params.sysmsg_period = 1;
	mib_params.s_cfg_idx = 2;
	slb_set_mib_params(NET_VAP_NAME, mib_params);
	slb_ifconfig(NET_VAP_NAME, 1);

	return (void*)"success";
}

void* wtsl_core_do_shortrange(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "node do shortrange test...");
    WTSLNode* pNode = (WTSLNode*)ph;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

    ret = wtsl_core_parse_json_do_shortrange(data);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
        return NULL;
    }

    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);

    return (void *)"do_shortrange_success";

}

void* wtsl_core_do_remoterange(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "node do remoterange test...");
    WTSLNode* pNode = (WTSLNode*)ph;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

    ret = wtsl_core_parse_json_do_remoterange(data);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
        return NULL;
    }

    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);

    return (void *)"do_remoterange_success";
}

void* wtsl_core_do_lowlatency(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	int ret = -1;
	    WTSL_LOG_INFO(MODULE_NAME, "node do lowlatency test...");
    WTSLNode* pNode = (WTSLNode*)ph;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

    ret = wtsl_core_parse_json_do_lowlatency(data);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
        return NULL;
    }

    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);

    return (void *)"do_lowlatency_success";
}

void* wtsl_core_do_lowpow(void *ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "node do lowpow test...");
    WTSLNode* pNode = (WTSLNode*)ph;
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
    if(data == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "data is NULL");
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

    ret = wtsl_core_parse_json_do_lowpow(data);
    if (ret != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
        return NULL;
    }

    //同步本地节点修改到链表中
    ret = update_wtsl_node_basicinfo(get_wtsl_core_node_list(),&global_node_info.node_info.basic_info);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] error",__FUNCTION__,__LINE__);
        return NULL;
    }
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);

    return (void *)"do_lowpow_success";
}

static char* wtsl_core_get_traceinfo_vip(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	const char* protocols[] = {"DHCP", "ARP_REQ", "ARP_RSP", "ICMP"};
	int host_tx_h2h[] = {0,0,0,0};
	int host_tx_h2d[] = {0,0,0,0};
	int dmac_tx_h2d[] = {0,0,0,0};
	int dmac_tx_sch_fill_tb[] = {0,0,0,0};
	int host_rx_d2h[] = {0,0,0,0};
	int host_rx_defrag_compl[] = {0,0,0,0};
	int host_rx_deliver[] = {0,0,0,0};


	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s", status);
			continue;
		}
			
		if(sscanf(buffer, "host_tx_h2h %d %d %d %d", &host_tx_h2h[0], &host_tx_h2h[1], &host_tx_h2h[2], &host_tx_h2h[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_h2h %d %d %d %d", host_tx_h2h[0], host_tx_h2h[1], host_tx_h2h[2], host_tx_h2h[3]);
			continue;
		}
						
		if(sscanf(buffer, "host_tx_h2d %d %d %d %d", &host_tx_h2d[0], &host_tx_h2d[1], &host_tx_h2d[2], &host_tx_h2d[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_h2d %d %d %d %d", host_tx_h2d[0], host_tx_h2d[1], host_tx_h2d[2], host_tx_h2d[3]);
			continue;
		}			
		if(sscanf(buffer, "dmac_tx_h2d %d %d %d %d", &dmac_tx_h2d[0], &dmac_tx_h2d[1], &dmac_tx_h2d[2], &dmac_tx_h2d[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_tx_h2d %d %d %d %d", dmac_tx_h2d[0], dmac_tx_h2d[1], dmac_tx_h2d[2], dmac_tx_h2d[3]);
			continue;
		}
		if(sscanf(buffer, "dmac_tx_sch_fill_tb %d %d %d %d", &dmac_tx_sch_fill_tb[0], &dmac_tx_sch_fill_tb[1], &dmac_tx_sch_fill_tb[2], &dmac_tx_sch_fill_tb[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_tx_sch_fill_tb %d %d %d %d", dmac_tx_sch_fill_tb[0], dmac_tx_sch_fill_tb[1], dmac_tx_sch_fill_tb[2], dmac_tx_sch_fill_tb[3]);
			continue;
		}

		if(sscanf(buffer, "host_rx_d2h %d %d %d %d", &host_rx_d2h[0], &host_rx_d2h[1], &host_rx_d2h[2], &host_rx_d2h[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_d2h %d %d %d %d", host_rx_d2h[0], host_rx_d2h[1], host_rx_d2h[2], host_rx_d2h[3]);
			continue;
		}

		if(sscanf(buffer, "host_rx_defrag_compl %d %d %d %d", &host_rx_defrag_compl[0], &host_rx_defrag_compl[1], &host_rx_defrag_compl[2], &host_rx_defrag_compl[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_defrag_compl %d %d %d %d", host_rx_defrag_compl[0], host_rx_defrag_compl[1], host_rx_defrag_compl[2], host_rx_defrag_compl[3]);
			continue;
		}		
		if(sscanf(buffer, "host_rx_deliver %d %d %d %d", &host_rx_deliver[0], &host_rx_deliver[1], &host_rx_deliver[2], &host_rx_deliver[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_deliver %d %d %d %d", host_rx_deliver[0], host_rx_deliver[1], host_rx_deliver[2], host_rx_deliver[3]);
			break;
		}

	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "protocols", cJSON_CreateStringArray(protocols, 4));

	cJSON* stats_root= cJSON_CreateObject();
	cJSON_AddItemToObject(stats_root, "host_tx_h2h", cJSON_CreateIntArray(host_tx_h2h, 4));
	cJSON_AddItemToObject(stats_root, "host_tx_h2d", cJSON_CreateIntArray(host_tx_h2d, 4));
	cJSON_AddItemToObject(stats_root, "dmac_tx_h2d", cJSON_CreateIntArray(dmac_tx_h2d, 4));
	cJSON_AddItemToObject(stats_root, "dmac_tx_sch_fill_tb", cJSON_CreateIntArray(dmac_tx_sch_fill_tb, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_d2h", cJSON_CreateIntArray(host_rx_d2h, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_defrag_compl", cJSON_CreateIntArray(host_rx_defrag_compl, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_deliver", cJSON_CreateIntArray(host_rx_deliver, 4));


	cJSON_AddItemToObject(reply_root, "stats", stats_root);
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}

static char* wtsl_core_get_traceinfo_throughput(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[16] = {0};
	char host_tx_h2h[16] = {0};
	char host_tx_h2d[16] = {0};
	char host_rx_d2h[16] = {0};
	char host_rx_defrag_compl[16] = {0};
	char host_rx_deliver[16] = {0};

	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s", status);
			continue;
		}
			
		if(sscanf(buffer, "host_tx_h2h %s", host_tx_h2h) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_h2h %s", host_tx_h2h);
			continue;
		}
						
		if(sscanf(buffer, "host_tx_h2d %s", host_tx_h2d) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_h2d %s", host_tx_h2d);
			continue;
		}		
		if(sscanf(buffer, "host_rx_d2h %s", host_rx_d2h) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_d2h %s", host_rx_d2h);
			continue;
		}	

		if(sscanf(buffer, "host_rx_defrag_compl %s", host_rx_defrag_compl) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_defrag_compl %s", host_rx_defrag_compl);
			continue;
		}


		if(sscanf(buffer, "host_rx_deliver %s", host_rx_deliver) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_deliver %s", host_rx_deliver);
			break;
		}
	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "host_tx_h2h", cJSON_CreateString(host_tx_h2h));
	cJSON_AddItemToObject(reply_root, "host_tx_h2d", cJSON_CreateString(host_tx_h2d));
	cJSON_AddItemToObject(reply_root, "host_rx_d2h", cJSON_CreateString(host_rx_d2h));
	cJSON_AddItemToObject(reply_root, "host_rx_defrag_compl", cJSON_CreateString(host_rx_defrag_compl));
	cJSON_AddItemToObject(reply_root, "host_rx_deliver", cJSON_CreateString(host_rx_deliver));

	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}

static char* wtsl_core_get_traceinfo_delay(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[16] = {0};
	int TX_HMAC_FC_Q[] = {0,0,0,0,0,0,0,0,0,0};
	int TX_DMAC_LC_Q[] = {0,0,0,0,0,0,0,0,0,0};
	int RX_HMAC_DEFRAG[] = {0,0,0,0,0,0,0,0,0,0};
	int RX_HMAC_REORDER[] = {0,0,0,0,0,0,0,0,0,0};
	const char* protocols[] = {"skb1", "skb2", "skb3", "skb4", "skb5", "skb6", "skb7", "skb8", "skb9", "skb10",};



	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s\n", status);
			continue;
		}
			
		if(sscanf(buffer, "TX_HMAC_FC_Q %d %d %d %d %d %d %d %d %d %d", &TX_HMAC_FC_Q[0], &TX_HMAC_FC_Q[1], &TX_HMAC_FC_Q[2], 
		&TX_HMAC_FC_Q[3], &TX_HMAC_FC_Q[4], &TX_HMAC_FC_Q[5],&TX_HMAC_FC_Q[6], &TX_HMAC_FC_Q[7], &TX_HMAC_FC_Q[8], 
		&TX_HMAC_FC_Q[9]) == 10)
		{
			WTSL_LOG_INFO(MODULE_NAME, "TX_HMAC_FC_Q %d %d %d %d %d %d %d %d %d %d", TX_HMAC_FC_Q[0], TX_HMAC_FC_Q[1], TX_HMAC_FC_Q[2], 
		TX_HMAC_FC_Q[3], TX_HMAC_FC_Q[4], TX_HMAC_FC_Q[5],TX_HMAC_FC_Q[6], TX_HMAC_FC_Q[7], TX_HMAC_FC_Q[8], 
		TX_HMAC_FC_Q[9]);
			continue;
		}
						
		if(sscanf(buffer, "TX_DMAC_LC_Q %d %d %d %d %d %d %d %d %d %d", &TX_DMAC_LC_Q[0], &TX_DMAC_LC_Q[1], &TX_DMAC_LC_Q[2], 
		&TX_DMAC_LC_Q[3], &TX_DMAC_LC_Q[4], &TX_DMAC_LC_Q[5],&TX_DMAC_LC_Q[6], &TX_DMAC_LC_Q[7], &TX_DMAC_LC_Q[8], 
		&TX_DMAC_LC_Q[9]) == 10)
		{
			WTSL_LOG_INFO(MODULE_NAME, "TX_DMAC_LC_Q %d %d %d %d %d %d %d %d %d %d", TX_DMAC_LC_Q[0], TX_DMAC_LC_Q[1], TX_DMAC_LC_Q[2], 
		TX_DMAC_LC_Q[3], TX_DMAC_LC_Q[4], TX_DMAC_LC_Q[5],TX_DMAC_LC_Q[6], TX_DMAC_LC_Q[7], TX_DMAC_LC_Q[8], 
		TX_DMAC_LC_Q[9]);
			continue;
		}		
		if(sscanf(buffer, "RX_HMAC_DEFRAG %d %d %d %d %d %d %d %d %d %d", &RX_HMAC_DEFRAG[0], &RX_HMAC_DEFRAG[1], &RX_HMAC_DEFRAG[2], 
		&RX_HMAC_DEFRAG[3], &RX_HMAC_DEFRAG[4], &RX_HMAC_DEFRAG[5],&RX_HMAC_DEFRAG[6], &RX_HMAC_DEFRAG[7], &RX_HMAC_DEFRAG[8], 
		&RX_HMAC_DEFRAG[9]) == 10)
		{
			WTSL_LOG_INFO(MODULE_NAME, "RX_HMAC_DEFRAG %d %d %d %d %d %d %d %d %d %d", RX_HMAC_DEFRAG[0], RX_HMAC_DEFRAG[1], RX_HMAC_DEFRAG[2], 
		RX_HMAC_DEFRAG[3], RX_HMAC_DEFRAG[4], RX_HMAC_DEFRAG[5],RX_HMAC_DEFRAG[6], RX_HMAC_DEFRAG[7], RX_HMAC_DEFRAG[8], 
		RX_HMAC_DEFRAG[9]);
			continue;
		}


		if(sscanf(buffer, "RX_HMAC_REORDER %d %d %d %d %d %d %d %d %d %d", &RX_HMAC_REORDER[0], &RX_HMAC_REORDER[1], &RX_HMAC_REORDER[2], 
		&RX_HMAC_REORDER[3], &RX_HMAC_REORDER[4], &RX_HMAC_REORDER[5],&RX_HMAC_REORDER[6], &RX_HMAC_REORDER[7], &RX_HMAC_REORDER[8], 
		&RX_HMAC_REORDER[9]) == 10)
		{
			WTSL_LOG_INFO(MODULE_NAME, "RX_HMAC_REORDER %d %d %d %d %d %d %d %d %d %d", RX_HMAC_REORDER[0], RX_HMAC_REORDER[1], RX_HMAC_REORDER[2], 
		RX_HMAC_REORDER[3], RX_HMAC_REORDER[4], RX_HMAC_REORDER[5],RX_HMAC_REORDER[6], RX_HMAC_REORDER[7], RX_HMAC_REORDER[8], 
		RX_HMAC_REORDER[9]);
			break;
		}
	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "protocols", cJSON_CreateStringArray(protocols, 10));

	cJSON* stats_root= cJSON_CreateObject();	
	cJSON_AddItemToObject(stats_root, "TX_HMAC_FC_Q", cJSON_CreateIntArray(TX_HMAC_FC_Q, 10));
	cJSON_AddItemToObject(stats_root, "TX_DMAC_LC_Q", cJSON_CreateIntArray(TX_DMAC_LC_Q, 10));
	cJSON_AddItemToObject(stats_root, "RX_HMAC_DEFRAG", cJSON_CreateIntArray(RX_HMAC_DEFRAG, 10));
	cJSON_AddItemToObject(stats_root, "RX_HMAC_REORDER", cJSON_CreateIntArray(RX_HMAC_REORDER, 10));
	
	cJSON_AddItemToObject(reply_root, "stats", stats_root);
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}

static char* wtsl_core_get_traceinfo_drop(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	const char* protocols[] = {"drop_cnt", "rough_total", "ratio", "vip_drop_cnt"};
	int host_tx_fc_drop[] = {0,0,0,0};
	int host_tx_retx_drop[] = {0,0,0,0};
	int host_tx_a2h_drop[] = {0,0,0,0};
	int dmac_tx_distribute_fail[] = {0,0,0,0};
	int dmac_rx_parse_mac_hdr[] = {0,0,0,0};
	int dmac_rx_netbuf_alloc_fail[] = {0,0,0,0};
	int dmac_rx_gsec_fail[] = {0,0,0,0};
	int dmac_rx_gsec_err[] = {0,0,0,0};
	int host_rx_defrag_udpate[] = {0,0,0,0};
	int host_rx_defrag_flush[] = {0,0,0,0};
	int host_rx_reorder_lt_submit[] = {0,0,0,0};
	int host_rx_reorder_dup[] = {0,0,0,0};


	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s", status);
			continue;
		}
			
		if(sscanf(buffer, "host_tx_fc_drop %d %d %d %d", &host_tx_fc_drop[0], &host_tx_fc_drop[1], &host_tx_fc_drop[2], &host_tx_fc_drop[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_fc_drop %d %d %d %d", host_tx_fc_drop[0], host_tx_fc_drop[1], host_tx_fc_drop[2], host_tx_fc_drop[3]);
			continue;
		}
						
		if(sscanf(buffer, "host_tx_retx_drop %d %d %d %d", &host_tx_retx_drop[0], &host_tx_retx_drop[1], &host_tx_retx_drop[2], &host_tx_retx_drop[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_retx_drop %d %d %d %d", host_tx_retx_drop[0], host_tx_retx_drop[1], host_tx_retx_drop[2], host_tx_retx_drop[3]);
			continue;
		}			
		if(sscanf(buffer, "host_tx_a2h_drop %d %d %d %d", &host_tx_a2h_drop[0], &host_tx_a2h_drop[1], &host_tx_a2h_drop[2], &host_tx_a2h_drop[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_a2h_drop %d %d %d %d", host_tx_a2h_drop[0], host_tx_a2h_drop[1], host_tx_a2h_drop[2], host_tx_a2h_drop[3]);
			continue;
		}
		if(sscanf(buffer, "dmac_tx_distribute_fail %d %d %d %d", &dmac_tx_distribute_fail[0], &dmac_tx_distribute_fail[1], &dmac_tx_distribute_fail[2], &dmac_tx_distribute_fail[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_tx_distribute_fail %d %d %d %d", dmac_tx_distribute_fail[0], dmac_tx_distribute_fail[1], dmac_tx_distribute_fail[2], dmac_tx_distribute_fail[3]);
			continue;
		}

		if(sscanf(buffer, "dmac_rx_parse_mac_hdr %d %d %d %d", &dmac_rx_parse_mac_hdr[0], &dmac_rx_parse_mac_hdr[1], &dmac_rx_parse_mac_hdr[2], &dmac_rx_parse_mac_hdr[3]) == 4)
        {
			WTSL_LOG_INFO(MODULE_NAME, "dmac_rx_parse_mac_hdr %d %d %d %d", dmac_rx_parse_mac_hdr[0], dmac_rx_parse_mac_hdr[1], dmac_rx_parse_mac_hdr[2], dmac_rx_parse_mac_hdr[3]);
			continue;
		}

		if(sscanf(buffer, "dmac_rx_netbuf_alloc_fail %d %d %d %d", &dmac_rx_netbuf_alloc_fail[0], &dmac_rx_netbuf_alloc_fail[1], &dmac_rx_netbuf_alloc_fail[2], &dmac_rx_netbuf_alloc_fail[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_rx_netbuf_alloc_fail %d %d %d %d", dmac_rx_netbuf_alloc_fail[0], dmac_rx_netbuf_alloc_fail[1], dmac_rx_netbuf_alloc_fail[2], dmac_rx_netbuf_alloc_fail[3]);
			continue;
		}
		
		if(sscanf(buffer, "dmac_rx_gsec_fail %d %d %d %d", &dmac_rx_gsec_fail[0], &dmac_rx_gsec_fail[1], &dmac_rx_gsec_fail[2], &dmac_rx_gsec_fail[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_rx_gsec_fail %d %d %d %d", dmac_rx_gsec_fail[0], dmac_rx_gsec_fail[1], dmac_rx_gsec_fail[2], dmac_rx_gsec_fail[3]);
			continue;
		}

		if(sscanf(buffer, "dmac_rx_gsec_err %d %d %d %d", &dmac_rx_gsec_err[0], &dmac_rx_gsec_err[1], &dmac_rx_gsec_err[2], &dmac_rx_gsec_err[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "dmac_rx_gsec_err %d %d %d %d", dmac_rx_gsec_err[0], dmac_rx_gsec_err[1], dmac_rx_gsec_err[2], dmac_rx_gsec_err[3]);
			continue;
		}			
		if(sscanf(buffer, "host_rx_defrag_udpate %d %d %d %d", &host_rx_defrag_udpate[0], &host_rx_defrag_udpate[1], &host_rx_defrag_udpate[2], &host_rx_defrag_udpate[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_defrag_udpate %d %d %d %d", host_rx_defrag_udpate[0], host_rx_defrag_udpate[1], host_rx_defrag_udpate[2], host_rx_defrag_udpate[3]);
			continue;
		}
		if(sscanf(buffer, "host_rx_defrag_flush %d %d %d %d", &host_rx_defrag_flush[0], &host_rx_defrag_flush[1], &host_rx_defrag_flush[2], &host_rx_defrag_flush[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_defrag_flush %d %d %d %d", host_rx_defrag_flush[0], host_rx_defrag_flush[1], host_rx_defrag_flush[2], host_rx_defrag_flush[3]);
			continue;
		}

		if(sscanf(buffer, "host_rx_reorder_lt_submit %d %d %d %d", &host_rx_reorder_lt_submit[0], &host_rx_reorder_lt_submit[1], &host_rx_reorder_lt_submit[2], &host_rx_reorder_lt_submit[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_reorder_lt_submit %d %d %d %d", host_rx_reorder_lt_submit[0], host_rx_reorder_lt_submit[1], host_rx_reorder_lt_submit[2], host_rx_reorder_lt_submit[3]);
			continue;
		}

		if(sscanf(buffer, "host_rx_reorder_dup %d %d %d %d", &host_rx_reorder_dup[0], &host_rx_reorder_dup[1], &host_rx_reorder_dup[2], &host_rx_reorder_dup[3]) == 4)
		{
			WTSL_LOG_INFO(MODULE_NAME, "host_rx_reorder_dup %d %d %d %d", host_rx_reorder_dup[0], host_rx_reorder_dup[1], host_rx_reorder_dup[2], host_rx_reorder_dup[3]);
			break;
		}		
		

	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "protocols", cJSON_CreateStringArray(protocols, 4));

	cJSON* stats_root= cJSON_CreateObject();
	cJSON_AddItemToObject(stats_root, "host_tx_fc_drop", cJSON_CreateIntArray(host_tx_fc_drop, 4));
	cJSON_AddItemToObject(stats_root, "host_tx_retx_drop", cJSON_CreateIntArray(host_tx_retx_drop, 4));
	cJSON_AddItemToObject(stats_root, "host_tx_a2h_drop", cJSON_CreateIntArray(host_tx_a2h_drop, 4));
	cJSON_AddItemToObject(stats_root, "dmac_tx_distribute_fail", cJSON_CreateIntArray(dmac_tx_distribute_fail, 4));
	cJSON_AddItemToObject(stats_root, "dmac_rx_parse_mac_hdr", cJSON_CreateIntArray(dmac_rx_parse_mac_hdr, 4));
	cJSON_AddItemToObject(stats_root, "dmac_rx_netbuf_alloc_fail", cJSON_CreateIntArray(dmac_rx_netbuf_alloc_fail, 4));
	cJSON_AddItemToObject(stats_root, "dmac_rx_gsec_fail", cJSON_CreateIntArray(dmac_rx_gsec_fail, 4));
	cJSON_AddItemToObject(stats_root, "dmac_rx_gsec_err", cJSON_CreateIntArray(dmac_rx_gsec_err, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_defrag_udpate", cJSON_CreateIntArray(host_rx_defrag_udpate, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_defrag_flush", cJSON_CreateIntArray(host_rx_defrag_flush, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_reorder_lt_submit", cJSON_CreateIntArray(host_rx_reorder_lt_submit, 4));
	cJSON_AddItemToObject(stats_root, "host_rx_reorder_dup", cJSON_CreateIntArray(host_rx_reorder_dup, 4));


	cJSON_AddItemToObject(reply_root, "stats", stats_root);
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}

static char* wtsl_core_get_traceinfo_mcs(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	const char* protocols[] = {"total", "0-9", "10-16", "17-22", "23-27", "28-31"};
	int first_trans[] = {0,0,0,0,0,0};
	int retrans[] = {0,0,0,0,0,0};
    int msec = 0;


    snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s", status);
			continue;
		}
		
		if(sscanf(buffer, "msec:%d", &msec) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "msec %s", msec);
			continue;
		}
		
		if(sscanf(buffer, "first_trans %d %d %d %d %d %d", &first_trans[0], &first_trans[1], &first_trans[2], &first_trans[3], &first_trans[4], &first_trans[5]) == 6)
		{
			WTSL_LOG_INFO(MODULE_NAME, "first_trans %d %d %d %d", first_trans[0], first_trans[1], first_trans[2], first_trans[3], first_trans[4], first_trans[5]);
			continue;
		}
						
		if(sscanf(buffer, "retrans %d %d %d %d %d %d", &retrans[0], &retrans[1], &retrans[2], &retrans[3], &retrans[4], &retrans[5]) == 6)
		{
			WTSL_LOG_INFO(MODULE_NAME, "retrans %d %d %d %d", retrans[0], retrans[1], retrans[2], retrans[3], retrans[4], retrans[5]);
			break;
		}				
		

	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "msec", cJSON_CreateNumber(msec));
	cJSON_AddItemToObject(reply_root, "protocols", cJSON_CreateStringArray(protocols, 6));

	cJSON* stats_root= cJSON_CreateObject();
	cJSON_AddItemToObject(stats_root, "first_trans", cJSON_CreateIntArray(first_trans, 6));
	cJSON_AddItemToObject(stats_root, "retrans", cJSON_CreateIntArray(retrans, 6));

	cJSON_AddItemToObject(reply_root, "stats", stats_root);
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}

static char* wtsl_core_get_traceinfo_user_access(char* user_mac, char* feature)
{
	char buffer[128] = {0};
	char cmd[64] ={0};
	char status[8] = {0};
	const char* protocols[] = {"fail_0", "fail_1"};
	int host_tx_XrcSetup[] = {0,0};
	int dmac_msg34_tx_complete[] = {0,0};
	int host_sync_phyid[] = {0,0};
	int host_tx_SecCtxReq[] = {0,0};
	int host_sync_sec_key[] = {0,0};
	int host_tx_AssocSetup[] = {0,0};
	int host_tx_XrcReconf[] = {0,0};
	int host_retx_lce1[] = {0,0};
	int dmac_lcl_tx_cnt[] = {0,0};
	int host_usp_fail[] = {0,0};
	int host_sec_fail[] = {0,0};
	int dmac_decrypt_fail[] = {0,0};
	


	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"trace %s %s\"", NET_VAP_NAME, feature, user_mac);
	FILE* fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		WTSL_LOG_INFO(MODULE_NAME, "buffer: %s", buffer);
		if(sscanf(buffer, "%*[^[][%[^]]", status) == 1)
		{
			WTSL_LOG_INFO(MODULE_NAME, "status %s", status);
			continue;
		}
		
		if(strstr(buffer, "host_tx_XrcSetup"))
		{
			sscanf(buffer, "host_tx_XrcSetup %d %d", &host_tx_XrcSetup[0], &host_tx_XrcSetup[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_XrcSetup %d %d", host_tx_XrcSetup[0], host_tx_XrcSetup[1]);
			continue;
		}
					
		if(strstr(buffer, "dmac_msg34_tx_complete"))
		{
			sscanf(buffer, "dmac_msg34_tx_complete %d %d", &dmac_msg34_tx_complete[0], &dmac_msg34_tx_complete[1]);
			WTSL_LOG_INFO(MODULE_NAME, "dmac_msg34_tx_complete %d %d", dmac_msg34_tx_complete[0], dmac_msg34_tx_complete[1]);
			continue;
		}

		if(strstr(buffer, "host_sync_phyid"))
		{
			sscanf(buffer, "host_sync_phyid %d %d", &host_sync_phyid[0], &host_sync_phyid[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_sync_phyid %d %d", host_sync_phyid[0], host_sync_phyid[1]);
			continue;
		}		

		if(strstr(buffer, "host_tx_SecCtxReq"))
		{
			sscanf(buffer, "host_tx_SecCtxReq %d %d", &host_tx_SecCtxReq[0], &host_tx_SecCtxReq[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_SecCtxReq %d %d", host_tx_SecCtxReq[0], host_tx_SecCtxReq[1]);
			continue;
		}
					
		if(strstr(buffer, "host_sync_sec_key"))
		{
			sscanf(buffer, "host_sync_sec_key %d %d", &host_sync_sec_key[0], &host_sync_sec_key[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_sync_sec_key %d %d", host_sync_sec_key[0], host_sync_sec_key[1]);
			continue;
		}

		if(strstr(buffer, "host_tx_AssocSetup"))
		{
			sscanf(buffer, "host_tx_AssocSetup %d %d", &host_tx_AssocSetup[0], &host_tx_AssocSetup[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_AssocSetup %d %d", host_tx_AssocSetup[0], host_tx_AssocSetup[1]);
			continue;
		}		

		if(strstr(buffer, "host_tx_XrcReconf"))
		{
			sscanf(buffer, "host_tx_XrcReconf %d %d", &host_tx_XrcReconf[0], &host_tx_XrcReconf[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_tx_XrcReconf %d %d", host_tx_XrcReconf[0], host_tx_XrcReconf[1]);
			continue;
		}
					
		if(strstr(buffer, "host_retx_lce1"))
		{
			sscanf(buffer, "host_retx_lce1 %d %d", &host_retx_lce1[0], &host_retx_lce1[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_retx_lce1 %d %d", host_retx_lce1[0], host_retx_lce1[1]);
			continue;
		}

		if(strstr(buffer, "dmac_lcl_tx_cnt"))
		{
			sscanf(buffer, "dmac_lcl_tx_cnt %d %d", &dmac_lcl_tx_cnt[0], &dmac_lcl_tx_cnt[1]);
			WTSL_LOG_INFO(MODULE_NAME, "dmac_lcl_tx_cnt %d %d", dmac_lcl_tx_cnt[0], dmac_lcl_tx_cnt[1]);
			continue;
		}		

		if(strstr(buffer, "host_usp_fail"))
		{
			sscanf(buffer, "host_usp_fail %d %d", &host_usp_fail[0], &host_usp_fail[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_usp_fail %d %d", host_usp_fail[0], host_usp_fail[1]);
			continue;
		}
					
		if(strstr(buffer, "host_sec_fail"))
		{
			sscanf(buffer, "host_sec_fail %d %d", &host_sec_fail[0], &host_sec_fail[1]);
			WTSL_LOG_INFO(MODULE_NAME, "host_sec_fail %d %d", host_sec_fail[0], host_sec_fail[1]);
			continue;
		}

		if(strstr(buffer, "dmac_decrypt_fail"))
		{
			sscanf(buffer, "dmac_decrypt_fail %d %d", &dmac_decrypt_fail[0], &dmac_decrypt_fail[1]);
			WTSL_LOG_INFO(MODULE_NAME, "dmac_decrypt_fail %d %d", dmac_decrypt_fail[0], dmac_decrypt_fail[1]);
			break;
		}		

	}
	pclose(fp);

	cJSON* reply_root= cJSON_CreateObject();
	cJSON_AddItemToObject(reply_root, "role", cJSON_CreateNumber(global_node_info.node_info.basic_info.type));
	cJSON_AddItemToObject(reply_root, "feature", cJSON_CreateString(feature));
	cJSON_AddItemToObject(reply_root, "user_mac", cJSON_CreateString(user_mac));
	cJSON_AddItemToObject(reply_root, "protocols", cJSON_CreateStringArray(protocols, 2));

	cJSON* stats_root= cJSON_CreateObject();
	cJSON_AddItemToObject(stats_root, "host_tx_XrcSetup", cJSON_CreateIntArray(host_tx_XrcSetup, 2));
	cJSON_AddItemToObject(stats_root, "dmac_msg34_tx_complete", cJSON_CreateIntArray(dmac_msg34_tx_complete, 2));
	cJSON_AddItemToObject(stats_root, "host_sync_phyid", cJSON_CreateIntArray(host_sync_phyid, 2));
	cJSON_AddItemToObject(stats_root, "host_tx_SecCtxReq", cJSON_CreateIntArray(host_tx_SecCtxReq, 2));
	cJSON_AddItemToObject(stats_root, "host_sync_sec_key", cJSON_CreateIntArray(host_sync_sec_key, 2));
	cJSON_AddItemToObject(stats_root, "host_tx_AssocSetup", cJSON_CreateIntArray(host_tx_AssocSetup, 2));
	cJSON_AddItemToObject(stats_root, "host_tx_XrcReconf", cJSON_CreateIntArray(host_tx_XrcReconf, 2));
	cJSON_AddItemToObject(stats_root, "host_retx_lce1", cJSON_CreateIntArray(host_retx_lce1, 2));
	cJSON_AddItemToObject(stats_root, "dmac_lcl_tx_cnt", cJSON_CreateIntArray(dmac_lcl_tx_cnt, 2));
	cJSON_AddItemToObject(stats_root, "host_usp_fail", cJSON_CreateIntArray(host_usp_fail, 2));
	cJSON_AddItemToObject(stats_root, "host_sec_fail", cJSON_CreateIntArray(host_sec_fail, 2));
	cJSON_AddItemToObject(stats_root, "dmac_decrypt_fail", cJSON_CreateIntArray(dmac_decrypt_fail, 2));	

	cJSON_AddItemToObject(reply_root, "stats", stats_root);
	char* json_str = cJSON_Print(reply_root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(reply_root);
		return NULL;
	}	
	cJSON_Delete(reply_root);
	return json_str;
}



void* wtsl_core_get_traceinfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
    WTSLNode* pNode = (WTSLNode* )ph;
    WTSL_LOG_INFO(MODULE_NAME, "get traceinfo...");
    if(pNode == NULL){
        WTSL_LOG_ERROR(MODULE_NAME, "Node pointer is NULL");
        return NULL;
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char user_mac[18] = {0};
	char feature[18] = {0};
	
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"user_mac");
	if(cjson_mac != NULL)
		strcpy(user_mac, cjson_mac->valuestring);

	cJSON* cjson_feature = cJSON_GetObjectItem(root,"feature");
	if(cjson_feature != NULL)
		strcpy(feature, cjson_feature->valuestring);		
	
	cJSON_Delete(root);
	
	if(strncmp("vip", feature, 3) == 0)
		return wtsl_core_get_traceinfo_vip(user_mac, feature);
	else if(strncmp("throughput", feature, 10) == 0)
		return wtsl_core_get_traceinfo_throughput(user_mac, feature);
	else if(strncmp("delay", feature, 5) == 0)
		return wtsl_core_get_traceinfo_delay(user_mac, feature);
	else if(strncmp("drop", feature, 4) == 0)
		return wtsl_core_get_traceinfo_drop(user_mac, feature);
	else if(strncmp("mcs", feature, 3) == 0)
		return wtsl_core_get_traceinfo_mcs(user_mac, feature);
	else if(strncmp("user_access", feature, 11) == 0)
		return wtsl_core_get_traceinfo_user_access(user_mac, feature);
	else
		WTSL_LOG_ERROR(MODULE_NAME, "feature is not valid data");

	return NULL;
}


void* wtsl_core_sle_start_scan(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)data;
	(void)size;
	
	scan_num = 0;
	sle_start_scan();
	sleep(1);
	sle_stop_seek();
	
	return wtsl_core_sle_show_bss(ph, data, size, ctx);
}

void* wtsl_core_sle_show_bss(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)data;
	(void)size;
	
	cJSON* root= cJSON_CreateArray();
	for(int i = 0; i < scan_num; i++)
	{
		printf("sle show bss index:%d rssi:%d, mac:%x:%x:%x:%x:%x:%x\n\n", i, seek_result_info[i].rssi, seek_result_info[i].addr.addr[0],
		seek_result_info[i].addr.addr[1], seek_result_info[i].addr.addr[2], seek_result_info[i].addr.addr[3], seek_result_info[i].addr.addr[4],
		seek_result_info[i].addr.addr[5]);
		cJSON* temp_root= cJSON_CreateObject();
		char tmp[18] = {0};
		snprintf(tmp, sizeof(tmp), "%02x:%02x:%02x:%02x:%02x:%02x", seek_result_info[i].addr.addr[0], seek_result_info[i].addr.addr[1], seek_result_info[i].addr.addr[2],
		seek_result_info[i].addr.addr[3], seek_result_info[i].addr.addr[4], seek_result_info[i].addr.addr[5]);
		cJSON_AddItemToObject(temp_root, "rssi", cJSON_CreateNumber(seek_result_info[i].rssi));
		cJSON_AddItemToObject(temp_root, "mac", cJSON_CreateString(tmp));
		cJSON_AddItemToArray(root, temp_root);		
	}
	
	char* json_str = cJSON_Print(root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(root);
		return NULL;
	}	
	
	cJSON_Delete(root);
	return json_str;
}

void* wtsl_core_sle_connect(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)ctx;
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	char mac[18] = {0};
	
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_mac = cJSON_GetObjectItem(root,"mac");
	if(cjson_mac != NULL)
		strcpy(mac, cjson_mac->valuestring);

	sle_addr_t sle_addr;
	sle_addr.type = 0;
	
	//strncpy(sle_addr.addr, mac, sizeof(sle_addr.addr));
	sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", &sle_addr.addr[0], &sle_addr.addr[1], &sle_addr.addr[2], &sle_addr.addr[3], &sle_addr.addr[4], &sle_addr.addr[5]);
	printf("connect mac: %x:%x:%x:%x:%x:%x\n", sle_addr.addr[0], sle_addr.addr[1], sle_addr.addr[2], sle_addr.addr[3], sle_addr.addr[4], sle_addr.addr[5]);
	sle_create_connection(&sle_addr);

	//connected_num++;
	cJSON_Delete(root);
	return (void*)"success";
}

void* wtsl_core_sle_conninfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)ctx;
	(void)data;
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	cJSON* array = cJSON_CreateArray();
	for(int i = 0; i < SLE_MAX_CONN; i++)
	{		
		if(connect_manage[i].server_info.state == SLE_ACB_STATE_CONNECTED)
		{
			cJSON* root = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "mac", cJSON_CreateString(connect_manage[i].mac));
			cJSON_AddItemToObject(root, "conn_id", cJSON_CreateNumber(connect_manage[i].server_info.conn_id));
			cJSON_AddItemToArray(array, root);
		}		
	}

	char* json_str = cJSON_Print(array);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(array);
		return NULL;
	}
	cJSON_Delete(array);
	return json_str;
}

void* wtsl_core_get_sle_basicinfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)ctx;
	(void)data;
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);

	cJSON* root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "sle_type", cJSON_CreateNumber(global_node_info.node_info.basic_info.sle_type));
	cJSON_AddItemToObject(root, "sle_name", cJSON_CreateString(global_node_info.node_info.basic_info.sle_name));
	cJSON_AddItemToObject(root, "mac", cJSON_CreateString(global_node_info.node_info.basic_info.mac));

	char* json_str = cJSON_Print(root);
	if(!json_str)
	{
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(root);
		return NULL;
	}
	cJSON_Delete(root);
	return json_str;
}

void* wtsl_core_set_sle_basicinfo(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)ctx;
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);
	
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}

	cJSON* sle_name = cJSON_GetObjectItem(root,"sle_name");
	if(sle_name != NULL){
		strcpy(global_node_info.node_info.basic_info.sle_name, sle_name->valuestring);
	}
	config_set("SLE_NAME", global_node_info.node_info.basic_info.sle_name);
	
	cJSON* sle_type = cJSON_GetObjectItem(root,"sle_type");
	if(sle_type != NULL) {
		if (global_node_info.node_info.basic_info.sle_type == sle_type->valueint)
		{
			config_set_int("SLE_TYPE", sle_type->valueint);
			global_node_info.node_info.basic_info.sle_type = sle_type->valueint;
		} else {
			WTSL_LOG_INFO(MODULE_NAME, "SLE G/T online switching is not supported, and the device will restart after setting");
			config_set_int("SLE_TYPE", sle_type->valueint);
			global_node_info.node_info.basic_info.sle_type = sle_type->valueint;
			int ret = system("reboot");
			if(ret != 0){
				WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
			}
		}
	}
	cJSON_Delete(root);
	return (void*)"success";
}

void* wtsl_core_set_announce_id(void* ph, void *data, unsigned int size, UserContext *ctx)
{
	(void)ctx;
	(void)ph;
	(void)ctx;
	WTSL_LOG_DEBUG(MODULE_NAME, "Received data to set,size:%d,data:%s",size,(char *)data);
	
	cJSON *root =cJSON_Parse(data);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return NULL;
	}
	
	cJSON* cjson_announce_id = cJSON_GetObjectItemCaseSensitive(root, "announce_id");
	if(cjson_announce_id != NULL){
		announce_id_num = cJSON_GetArraySize(cjson_announce_id);
		printf("announce id array num: %d\n", announce_id_num);
		for(int i = 0; i < announce_id_num; i++)
		{
			cJSON* item = cJSON_GetArrayItem(cjson_announce_id, i);
			announce_id[i] = item->valueint;
			printf("announ_id[%d]: %d\n", i, announce_id[i]);
		}
	}		
	
	cJSON_Delete(root);
	return (void*)"success";
}