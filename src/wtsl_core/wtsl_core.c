#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "wtsl_log_manager.h"
#include "wtsl_core_api.h"
#include "wtsl_core_dataparser.h"
#include "wtsl_core_slb_interface.h"
#include "wtsl_cfg_manager.h"
const char *js_cfg_path="/home/wt/www/config.json";
extern int wtsl_http_main();
char g_is_run = 1;
extern int create_splink_state_watcher();
extern int wtsl_gw_port;
extern SPLINK_INFO global_node_info;

#define MODULE_NAME "wt_core"



static char* create_default_js_config_jsonstr(char *ipstr){
	cJSON *root = cJSON_CreateObject();
	if(!root){
		WTSL_LOG_ERROR(MODULE_NAME,"create json object error");
		return NULL;
	}
	cJSON_AddStringToObject(root,"serverip",ipstr);
	cJSON_AddNumberToObject(root,"port",wtsl_gw_port);
	char *json_str = cJSON_Print(root);
	if(!json_str){
		WTSL_LOG_ERROR(MODULE_NAME,"cjson str create error");
		cJSON_Delete(root);
		return NULL;
	}
	cJSON_Delete(root);
	return json_str;
}

int sync_ip_to_js_cfg(const char *path,WTSLNodeBasicInfo* param){
	FILE *fp = NULL;
	char *ip_ptr = NULL;
	char *update_str = NULL;

	if(param == NULL || path == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "null error");
		return -1;
	}
	ip_ptr = param->ip;
	fp = fopen(path,"w");
	update_str = create_default_js_config_jsonstr(ip_ptr);
	if(update_str != NULL){
		fwrite(update_str,1,strlen(update_str),fp);
		cJSON_free(update_str);
	}
	fclose(fp);
	return 0;
}

int wtsl_core_main() {
	int ret,vercode;
	char verstr[10];

	srand((unsigned int)time(NULL));
	ret = wtsl_core_get_version(&vercode,verstr);
	global_node_info.node_info.basic_info.adv_info = (uintptr_t)malloc(sizeof(WTSLNodeAdvInfo));
	param_default();
	config_read();
	WTSLNodeBasicInfo* param = &global_node_info.node_info.basic_info;
	if(param->type == NODE_TYPE_SLB_G){
		ret |= system("echo 1 > /sys/class/gpio/gpio419/value");
	}else if(param->type == NODE_TYPE_SLB_T){
		ret |= system("echo 0 > /sys/class/gpio/gpio419/value");
	}
	init_node();

	if (wtsl_logger_init(APP_NAME, global_node_info) != 0) {
		printf("Failed to initialize logger\n");
		ret = 1;
	}

#ifdef CONFIG_APP_DEBUG
	// silence_stdout_stderr();
#else
	silence_stdout_stderr();
#endif

	WTSL_LOG_INFO(MODULE_NAME, "wtsl logger initialized!");
	WTSL_LOG_INFO(MODULE_NAME, "wtsl_core vercode:%d, commitid:%s",vercode,verstr);
	WTSL_LOG_INFO(MODULE_NAME, " bw: %d, type: %d, tfc_bw: %d,name:%s", param->bw,param->type,param->tfc_bw, param->name); //打印所有参数


#ifdef HTTP_RESTFUL_API_SUPPORT	
	wtsl_http_main();
#endif

//与js同步ip通信地址
sync_ip_to_js_cfg(js_cfg_path,param);

#ifdef MQTT_API_SUPPORT	
	wtsl_mqtt_main();
#endif
	create_splink_state_watcher();
    return ret;
}


int wtsl_core_release(){
	int ret = -1;
	WTSL_LOG_INFO("SYSTEM", "[%s][%d] In\n",__FUNCTION__,__LINE__);
	g_is_run = 0;
	ret = system("killall udhcpc");
	ret |= system("killall udhcpd");
	ret |= system("killall uspd");
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
	}
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] Out\n",__FUNCTION__,__LINE__);
	sleep(1);
	return 0;
}
