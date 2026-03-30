#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/reboot.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include "wtsl_core_api.h"
#include "wtsl_core_dataparser.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_slb_interface.h"
#include "wtsl_core_dataparser_ota.h"
#include "wtsl_cfg_manager.h"
#include "wtsl_core_node_manager.h"
#include "wtsl_core_node_list.h"

// static node_params_t node_param;
// extern int client_count;
// extern Client clients[];
extern pthread_mutex_t scan_mutex;
extern SPLINK_INFO global_node_info;

#define MODULE_NAME "date_parse"
// WTSLNodeInfo g_server_node_info = {0};
/**
* 读取系统统计信息
*
*/
int get_cpu_usage_stats(double *overall_usage, double core_usage[4])
{
	FILE *fp = popen("mpstat -P ALL 1 1", "r");
	if (!fp) {
		return -1;
	}

	char line[256] = {0};
	int found_average = 0;

	// 初始化使用率为0
	*overall_usage = 0.0;
	for (int i = 0; i < 4; i++) {
		core_usage[i] = 0.0;
	}

	// 查找平均值部分
	while (fgets(line, sizeof(line), fp)) {
		// 查找"Average: "开头的行
		if (strstr(line, "Average:")) {
			found_average = 1;
			break;
		}
	}

	if (!found_average) {
		pclose(fp);
		return -1;
	}

	// 解析平均值部分
	while (fgets(line, sizeof(line), fp)) {
		// 解析总体CPU使用率（all）
		if (strstr(line, "Average:") && strstr(line, "all")) {
			double usr, nice, sys, iowait, irq, soft, steal, guest, idle;
			if (sscanf(line, "Average: all %lf %lf %lf %lf %lf %lf %lf %lf %lf",
						&usr, &nice, &sys, &iowait, &irq, &soft, &steal, &guest, &idle) >= 9) {
				*overall_usage = 100.0 - idle;
			}
		}
		// 解析各个CPU核心使用率
		else if (strstr(line, "Average:") &&
				(strstr(line, " 0") || strstr(line, " 1") || strstr(line, " 2") || strstr(line, " 3"))) {
			int core_id;
			double usr, nice, sys, iowait, irq, soft, steal, guest, idle;

			if (sscanf(line, "Average: %d %lf %lf %lf %lf %lf %lf %lf %lf %lf",
						&core_id, &usr, &nice, &sys, &iowait, &irq, &soft, &steal, &guest, &idle) >= 10) {
				if (core_id >= 0 && core_id < 4) {
					core_usage[core_id] = 100.0 - idle;
				}
			}
		}
	}

	pclose(fp);
	return 0;
}

// 获取系统负载平均值
int get_load_average(double *load1, double *load5, double*load15)
{
	FILE *fp = fopen("/proc/loadavg", "r");
	if (!fp)
	{
		return -1;
	}

	if (fscanf(fp, "%lf %lf %lf", load1, load5, load15) != 3) {
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

int get_memory_usage(unsigned long *total, unsigned long *used, unsigned long *free, double *usage_percent)
{
	FILE *fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		return -1;
	}

	char line[256] = {0};
	unsigned long mem_total = 0;
	unsigned long mem_free = 0;
	unsigned long buffers = 0;
	unsigned long cached = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "MemTotal:", 9) == 0) {
			sscanf(line + 9, "%lu", &mem_total);
		} else if (strncmp(line, "MemFree:", 8) == 0) {
			sscanf(line + 8, "%lu", &mem_free);
		} else if (strncmp(line, "Buffers:", 8) == 0) {
			sscanf(line + 8, "%lu", &buffers);
		} else if (strncmp(line, "Cached:", 7) == 0) {
			sscanf(line + 7, "%lu", &cached);
		}
	}

	fclose(fp);

	if (mem_total == 0) {
		return -1;
	}

	// 计算实际使用的内存（不包括buffers和cached）
	unsigned long actual_used = mem_total - mem_free - buffers - cached;

	*total = mem_total * 1024;
	*used = actual_used * 1024;
	*free = (mem_free + buffers + cached) * 1024; // 可用内存包括free+buffers+cached
	*usage_percent = (double)actual_used / mem_total * 100.0;

	return 0;
}

// 获取磁盘使用率
int get_disk_usage(unsigned long *total, unsigned long *used, unsigned long *free, double *usage_percent)
{
	struct statvfs buf;

	if (statvfs("/", &buf) != 0) {
		return -1;
	}

	unsigned long block_size = buf.f_frsize;
	*total = buf.f_blocks * block_size;
	*free = buf.f_bfree * block_size;
	*used = *total - *free;
	*usage_percent = (double)(*used) / (*total) * 100.0;

	return 0;
}

// 格式化字节大小为可读格式
void format_bytes(unsigned long bytes, char *buffer, size_t buffer_size)
{
	const char *units[] = {"B", "KB", "M", "GB", "TB"};
	int unit_index = 0;
	double size = bytes;

	while (size >= 1024.0 && unit_index < 4) {
		size /= 1024.0;
		unit_index++;
	}

	snprintf(buffer, buffer_size, "%.2f %s", size, units[unit_index]);
}

//解析字符串数组
static void parse_string_array(cJSON *array,char **str_arr) {
    if (array == NULL || array->type != cJSON_Array) {
        WTSL_LOG_ERROR(MODULE_NAME, "Invalid string array");
        return;
    }
    
    int size = cJSON_GetArraySize(array);
    //WTSL_LOG_INFO(MODULE_NAME, "String array (size: %d):", size);
    
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item != NULL && item->type == cJSON_String) {
            //WTSL_LOG_INFO(MODULE_NAME, "  [%d]: %s", i, item->valuestring);
			strcpy(str_arr[i],item->valuestring);
			//WTSL_LOG_INFO(MODULE_NAME, " ptr [%d]: %s", i, str_arr[i]);
        }
    }
}

//解析整型数组
static void parse_int_array(cJSON *array,int *iarr) {
	int *ptr = iarr;
    if (array == NULL || array->type != cJSON_Array) {
        WTSL_LOG_ERROR(MODULE_NAME, "Invalid integer array");
        return;
    }
    
    int size = cJSON_GetArraySize(array);
    WTSL_LOG_INFO(MODULE_NAME, "Integer array (size: %d):", size);
    
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item != NULL && item->type == cJSON_Number) {
            WTSL_LOG_INFO(MODULE_NAME, "  [%d]: %d", i, item->valueint);
			*ptr = item->valueint;
			ptr++;
        }
    }
}

// 去除字符串前后空格
void trim_string(char *str) {
    char *start = str;
    char *end = str + strlen(str) - 1;

    while (*start && isspace(*start)) {
        start++;
    }
    while (end > start && isspace(*end)) {
        *end-- = '\0';
    }
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

static void parse_until_special_char(const char* str, char* result, int result_size)
{
	int i = 0;
	//跳过前面的空格
	while(str && (*str == ' '))
	{
		str++;
	}

	//复制字符直到遇到','结束,遇到']'结束
	while(str && *str != ',' && (i < result_size - 1) && *str != ']')
	{
		result[i++] = *str;
		str++;
	}
	result[i] = '\0';
}

void parse_line_param(cJSON* item, const char* line, char* param, char symol)
{
	char result[20]={0};
	char* colon = strstr(line, param); //寻找该字符串的起始指针

	//带有设备信息行
	if (colon)
	{
		colon = strchr(colon, symol);
		if (colon)
		{
			parse_until_special_char(colon + 1, result, sizeof(result));
			WTSL_LOG_INFO(MODULE_NAME, "%s : %s", param, result);
			cJSON_AddItemToObject(item, param, cJSON_CreateString(result));
		}
	}
}

void parse_param(cJSON* response, const char* data,  char* param, char symol)
{
	char result[20]={0};
	char* colon = strstr(data, param); //寻找该字符串的起始指针

	//带有设备信息行
	if (colon)
	{
		colon = strchr(colon, symol);
		if (colon)
		{
			parse_until_special_char(colon + 1, result, sizeof(result));
			WTSL_LOG_INFO(MODULE_NAME, "%s : %s", param, result);
			cJSON_AddItemToObject(response, param, cJSON_CreateString(result));
		}
	}
}


// 主解析函数
char* parse_show_bss_output() 
{
	char *jsonstr;
	char name[128];
	int channel;
	int rssi;
	int cell_id;
	int domain_cnt;
	int i = 0;
	
	cJSON *response = cJSON_CreateArray();
	
	FILE *fp = NULL;
	char buffer[128]={0};
	char cmd[64] ={0};
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg show_bss", NET_VAP_NAME);
	fp = popen(cmd, "r");

	while(fgets(buffer, sizeof(buffer), fp))
	{
		cJSON* item = cJSON_CreateObject();
		int num = sscanf(buffer, "%s %d %d %d %d", name, &cell_id, &channel, &rssi,&domain_cnt);
		WTSL_LOG_INFO(MODULE_NAME, "%d", num);
		i++;
		if(num != 5 || name == NULL || domain_cnt == 0)
		{
			continue;
		}
		WTSL_LOG_INFO(MODULE_NAME, "扫描到的设备: name: %s, domain_cnt: %d", name, domain_cnt);
		cJSON_AddItemToObject(item, "index", cJSON_CreateNumber(i - 3));
		cJSON_AddItemToObject(item, "name", cJSON_CreateString(name));
		cJSON_AddItemToObject(item, "cell_id", cJSON_CreateNumber(cell_id));
		cJSON_AddItemToObject(item, "mac", cJSON_CreateString("xx:xx:xx:xx:xx:xx"));
		cJSON_AddItemToObject(item, "channel", cJSON_CreateNumber(channel));
		cJSON_AddItemToObject(item, "rssi", cJSON_CreateNumber(rssi));
		cJSON_AddItemToObject(item, "ip", cJSON_CreateString("xx:xx:xx:xx"));
		cJSON_AddItemToArray(response, item);
	}
	char *res = cJSON_Print(response);
	jsonstr = malloc(strlen(res)+1);
	strcpy(jsonstr,res);
	cJSON_Delete(response);
	pclose(fp);
	return jsonstr;
}

int wtsl_core_parse_json_self_test(void *args){
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec self test.......",__FUNCTION__,__LINE__);
	char* result =  slb_node_self_test();
	WTSL_LOG_INFO(MODULE_NAME, "self test:%s",result);
	strcpy(args,result);

	//释放字符串内存
	if(result)
		free(result);

	return 0;
}


#if 0
int wtsl_core_parse_json_view_users(void *args){
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec view users.......",__FUNCTION__,__LINE__);
	char* result =  slb_node_view_users("vap0");
if(result)	
	WTSL_LOG_INFO(MODULE_NAME, "view users:%s",result);
	char* json_result = parse_view_users_output(result);
if(json_result)
	strcpy(args,json_result);

	//释放字符串内存
	if(result)
		free(result);
if(json_result)free(json_result);
	return 0;
}
#else
int wtsl_core_parse_json_view_users(void *args){
	int i=0,role,state,num,scan,roam;
	int uid;
	char nodename[128]={0};
	char macstr[24]={0};
	FILE *fp= NULL;
	char buffer[128]={0};
	// pClient pCurrentClient = NULL;
	cJSON *response = cJSON_CreateArray();
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg view_users", NET_VAP_NAME);
	fp = popen(cmd, "r");
	// WTSL_Core_ListNodes *pList = wtsl_core_get_node_list();
	WTSLNodeList *pList = get_wtsl_core_node_list();
	print_list_info(pList);
    while(fgets(buffer, sizeof(buffer), fp))
	{
		if(strstr(buffer,"vap_role") != 0){
			sscanf(buffer,"vap_role[%d], vap_state[%d], user_num[%d], scanning[%d], roamming[%d]",&role,&state,&num,&scan,&roam);
			WTSL_LOG_INFO(MODULE_NAME, "PPP vap_role[%d], vap_state[%d], user_num[%d], scanning[%d], roamming[%d]",role,state,num,scan,roam);
		}
		if(strstr(buffer,"uid mac_addr") !=0){
			WTSL_LOG_INFO(MODULE_NAME, "################## find uid mac_addr #########################");
			for(int i=0;i<num;i++){
				char *ptr = fgets(buffer,sizeof(buffer),fp);
				if(ptr != NULL)
					sscanf(buffer,"%d %s",&uid,macstr);
				cJSON* item = cJSON_CreateObject();
				cJSON_AddItemToObject(item, "uid", cJSON_CreateNumber(uid));
				cJSON_AddItemToObject(item, "mac", cJSON_CreateString(macstr));
				WTSL_LOG_INFO(MODULE_NAME, "role:%d,uid:%d,macstr:%s",role,uid,macstr);

				
				WTSLNode * pnode = find_wtsl_node_by_mac(pList,macstr);
				// WTSLNode *pnode = find_wtsl_node_by_id(pList,i);
				WTSLNode *gwnode = find_wtsl_node_by_id(pList,i);
				if(pnode != NULL){
					WTSL_LOG_INFO(MODULE_NAME, "pnode:(name:%s,mac:%s,ip:%s)",pnode->info.basic_info.name,pnode->info.basic_info.mac,pnode->info.basic_info.ip);
				}else if(gwnode != NULL){
					WTSL_LOG_INFO(MODULE_NAME, "gwnode:(name:%s,mac:%s,ip:%s)",gwnode->info.basic_info.name,gwnode->info.basic_info.mac,gwnode->info.basic_info.ip);
				}else{
					WTSL_LOG_ERROR(MODULE_NAME, "####### pnode is null or gwnode is null ###########");
				}
				if(role == 2){
					//rolo 2 gnode
					WTSL_LOG_INFO(MODULE_NAME, "role=%d,gnode",role);
					if(pnode != NULL){
						cJSON_AddItemToObject(item, "ip", cJSON_CreateString(pnode->info.basic_info.ip));
						cJSON_AddItemToObject(item, "version", cJSON_CreateString(pnode->info.basic_info.version));
						cJSON_AddItemToObject(item, "rssi", cJSON_CreateNumber(pnode->info.basic_info.rssi));
						cJSON_AddItemToObject(item, "bw", cJSON_CreateNumber(pnode->info.basic_info.bw));
						cJSON_AddItemToObject(item, "tfc_bw", cJSON_CreateNumber(pnode->info.basic_info.tfc_bw));
						cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(NODE_TYPE_SLB_T));
						sprintf(nodename,"%s",pnode->info.basic_info.name);
						WTSL_LOG_INFO(MODULE_NAME, "########## pnode(name:%s,ip:%s,mac:%s) ########",pnode->info.basic_info.name,pnode->info.basic_info.ip,pnode->info.basic_info.mac);
					}else{
						WTSL_LOG_ERROR(MODULE_NAME, "########## pnode and gwnode is null ########");
					}

					
				}else if(role == 1){
					//tnode
					WTSL_LOG_INFO(MODULE_NAME, "########## role is 1 tnode ########");
					pthread_mutex_lock(&global_node_info.mutex);
					if(pnode != NULL){
						sprintf(nodename,"%s",pnode->info.basic_info.name);
						cJSON_AddItemToObject(item, "ip", cJSON_CreateString(pnode->info.basic_info.ip));
						cJSON_AddItemToObject(item, "version", cJSON_CreateString(pnode->info.basic_info.version));
						cJSON_AddItemToObject(item, "type", cJSON_CreateNumber(NODE_TYPE_SLB_G));
						cJSON_AddItemToObject(item, "rssi", cJSON_CreateNumber(global_node_info.node_info.basic_info.rssi));
						cJSON_AddItemToObject(item, "bw", cJSON_CreateNumber(global_node_info.node_info.basic_info.bw));
						cJSON_AddItemToObject(item, "tfc_bw", cJSON_CreateNumber(global_node_info.node_info.basic_info.tfc_bw));
					}
					pthread_mutex_unlock(&global_node_info.mutex);
				}
				cJSON_AddItemToObject(item, "name", cJSON_CreateString(nodename));
				cJSON_AddItemToArray(response, item);
			}
			break;
		}
	}
	pclose(fp);
	char *res = cJSON_Print(response);
	if(res){
		strcpy(args,res);
		WTSL_LOG_INFO(MODULE_NAME, "res:%s",res);
	}
	cJSON_Delete(response);
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] out,i=%d",__FUNCTION__,__LINE__,i);
	return 0;
}
#endif


int wtsl_core_parse_json_tnode_join_net(void *args){
	int ret = -1;
	cJSON *root =cJSON_Parse(args);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return -1;
	}
	char *idx =  cJSON_GetObjectItem(root,"index")->valuestring;
	WTSL_LOG_INFO(MODULE_NAME, "idx:%s",idx);

	ret = slb_t_node_start_join(NET_VAP_NAME, atoi(idx));

	//入网后更新参数，并同步写入配置文件
	WTSLNodeBasicInfo* node = &global_node_info.node_info.basic_info;
	
	int tmp_channel = slb_get_channel(NET_VAP_NAME);
	if(tmp_channel != 0)
		node->channel = tmp_channel;
	node->bw = slb_get_bw(NET_VAP_NAME);
	node->tfc_bw = slb_get_tfc_bw(NET_VAP_NAME);
	config_set_int("CHANNEL", node->channel);
	config_set_int("BW", node->bw);
	config_set_int("TFC_BW", node->tfc_bw);
	
	WTSL_LOG_INFO(MODULE_NAME, "update channel: %d, bw: %d, tfc: %d\n", node->channel, node->bw, node->tfc_bw);	
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec join net.......",__FUNCTION__,__LINE__);
	cJSON_Delete(root);
	return ret;
}

int wtsl_core_parse_json_tnode_show_bss(void *args){
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec show bss.......",__FUNCTION__,__LINE__);
	//slb_t_node_scan("vap0");
	//sleep(5);
	char* json_result = parse_show_bss_output()
    WTSL_LOG_INFO(MODULE_NAME, "%s", json_result);
	strcpy(args,json_result);
	//释放字符串内存
	if(json_result)
		free(json_result);
	return 0;
}

int wtsl_core_parse_json_scan(void *args){
	int ret = -1;
	(void)args;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec scan.......",__FUNCTION__,__LINE__);
	ret = slb_t_node_scan(NET_VAP_NAME);
	return ret;
}

int wtsl_core_parse_json_sle_scan(void *args){
	(void)args;
	return 0;
}

int wtsl_core_parse_json_sle_scan_info(void *args){
	(void)args;
	return 0;
}

int wtsl_core_parse_json_sle_connect(void *args){
	(void)args;
	return 0;
}

int wtsl_core_parse_json_set_node_advinfo(const char *args){
	
	WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	cJSON *root = cJSON_Parse(args);
	if (root == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__FUNCTION__,__LINE__);
		return -1;
	}

	cJSON *cell_id = cJSON_GetObjectItem(root,"cell_id");
	if(cell_id != NULL)
		adv_info->cell_id = cell_id->valueint;

	cJSON *cc_spos = cJSON_GetObjectItem(root,"cc_spos");
	if(cc_spos != NULL)
		adv_info->cc_offset = cc_spos->valueint;

	cJSON *cp_type = cJSON_GetObjectItem(root,"cp_type");
	if(cp_type != NULL)
		adv_info->mib_params.cp_type = cp_type->valueint;
	
	cJSON *symbol_type = cJSON_GetObjectItem(root,"symbol_type");
	if(symbol_type != NULL)
		adv_info->mib_params.symbol_type = symbol_type->valueint;

	cJSON *sysmsg_period = cJSON_GetObjectItem(root,"sysmsg_period");
	if(sysmsg_period != NULL)
		adv_info->mib_params.sysmsg_period = sysmsg_period->valueint;

	cJSON *s_cfg_idx = cJSON_GetObjectItem(root,"s_cfg_idx");
	if(s_cfg_idx != NULL)
		adv_info->mib_params.s_cfg_idx = s_cfg_idx->valueint;

	cJSON *sec_exch_cap = cJSON_GetObjectItem(root,"sec_exch_cap");
	if(sec_exch_cap != NULL)
		adv_info->sec_exch_cap = sec_exch_cap->valueint;

	cJSON *range_opt = cJSON_GetObjectItem(root,"range_opt");
	if(range_opt != NULL)
		adv_info->range_opt = range_opt->valueint;

	cJSON *acs_enable = cJSON_GetObjectItem(root,"acs_enable");
	if(acs_enable != NULL)
		node_param->aifh_enable = acs_enable->valueint;
	
	cJSON *tx_power = cJSON_GetObjectItem(root, "tx_power");
	if(tx_power != NULL)
		adv_info->power = tx_power->valueint;

	//设置参数
	set_node();

	config_set_int("CELL_ID", adv_info->cell_id);
	config_set_int("CC_SPOS", adv_info->cc_offset);
	config_set_int("CP_TYPE", adv_info->mib_params.cp_type);
	config_set_int("SYMBOL_CFG", adv_info->mib_params.symbol_type);
	config_set_int("SYSMSG_PERIOD", adv_info->mib_params.sysmsg_period);
	config_set_int("S_CFG_IDX", adv_info->mib_params.s_cfg_idx);
	config_set_int("SEC_EXCH_CAP", adv_info->sec_exch_cap);
	config_set_int("RANGE_OPT", adv_info->range_opt);
	config_set_int("TX_POWER", adv_info->power);
	config_set_int("ACS_ENABLE", node_param->aifh_enable);

	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec set node advinfo.......",__FUNCTION__,__LINE__);	
	
	cJSON_Delete(root);

	return 0;
}

int wtsl_core_parse_json_setnode(const char *args){
	int mib_params[4]={0};
	char mcs_bound[3][128]={{0}};
	char* ptr_array[3];
	WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	cJSON *root =cJSON_Parse(args);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
		return -1;
	}
	
	pthread_mutex_lock(&global_node_info.mutex);
	cJSON* type = cJSON_GetObjectItem(root,"type");
	if(type != NULL)
		node_param->type = type->valueint;

	cJSON* channel = cJSON_GetObjectItem(root,"channel");
	if(channel != NULL)
		node_param->channel = channel->valueint;

	cJSON* bw = cJSON_GetObjectItem(root,"bw");
	if(bw != NULL)
		node_param->bw = bw->valueint;	

	cJSON *tfc_bw = cJSON_GetObjectItem(root,"tfc_bw");
	if(tfc_bw != NULL)
		node_param->tfc_bw = tfc_bw->valueint;

	cJSON *cell_id = cJSON_GetObjectItem(root,"cell_id");
	if(cell_id != NULL)
		adv_info->cell_id = cell_id->valueint;

	cJSON *name = cJSON_GetObjectItem(root,"name");
	if(name != NULL)
		strcpy(node_param->name, name->valuestring);

	cJSON *ip = cJSON_GetObjectItem(root,"ip");
	if(ip != NULL)
		strcpy(node_param->ip, ip->valuestring);

	cJSON *set_sec_auth_pwd = cJSON_GetObjectItem(root,"set_sec_auth_pwd");
	if(set_sec_auth_pwd != NULL)
		strcpy(adv_info->password, set_sec_auth_pwd->valuestring);

	cJSON *set_mib_params = cJSON_GetObjectItemCaseSensitive(root, "set_mib_params");
	if(set_mib_params != NULL){
		parse_int_array(set_mib_params,mib_params);
		WTSL_LOG_INFO(MODULE_NAME, "mib_params:%d,%d,%d,%d",mib_params[0],mib_params[1],mib_params[2],mib_params[3]);
	}

    cJSON *set_mcs_bound = cJSON_GetObjectItemCaseSensitive(root, "set_mcs_bound");
	if(set_mcs_bound != NULL){

	    for (int i = 0; i < 3; i++) {
	        ptr_array[i] = mcs_bound[i];
	    }
	    parse_string_array(set_mcs_bound,(char **)ptr_array);
		WTSL_LOG_INFO(MODULE_NAME, "msc_bound[0]:%s\nmsc_bound[1]:%s\nmsc_bound[2]:%s\n",mcs_bound[0],mcs_bound[1],mcs_bound[2]);
	}

	cJSON* log_port = cJSON_GetObjectItem(root,"log_port");
	if(log_port != NULL)
		node_param->log_port = log_port->valueint;

	cJSON *net_manage_ip = cJSON_GetObjectItem(root,"net_manage_ip");
	if(net_manage_ip != NULL)
		strcpy(node_param->net_manage_ip, net_manage_ip->valuestring);

	//设置参数
	set_node();

	//保存节点参数到配置文件
	config_set_int("BW", node_param->bw);
	config_set_int("TFC_BW", node_param->tfc_bw);
	config_set_int("CHANNEL", node_param->channel);
	config_set_int("TYPE", node_param->type);
	config_set_int("CELL_ID", adv_info->cell_id);
	config_set_int("TYPE", node_param->type);
	config_set_int("LOG_PORT", node_param->log_port);
	config_set("IP", node_param->ip);
	//config_set("MAC", node_param->mac);
	config_set("NAME", node_param->name);
	config_set("NET_MANAGE_IP", node_param->net_manage_ip);

	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec setnode.......",__FUNCTION__,__LINE__);	
	
	cJSON_Delete(root);
	pthread_mutex_unlock(&global_node_info.mutex);
	return 0;
}

int wtsl_core_parse_json_do_shortrange(const char *args)
{
#ifdef CONFIG_APP_DEBUG
	WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	cJSON *root = cJSON_Parse(args);
	if (root == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__FUNCTION__,__LINE__);
		return -1;
	}

	cJSON* channel = cJSON_GetObjectItem(root,"channel");
	if(channel != NULL)
		node_param->channel = channel->valueint;

	cJSON* bw = cJSON_GetObjectItem(root,"bw");
	if(bw != NULL)
		node_param->bw = bw->valueint;	

	cJSON *tfc_bw = cJSON_GetObjectItem(root,"tfc_bw");
	if(tfc_bw != NULL)
		node_param->tfc_bw = tfc_bw->valueint;

	cJSON *cp_type = cJSON_GetObjectItem(root,"cp_type");
	if(cp_type != NULL)
		adv_info->mib_params.cp_type = cp_type->valueint;
	
	cJSON *symbol_type = cJSON_GetObjectItem(root,"symbol_type");
	if(symbol_type != NULL)
		adv_info->mib_params.symbol_type = symbol_type->valueint;

	cJSON *sysmsg_period = cJSON_GetObjectItem(root,"sysmsg_period");
	if(sysmsg_period != NULL)
		adv_info->mib_params.sysmsg_period = sysmsg_period->valueint;

	cJSON *s_cfg_idx = cJSON_GetObjectItem(root,"s_cfg_idx");
	if(s_cfg_idx != NULL)
		adv_info->mib_params.s_cfg_idx = s_cfg_idx->valueint;

	//设置参数
	set_node();

	config_set_int("CHANNEL", node_param->channel);
	config_set_int("BW", node_param->bw);
	config_set_int("TFC_BW", node_param->tfc_bw);
	config_set_int("CP_TYPE", adv_info->mib_params.cp_type);
	config_set_int("SYMBOL_CFG", adv_info->mib_params.symbol_type);
	config_set_int("SYSMSG_PERIOD", adv_info->mib_params.sysmsg_period);
	config_set_int("S_CFG_IDX", adv_info->mib_params.s_cfg_idx);

	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec do short range test.......",__FUNCTION__,__LINE__);	
	
	cJSON_Delete(root);
#else
	(void)args;
	slb_ifconfig(NET_VAP_NAME, 0);
	
	slb_set_channel(NET_VAP_NAME, 1875);

	int ret = slb_set_bw(NET_VAP_NAME, 80);
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
	mib_params.symbol_type = 3;
	mib_params.sysmsg_period = 1;
	mib_params.s_cfg_idx = 2;
	slb_set_mib_params(NET_VAP_NAME, mib_params);
	slb_ifconfig(NET_VAP_NAME, 1);
#endif

	return 0;
}

int wtsl_core_parse_json_do_remoterange(const char *args)
{
#ifdef CONFIG_APP_DEBUG
	WTSLNodeBasicInfo* node_param = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	cJSON *root = cJSON_Parse(args);
	if (root == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__FUNCTION__,__LINE__);
		return -1;
	}

	cJSON* channel = cJSON_GetObjectItem(root,"channel");
	if(channel != NULL)
		node_param->channel = channel->valueint;

	cJSON* bw = cJSON_GetObjectItem(root,"bw");
	if(bw != NULL)
		node_param->bw = bw->valueint;	

	cJSON *tfc_bw = cJSON_GetObjectItem(root,"tfc_bw");
	if(tfc_bw != NULL)
		node_param->tfc_bw = tfc_bw->valueint;

	cJSON *cp_type = cJSON_GetObjectItem(root,"cp_type");
	if(cp_type != NULL)
		adv_info->mib_params.cp_type = cp_type->valueint;
	
	cJSON *symbol_type = cJSON_GetObjectItem(root,"symbol_type");
	if(symbol_type != NULL)
		adv_info->mib_params.symbol_type = symbol_type->valueint;

	cJSON *sysmsg_period = cJSON_GetObjectItem(root,"sysmsg_period");
	if(sysmsg_period != NULL)
		adv_info->mib_params.sysmsg_period = sysmsg_period->valueint;

	cJSON *s_cfg_idx = cJSON_GetObjectItem(root,"s_cfg_idx");
	if(s_cfg_idx != NULL)
		adv_info->mib_params.s_cfg_idx = s_cfg_idx->valueint;

	cJSON *range_opt = cJSON_GetObjectItem(root, "range_opt");
	if(range_opt != NULL)
		adv_info->range_opt = range_opt->valueint;

	//设置参数
	set_node();

	config_set_int("CHANNEL", node_param->channel);
	config_set_int("BW", node_param->bw);
	config_set_int("TFC_BW", node_param->tfc_bw);
	config_set_int("CP_TYPE", adv_info->mib_params.cp_type);
	config_set_int("SYMBOL_CFG",  adv_info->mib_params.symbol_type);
	config_set_int("SYSMSG_PERIOD",  adv_info->mib_params.sysmsg_period);
	config_set_int("S_CFG_IDX",  adv_info->mib_params.s_cfg_idx);
	config_set_int("RANGE_OPT",  adv_info->range_opt);

	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec do short range test.......",__FUNCTION__,__LINE__);	
	
	cJSON_Delete(root);
#else
	(void)args;
	slb_ifconfig(NET_VAP_NAME, 0);
	
	slb_set_channel(NET_VAP_NAME, 1875);

	int ret = slb_set_bw(NET_VAP_NAME, 80);
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
	mib_params.cp_type = 3;
	mib_params.symbol_type = 2;
	mib_params.sysmsg_period = 1;
	mib_params.s_cfg_idx = 2;
	slb_set_mib_params(NET_VAP_NAME, mib_params);
	slb_set_range_opt(NET_VAP_NAME, 1);
	slb_ifconfig(NET_VAP_NAME, 1);

#endif
	return 0;
}

int wtsl_core_parse_json_do_lowlatency(const char *args)
{
	(void)args;

	//构建命令
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_rtd_params 1 0 5001 5004\"", NET_VAP_NAME);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"max_sf_res_req_en enable\"", NET_VAP_NAME);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"set_mcs_bound 00:00:00:00:00:00 0 12\"", NET_VAP_NAME);
	system(cmd);
	return 0;
}

int wtsl_core_parse_json_do_lowpow(const char *args)
{
	(void)args;
	return 0;
}

int wtsl_core_parse_json_get_hw_resources_usage(void *args)
{
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]exec get dev cpu usage.......",__FUNCTION__,__LINE__);
	char* result = slb_node_get_hw_resources_info();
	WTSL_LOG_INFO(MODULE_NAME, "CPU info:%s",result);
	strcpy(args,result);

	//释放字符串内存
	if(result)
		free(result);

	return 0;
}



void* wtsl_core_get_hw_resources_usage(void* pNode, void *data, unsigned int size, UserContext *ctx){
	(void)pNode;
	(void)size;
	(void)ctx;
	// 如果必须返回指针，需用整数转指针的安全方式
#ifdef __LP64__
	// 64位系统：使用intptr_t确保类型大小匹配
	return (void*)(intptr_t)wtsl_core_parse_json_get_hw_resources_usage(data);
#else
	// 32位系统
	return (void*)wtsl_core_parse_json_get_hw_resources_usage(data);
#endif
}

void* wtsl_core_firmware_upgrade(void *args)
{
	WTSL_LOG_INFO(MODULE_NAME, "start upgrade handle...");

	const char *upload_file = (const char *)args;
	char header_data[1024] = {0};
	firmware_header_t header;
	char version[64] = {0};

	FILE *file =fopen(upload_file, "rb");
	size_t bytes_read = fread(header_data, 1, 1024, file);
	WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d],bytes read:%d",__FUNCTION__,__LINE__,bytes_read);
	fclose(file);
	// 解析JSON头部
    WTSL_LOG_INFO(MODULE_NAME, "start parse firmware header...");
    if (parse_firmware_header(header_data, &header) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "JSON header parse failed!");
        
        // 尝试查找JSON开始位置（容错处理）
        const char *json_start = strchr(header_data, '{');
        if (json_start) {
            WTSL_LOG_INFO(MODULE_NAME, "Attempt from offset %d reparser JSON", json_start - header_data);
            if (parse_firmware_header(json_start, &header) == 0) {
                WTSL_LOG_INFO(MODULE_NAME, "reparser JSON success!");
            } else {
                return strdup("{\"status\":\"error\",\"message\":\"Invalid firmware header format\"}");
            }
        } else {
            return strdup("{\"status\":\"error\",\"message\":\"No JSON data found in header\"}");
        }
    }

	WTSL_LOG_INFO(MODULE_NAME, "start Platform Architecture Verify...");
	if (Platform_verify(&header) == 0) {
		WTSL_LOG_INFO(MODULE_NAME, "Running on %s arch...", header.targetarch);
	} else {
		WTSL_LOG_INFO(MODULE_NAME, "Platform Architecture verify failed");
		return strdup("{\"status\":\"error\",\"message\":\"Platform Architecture verify failed\"}");
	}

	WTSL_LOG_INFO(MODULE_NAME, "start MD5 verify...");
    if (extract_and_verify_firmware("/tmp/upgrade.tar.gz", &header) != 0) {
        WTSL_LOG_INFO(MODULE_NAME, "MD5 verify failed");
        return strdup("{\"status\":\"error\",\"message\":\"MD5 verification failed\"}");
    }

	WTSL_LOG_INFO(MODULE_NAME, "MD5 verify success!");

	int upgrade_result = handle_firmware_upgrade(&header, version);
	if (upgrade_result != 0) {
		WTSL_LOG_ERROR(MODULE_NAME, "upgrade handle failed");
		return strdup("{\"status\":\"error\",\"message\":\"Upgrade process failed\"}");
	}

	char success_response[1024] = {0};

	// check upgrade package
	char extract_dir[UPGRADE_PATH_SIZE] = {0};
	snprintf(extract_dir, sizeof(extract_dir), "/tmp/upload_file");

    char app_dir[UPGRADE_PATH_SIZE] = {0};
	char www_dir[UPGRADE_PATH_SIZE] = {0};
	char firmware_dir[UPGRADE_PATH_SIZE] = {0};
	snprintf(app_dir, sizeof(app_dir), "%s/app", extract_dir);
	snprintf(www_dir, sizeof(www_dir), "%s/www", extract_dir);
	snprintf(firmware_dir, sizeof(firmware_dir), "%s/firmware_t%s", extract_dir, Version);

	int has_app = (access(app_dir, F_OK) == 0);
	int has_www = (access(www_dir, F_OK) == 0);
	int has_firmware = (access(firmware_dir, F_OK) == 0);

	int has_libs = 0;
	if (has_app) {
		char lib_wtsl_core[UPGRADE_PATH_SIZE] = {0};
		char lib_cjson[UPGRADE_PATH_SIZE] = {0};
		char lib_microhttpd[UPGRADE_PATH_SIZE] = {0};

		snprintf(lib_wtsl_core, sizeof(lib_wtsl_core), "%s/libwtsl_core.so", app_dir);
		snprintf(lib_cjson, sizeof(lib_cjson), "%s/libcjson.so", app_dir);
		snprintf(lib_microhttpd, sizeof(lib_microhttpd), "%s/libmicrohttpd.so", app_dir);

		has_libs = (access(lib_wtsl_core, F_OK) == 0) ||
					(access(lib_cjson, F_OK) == 0) ||
					(access(lib_microhttpd, F_OK) == 0);
	}

	if (has_app && has_www) {
		if (has_libs) {
			snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"Application and libraries and web files upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"wtsl_app, shared libraries, web files\","
				"\"note\":\"Application upgrade running in background with libraries update\"}",
				header.version, header.firmwaretype, header.process);
		} else {
			snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"Application and web files upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"wtsl_app,web files\","
				"\"note\":\"Application upgrade running in background\"}",
				header.version, header.firmwaretype, header.process);
		}
	} else if (has_app) {
		if (has_libs) {
			snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"Application and libraries upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"wtsl_app, shared libraries\","
				"\"note\":\"Application upgrade running in background with libraries update\"}",
				header.version, header.firmwaretype, header.process);
		} else {
			snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"Application upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"wtsl_app\","
				"\"note\":\"Application upgrade running in background as orphan process\"}",
				header.version, header.firmwaretype, header.process);
		}
	} else if (has_www) {
		snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"web files upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"web files\","
				"\"note\":\"web files have been updated, no service restart required\"}",
				header.version, header.firmwaretype, header.process);
	}

	if (has_firmware) {
		snprintf(success_response, sizeof(success_response),
				"{\"status\":success\",\"message\":\"hi firmware files upgrade submitted\","
				"\"version\":\"%s\",\"type\":\"%s\",\"process\":\"%s\","
				"\"upgrade_components\":\"hi firmware ko files\","
				"\"note\":\"hi firmware ko files have been updated, no service restart required\"}",
				header.version, header.firmwaretype, header.process);
	}

	WTSL_LOG_INFO(MODULE_NAME, "upgrade handle finished!");
	WTSL_LOG_INFO(MODULE_NAME, "Firmware update completed, rebooting...");

	return strdup(success_response);
}

void* wtsl_core_parse_json_extract_file_header(void *args)
{
	const char *upload_file = (const char *)args;
	FILE *file,*filew;
	char header_data[2048] = {0};
	firmware_header_t header;


	WTSL_LOG_INFO(MODULE_NAME, "file path : %s", upload_file);

	// 检查文件是否存在
	if (access(upload_file, F_OK) != 0) {
		WTSL_LOG_WARNING(MODULE_NAME, "file not exist:%s", upload_file);
		return strdup("{\"status\":\"error\",\"message\":\"Uploaded file not found\"}");
	}

	// 获取文件详细信息
    struct stat st;
    if (stat(upload_file, &st) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "unable to get file status: %s", upload_file);
        return strdup("{\"status\":\"error\",\"message\":\"Cannot access file information\"}");
    }

	WTSL_LOG_INFO(MODULE_NAME, "file exist,size: %lld bytes", st.st_size);

	// 检查文件大小是否足够
    if (st.st_size <= 512) {
        WTSL_LOG_INFO(MODULE_NAME, "file size: %lld bytes", st.st_size);
        return strdup("{\"status\":\"error\",\"message\":\"File too small\"}");
    }

	// 读取前512字节头部
    file = fopen(upload_file, "rb");
	filew = fopen("/tmp/upgrade.tar.gz","wb");
    if (!file) {
        WTSL_LOG_WARNING(MODULE_NAME, "can't open file: %s", upload_file);
        return strdup("{\"status\":\"error\",\"message\":\"Cannot open uploaded file\"}");
    }

    size_t read_size = fread(header_data, 1, 1024, file);
	WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d],read size:%d",__FUNCTION__,__LINE__,read_size);
	// char *ptr = strstr(header_data,"{");
	// char *tar_header = ptr+512;
	// fwrite(tar_header,)

    fclose(file);

	// 解析JSON头部
    WTSL_LOG_INFO(MODULE_NAME, "start parse firmware header...");
    if (parse_firmware_header(header_data, &header) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "JSON header parse failed!");
        
        // 尝试查找JSON开始位置（容错处理）
        const char *json_start = strchr(header_data, '{');
        if (json_start) {
            WTSL_LOG_INFO(MODULE_NAME, "Attempt from offset %d reparser JSON", json_start - header_data);
            if (parse_firmware_header(json_start, &header) == 0) {
                WTSL_LOG_INFO(MODULE_NAME, "reparser JSON success!");
            } else {
                return strdup("{\"status\":\"error\",\"message\":\"Invalid firmware header format\"}");
            }
        } else {
            return strdup("{\"status\":\"error\",\"message\":\"No JSON data found in header\"}");
        }
    }

	WTSL_LOG_INFO(MODULE_NAME, "JSON header parser successfully!");

	WTSL_LOG_INFO(MODULE_NAME, "=== firmware header ===");
    WTSL_LOG_INFO(MODULE_NAME, "  filename: %s", header.filename);
    WTSL_LOG_INFO(MODULE_NAME, "  MD5: %s", header.md5sum);
    WTSL_LOG_INFO(MODULE_NAME, "  filesize: %s", header.filesize);
    WTSL_LOG_INFO(MODULE_NAME, "  version: %s", header.version);
    WTSL_LOG_INFO(MODULE_NAME, "  file_type: %s", header.filetype);
    WTSL_LOG_INFO(MODULE_NAME, "  firmwaretype: %s", header.firmwaretype);
    WTSL_LOG_INFO(MODULE_NAME, "  up_method: %s", header.up_method);
    WTSL_LOG_INFO(MODULE_NAME, "  process: %s", header.process);
	WTSL_LOG_INFO(MODULE_NAME, "  targetarch: %s", header.targetarch);

	file = fopen(upload_file, "rb");
	 if (!file) {
        WTSL_LOG_INFO(MODULE_NAME, "can't open file: %s", upload_file);
        return strdup("{\"status\":\"error\",\"message\":\"Cannot open uploaded file\"}");
    }
	WTSL_LOG_INFO(MODULE_NAME, "header.filesize:%d",atoi(header.filesize));
	char *read_buf = malloc(st.st_size);
	read_size = fread(read_buf,st.st_size,1,file);
	WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d],read size:%d",__FUNCTION__,__LINE__,read_size);
	char *ptr = strstr(read_buf,"{");
	char *tar_header = ptr+1024;
	fwrite(tar_header,atoi(header.filesize),1,filew);
	fclose(filew);
	fclose(file);
	free(read_buf);

	char success_response[512];
    snprintf(success_response, sizeof(success_response),
             "{\"status\":\"success\",\"message\":\"Firmware uploaded and verified successfully\","
             "\"version\":\"%s\",\"type\":\"%s\",\"filename\":\"%s\","
             "\"md5\":\"%s\",\"size\":\"%s\"}",
             header.version, header.firmwaretype, header.filename,
             header.md5sum, header.filesize);
    
    WTSL_LOG_INFO(MODULE_NAME, "firmware upload Handle finish");
    return strdup(success_response);
}

char *data_response_to_json(char status,const char *data){
	char *jsonstr;
    cJSON *response = cJSON_CreateObject();
	cJSON* existing_json = cJSON_Parse(data);
	if(status != 0){
    	cJSON_AddStringToObject(response, "status", "Failed");
		
	}else{
		cJSON_AddStringToObject(response, "status", "success");
		cJSON_AddItemToObject(response, "data", existing_json);
	}
    char *res = cJSON_Print(response);
	jsonstr = malloc(strlen(res)+1);
	strcpy(jsonstr,res);
    cJSON_Delete(response);
	return jsonstr;
}

void parse_host_version(char* host_verison)
{
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_version", NET_VAP_NAME);
	char* tmp = system_output_to_string(cmd);
	char* colon = strstr(tmp, "device version:"); //寻找该字符串的起始指针
	if (colon)
	{
		colon = strchr(colon, ':');

		if (colon)
		{
			//parse_until_special_char(colon + 1, host_verison, sizeof(host_verison));
        	strncpy(host_verison,colon+1,4);
        	host_verison[4]='\0';			
		}
	}
}

/**
 * 执行命令一次性读取所有的输出到字符串
 * &param command 要执行的命令
 * @return 包含所有输出的字符串，需要调用者free()释放，失败返回NULL
 
*/
char* system_output_to_string(const char* command)
{
	FILE* fp;
	char* content = NULL;;
	char buffer[1024]={0};
	size_t total_size = 0;
	
	//执行命令并打开读取管道
	fp = popen(command, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "exec command failed: %s", strerror(errno));
		return NULL;
	}

	while(fgets(buffer, sizeof(buffer), fp))
	{
		size_t line_size = strlen(buffer); //strlen返回的长度不包含终止符'\0'
		size_t new_size = total_size + line_size + 1;

		//重新分配内存
		char* new_content = realloc(content, new_size);
		if(new_content == NULL)
		{
			free(content);
			pclose(fp);
			WTSL_LOG_ERROR(MODULE_NAME, "failed to reallocate memory");
			return NULL;
		}

		content = new_content;

		//追加新行到内容中
		strcpy(content + total_size, buffer);
		total_size += line_size;

		content[total_size] = '\0'; //添加终止符
		
	}

	//关闭管道
	pclose(fp);

	return content;
	
}

char* parse_view_users_output(const char *data) 
{
	char *jsonstr;
	int expected_user_num = 0;
	char* token;
	if(!data){
        WTSL_LOG_ERROR(MODULE_NAME, "data is null");
        return NULL;
	}
    WTSL_LOG_INFO(MODULE_NAME, "data:[%s]",data);
	cJSON *response = cJSON_CreateArray();
	//表示收到这条指令了 返回success
	//cJSON_AddStringToObject(response, "status", "success");
	//获取扫描设备数量
	char* tmp = strstr(data, "user_num");
    if(tmp != NULL)WTSL_LOG_INFO(MODULE_NAME, "tmp:%s",tmp); 	
	char *colon = strchr(tmp, '[');
	if (colon)
	{
		expected_user_num = atoi(colon + 1);
		char num_str[20];
		snprintf(num_str, sizeof(num_str), "%d", expected_user_num);
		//cJSON_ReplaceItemInObject(response, "scan_num", cJSON_CreateString(num_str));
	}

	//cJSON* scan_result = cJSON_AddArrayToObject(response, "scan_result");

	//寻找第一个换行符
	tmp = strstr(data, "\n");
	//寻找第二个换行符
	tmp = strstr(tmp + 1, "\n");
	//寻找第三个换行符
	tmp = strstr(tmp + 1, "\n");
	//第四行的指针位置
	char* line_start = tmp + 1;

	for(int i = 0; i < expected_user_num; i++)
	{		
		cJSON* item = cJSON_CreateObject();
		token = strtok(line_start, " ");
		cJSON_AddItemToObject(item, "uid", cJSON_CreateString(token));
		token = strtok(NULL, " \n");
		cJSON_AddItemToObject(item, "mac", cJSON_CreateString(token));
		cJSON_AddItemToObject(item, "name", cJSON_CreateString("tnode_000"));
       	cJSON_AddItemToObject(item, "type", cJSON_CreateString("tnode"));
       	cJSON_AddItemToObject(item, "rssi", cJSON_CreateString("-75"));
       	cJSON_AddItemToObject(item, "bw", cJSON_CreateString("20"));
       	cJSON_AddItemToObject(item, "tfc_bw", cJSON_CreateString("20"));
		cJSON_AddItemToArray(response, item);
		line_start = strstr(line_start + 1, "\n");
	}
	char *res = cJSON_Print(response);
	jsonstr = malloc(strlen(res)+1);
	strcpy(jsonstr,res);
	cJSON_Delete(response);
	return jsonstr;
}
