#include <stdio.h>
#include <string.h>

#include "wtsl_log_manager.h"
#include "wtsl_core_http_restful_api.h"
#include "wtsl_core_node_manager.h"
#include "wtsl_core_list.h"
#include "wtsl_core_node_list.h"
#include "wtsl_user_manager.h"

#include "wtsl_core_dataparser.h"
#include "wtsl_core_dataparser_ota.h"


#define MODULE_NAME "http_api"

typedef struct {
    char *str;
} UrlData;

typedef void* (*WTApiFuncPtr)(const char *url,const void* args,int size);

typedef struct {
	char *apiname;
	WTApiFuncPtr pfunc;
}WTAPI_FUNC_MAP;


static List *g_url_list = NULL;

// #define OLD_API 1
#ifdef OLD_API

static void *wtsl_core_setnode(void *args);
static void *wtsl_core_scan(void *args);
static void *wtsl_core_tnode_join_net(void *args);
static void *wtsl_core_tnode_show_bss(void *args);
static void *wtsl_core_self_test(void *args);
static void *wtsl_core_view_users(void *args);
static void *wtsl_core_get_hw_resources_usage(void *args);
static void* wtsl_core_upgrade_firmware(void* args);

// 函数指针类型定义
typedef void* (*FuncPtr)(void *args);

// 结构体：关联名字和函数
typedef struct {
    char *name;       // 操作名称
    FuncPtr function; // 对应的函数指针
} NameFuncMap;


// 映射数组
NameFuncMap get_func_mappings[] = {
    {"scangnodes", wtsl_core_tnode_show_bss},
    {"getDevBasicinfo", wtsl_core_self_test},
    {"getDevConninfo", wtsl_core_view_users},
    {"cpu_usageinfo", wtsl_core_get_hw_resources_usage},
    {NULL, NULL}      // 结束标志
};

// 映射数组
NameFuncMap post_func_mappings[] = {
    {"setnode", wtsl_core_setnode},
    {"scan", wtsl_core_scan}, //暂时未用
    {"connectgnode", wtsl_core_tnode_join_net},
	{"upgradeFirmware", wtsl_core_upgrade_firmware},
    {NULL, NULL}      // 结束标志
};

static void *wtsl_core_self_test(void *args){
	char ret = -1;
	(void)args;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	static char recv_bss[512]={0};
	ret = wtsl_core_parse_json_self_test(recv_bss);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)recv_bss;
}

static void *wtsl_core_view_users(void *args){
	char ret = -1;
	(void)args;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	static char recv_bss[512]={0};
	ret = wtsl_core_parse_json_view_users(recv_bss);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)recv_bss;
}

static void *wtsl_core_tnode_join_net(void *args){
	char ret = -1;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	ret = wtsl_core_parse_json_tnode_join_net(args);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	//todo call
	return (void *)"join_net";
}

static void *wtsl_core_tnode_show_bss(void *args){
	char ret = -1;
	(void)args;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	static char recv_bss[512]={0};
	ret = wtsl_core_parse_json_tnode_show_bss(recv_bss);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)recv_bss;
}
static void *wtsl_core_scan(void *args){
	char ret = -1;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	ret = wtsl_core_parse_json_scan(args);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	//todo CALL_LOW_SCAN(args);
	 
	return (void *)"scan";
}
static void *wtsl_core_setnode(void *args){
	char ret = -1;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	if(args == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "args is NULL");
		return NULL;
	}
	ret = wtsl_core_parse_json_setnode(args);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	WTSL_LOG_INFO(MODULE_NAME, "执行获取节点操作,wtsl_core_setnode");
	return (void *)"gnode0_000";
}

static void *wtsl_core_get_hw_resources_usage(void *args)
{
	char ret = -1;
	(void)args;
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	static char recv_cpu_info[512] = {0};
	ret = wtsl_core_parse_json_get_hw_resources_usage(recv_cpu_info);
	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
		return NULL;
	}
	return (void *)recv_cpu_info;
}

static void* wtsl_core_uploadfirmware(const char* args,int size){
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	FILE *fp = fopen("/tmp/upgrade.bin","wb");
	fwrite(args,size,1,fp);
	fclose(fp);

	char recv_file_path[256] = "/tmp/upgrade.bin";
	APIRET apiret;
    apiret.data = wtsl_core_parse_json_extract_file_header((void *)recv_file_path);
	apiret.status = (apiret.data != NULL) ? 0 : -1;

	return apiret.data;
}

static void* wtsl_core_upgrade_firmware(void *args)
{
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]",__FUNCTION__,__LINE__);
	(void)args;
	char recv_file_path[256] = "/tmp/upgrade.bin";

	return wtsl_core_firmware_upgrade((void *)recv_file_path);
}

#endif

extern WTAPI_FUNC_MAP wtapi_get_func_mappings[];
extern WTAPI_FUNC_MAP wtapi_post_func_mappings[];

static inline char *param_arr(char *str){
	char *tokens[32];  // 存储分割后的子串（假设最多32个）
    int count = 0;

    // 首次调用strtok，传入原始字符串和分隔符
    char *token = strtok(str, "/");
    // 循环获取所有子串
    while (token != NULL && count < 31) {  // 留一个位置防止越界
        tokens[count++] = token;

        token = strtok(NULL, "/");  // 后续调用传入NULL
    }
    tokens[count] = NULL;  // 用NULL标记数组结束（可选）

    // 打印结果
    WTSL_LOG_INFO(MODULE_NAME, "substrings are divided %d :\n", count);
    for (int i = 0; i < count; i++) {
        WTSL_LOG_INFO(MODULE_NAME, "[%d]: %s\n", i, tokens[i]);
    }
	return 0;
}


int node_id_compare(const void *a, const void *b) {
	const WTSLNode *node = (const WTSLNode *)a;
	int target_id = *(const int *)b;
	WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d]compare node id:%d with target id:%d",__FUNCTION__,__LINE__,node->id,target_id);
	return (node->id == target_id);
}

static void *wtsl_core_get_nodes_functions(const char *url,const void *args,int size){
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],url:%s,args:%s,size:%d",__FUNCTION__,__LINE__,url,args,size);
	static char recv_bss[1024]={0};
	WTSLNode *p_node = NULL;
	char *api_str = ((UrlData *)list_get(g_url_list,2))->str;
	if(strncmp(api_str,"nodes",5)!=0){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no support api:%s",__FUNCTION__,__LINE__,api_str);
		return NULL;
	}
	if(g_url_list->size <= 3){
		WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d]no param,do get all nodes info",__FUNCTION__,__LINE__);
		WTSLNodeList *nd = get_wtsl_core_node_list();
		p_node = find_wtsl_node_by_id(nd,0);
		void *ret = CALL_INTERFACE(p_node,get_all_nodes_info,recv_bss,sizeof(recv_bss),&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get all nodes info error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}
	if(g_url_list->size == 5){
		WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d]do get node func info",__FUNCTION__,__LINE__);
		char *id_str = ((UrlData *)list_get(g_url_list,3))->str;
		char *func_str = ((UrlData *)list_get(g_url_list,4))->str;
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get id_str:%s,func:%s",__FUNCTION__,__LINE__,id_str,func_str);
		WTSLNodeList *nd = get_wtsl_core_node_list();
		p_node = find_wtsl_node_by_id(nd,atoi(id_str));
		if(p_node == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no node found for id:%s",__FUNCTION__,__LINE__,id_str);
			return NULL;
		}
		WTSL_LOG_DEBUG(MODULE_NAME,"pnode:0x%x,node[%d]:(name:%s,mac:%s,ip:%s)",p_node,p_node->id,p_node->info.basic_info.name,p_node->info.basic_info.mac,p_node->info.basic_info.ip);
		if(strncmp(func_str,"basicinfo",9)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do get basic info,interface:%p",__FUNCTION__,__LINE__,p_node->interface);
			if(p_node->interface->get_node_basicinfo == NULL){
				WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] get node basicinfo is null",__FUNCTION__,__LINE__);
				return NULL;
			}
			void *ret = CALL_INTERFACE(p_node,get_node_basicinfo,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get basic info error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)recv_bss;
		}else if(strncmp(func_str,"conninfo",8)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do get node conn info",__FUNCTION__,__LINE__);
			// void *ret = p_node->get_node_conninfo(p_node,recv_bss,sizeof(recv_bss));
			void *ret = CALL_INTERFACE(p_node,get_node_conninfo,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node conn info error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)recv_bss;
		} else if(strncmp(func_str,"advinfo",7)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do get node Advanced information",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_node_advinfo,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node Advanced information error",__FUNCTION__,__LINE__);
				return NULL;
			}
		}else if(strncmp(func_str,"scan",4)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node scan",__FUNCTION__,__LINE__);
			// void *ret = p_node->start_scan(p_node,recv_bss,sizeof(recv_bss));
			void *ret = CALL_INTERFACE(p_node,start_scan,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node start scan error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)recv_bss;
		}else if (strncmp(func_str,"resourcesinfo",13)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node get hw resources info",__FUNCTION__,__LINE__);
			// char *ret = (char *)p_node->get_hw_resources_info((void *)args);
			void *ret = CALL_INTERFACE(p_node,get_hw_resources_info,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] get node hw resources info error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
		}else if (strncmp(func_str,"show_bss_info", 14)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node show_bss",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,show_bss_info,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] node show bss info error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
		}else if (strncmp(func_str, "sle_scan", 8)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node sle scan",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_sle_scan,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if (ret == NULL) {
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]do node sle scan failed",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;			
		}else if (strncmp(func_str, "sle_show_bss", 12)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get node sle bss info",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_sle_scan_result,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if (ret == NULL) {
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node sle bss info failed",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
		}else if (strncmp(func_str, "sle_conninfo", 12)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get node sle conn info",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_sle_conninfo,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if (ret == NULL) {
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node sle conn info failed",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;	
		}else if (strncmp(func_str, "sle_basicinfo", 13)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get node sle basic info",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_sle_basicinfo,recv_bss,sizeof(recv_bss),&g_user_ctx);
			if (ret == NULL) {
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get node sle basic info failed",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;			
		}else{
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no support func:%s",__FUNCTION__,__LINE__,func_str);
			return NULL;
		}
	}

	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],url:%s,args:%s,size:%d OUT",__FUNCTION__,__LINE__,url,args,size);
	return (void *)recv_bss;
}

static void *wtsl_core_post_nodes_functions(const char *url,const void *args,int size){
	static char recv_bss[1024]={0};
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],url:%s,glist.size:%d",__FUNCTION__,__LINE__,url,g_url_list->size);

	if(g_url_list->size < 5){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no support api",__FUNCTION__,__LINE__);
		return NULL;
	}
	
	char *api_str = ((UrlData *)list_get(g_url_list,2))->str;
	char *id_str = ((UrlData *)list_get(g_url_list,3))->str;
	char *func_str = ((UrlData *)list_get(g_url_list,4))->str;
	WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d]api_str:%s,id_str:%s,func_str:%s",__FUNCTION__,__LINE__,api_str,id_str,func_str);

	WTSLNodeList *nd = get_wtsl_core_node_list();
	WTSLNode *p_node = find_wtsl_node_by_id(nd, atoi(id_str));
	if(p_node == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no node found for id:%s",__FUNCTION__,__LINE__,id_str);
		return NULL;
	}

	if(strcmp(func_str,"basicinfo")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do set node basic info",__FUNCTION__,__LINE__);
		// char *ret = (char *)p_node->set_node_basicinfo(p_node,(void *)args,size); // 清除旧信息
		void *ret = CALL_INTERFACE(p_node,set_node_basicinfo,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if(strcmp(func_str,"advinfo")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do set node Advanced information",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,set_node_advinfo,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]data paraser error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if(strcmp(func_str,"connect")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do connect node ",__FUNCTION__,__LINE__);
		// char *ret = (char *)p_node->do_connect(p_node,(void *)args,size); // 清除旧信息
		void *ret = CALL_INTERFACE(p_node,do_connect,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]node connect error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if(strcmp(func_str,"autojoinNetwork")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do autojoinNetwork ",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,do_autojoinNetwork,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]autojoinNetwork error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;		
	}else if(strcmp(func_str,"timesync")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do timesync node ",__FUNCTION__,__LINE__);
		// char *ret = (char *)p_node->do_time_sync(p_node,(void *)args,size); // 清除旧信息
		void *ret = CALL_INTERFACE(p_node,do_time_sync,(void *)args,size,&g_user_ctx);
        if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]node connect error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if(strcmp(func_str,"reboot")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do reboot node ",__FUNCTION__,__LINE__);
		// char *ret = (char *)p_node->do_reboot(p_node,(void *)args,size); // 清除旧信息
		void *ret = CALL_INTERFACE(p_node,do_reboot,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]node reboot error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if(strcmp(func_str,"disconnect")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do disconnect ",__FUNCTION__,__LINE__);
		// char *ret = (char *)p_node->do_reboot(p_node,(void *)args,size); // 清除旧信息
		void *ret = CALL_INTERFACE(p_node,do_disconnect,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]node disconnect error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;	
	}else if(strcmp(func_str,"restore")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do restore ",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,do_restore,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]node restore error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if (strcmp(func_str,"aifh")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node aifh",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,do_aifh,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] node aifh error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if (strcmp(func_str,"chanswitch")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do node chanswitch",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,do_chswitch,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] node chswitch error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if (strcmp(func_str,"secalg")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get secalg",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,get_secalg,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] get secalg error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;	
	}else if (strcmp(func_str,"adaptivemcs")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]set  adaptivemcs",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,set_adaptivemcs,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] set adaptivemcs error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;	
	}else if (strcmp(func_str,"clearschmcs")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do clearschmcs",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,do_clearschmcs,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do clearschmcs error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;	
	}else if (strcmp(func_str,"mcsboundinfo")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get mcsboundinfo",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,get_mcsboundinfo,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] get mcsboundinfo error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;	
	}else if (strcmp(func_str,"mcsbound")==0){
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]set mcsbound",__FUNCTION__,__LINE__);
		void *ret = CALL_INTERFACE(p_node,set_mcsbound,(void *)args,size,&g_user_ctx);
		if(ret == NULL){
			WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] set mcsbound error",__FUNCTION__,__LINE__);
			return NULL;
		}
		return (void *)ret;
	}else if (strncmp(func_str,"adaptivemcsinfo", 14)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get adaptivemcsinfo",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_adaptivemcsinfo,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] get adaptivemcsinfo error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str,"traceinfo", 9)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get traceinfo",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,get_traceinfo,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] get traceinfo error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;			
	}else if (strncmp(func_str,"throughput_test", 15)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do throughput test",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_throughput_test,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do throughput test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "shortrange_test", 15)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do shortrange test",__FUNCTION__,__LINE__);
            void *ret = CALL_INTERFACE(p_node,do_shortrange_test,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do shortrange test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "remoterange_test", 16)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do remoterange test",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_remoterange_test,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do remoterange test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "lowpow_test", 11)==0) {
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do lowpow test",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_lowpow_test,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do remoterange test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "lowlatency_test", 15)==0) {
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do lowlatency test",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_lowlatency_test,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do lowlatency test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "sle_connect", 11)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do sle connect",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,do_sle_connect,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] do remoterange test error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "sle_basicinfo", 13)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]set sle basicinfo",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,set_sle_basicinfo,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] set sle basicinfo error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
	}else if (strncmp(func_str, "sle_announce_id", 13)==0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]set sle announce_id",__FUNCTION__,__LINE__);
			void *ret = CALL_INTERFACE(p_node,set_sle_announce_id,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] set sle announce_id error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;							
	}else if(strncmp(func_str,"firmware", 8) == 0){
		if(g_url_list->size < 5)
		return NULL;

		char *func_way_str = ((UrlData *)list_get(g_url_list,5))->str;
		if (strncmp(func_way_str, "upload", 6) == 0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do nodes firmware upload",__FUNCTION__,__LINE__);
			// char *ret = (char *)p_node->do_upload(p_node,(void *)args,size);
			void *ret = CALL_INTERFACE(p_node,do_upload,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]upload firmware package error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
			}
		if(strncmp(func_way_str, "upgrade", 7) == 0){
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do nodes firmware upgrade",__FUNCTION__,__LINE__);
			// char *ret = (char *)p_node->do_upgrade(p_node,(void *)args,size);
			void *ret = CALL_INTERFACE(p_node,do_upgrade,(void *)args,size,&g_user_ctx);
			if(ret == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]upgrade firmware package error",__FUNCTION__,__LINE__);
				return NULL;
			}
			return (void *)ret;
		}
	}
	if(g_url_list->size == 6)
		{
			WTSL_LOG_DEBUG(MODULE_NAME, "[%s][%d]do get node func info",__FUNCTION__,__LINE__);
			char *id_str = ((UrlData *)list_get(g_url_list,3))->str;
			char *func_str = ((UrlData *)list_get(g_url_list,5))->str;
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]get id_str:%s,func:%s",__FUNCTION__,__LINE__,id_str,func_str);
			WTSLNodeList *nd = get_wtsl_core_node_list();
			p_node = find_wtsl_node_by_id(nd,atoi(id_str));
			if(p_node == NULL){
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no node found for id:%s",__FUNCTION__,__LINE__,id_str);
				return NULL;
			}
			WTSL_LOG_DEBUG(MODULE_NAME,"pnode:0x%x,node[%d]:(name:%s,mac:%s,ip:%s)",p_node,p_node->id,p_node->info.basic_info.name,p_node->info.basic_info.mac,p_node->info.basic_info.ip);
			if(strncmp(func_str,"traffic",7)==0){
				WTSL_LOG_INFO(MODULE_NAME, "[%s][%d]do get traffic info",__FUNCTION__,__LINE__);
				void *ret = CALL_INTERFACE(p_node,get_node_traffic,(void *)args,size,&g_user_ctx);
				if(ret == NULL){
					WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]get traffic info error",__FUNCTION__,__LINE__);
					return NULL;
				}
				return (void *)ret;
			}else{
				WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]no support func:%s",__FUNCTION__,__LINE__,func_str);
				return NULL;
			}		
	
	
		}
	
	else{
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]~~~~~~~~~~~~~~ no support func:%s",__FUNCTION__,__LINE__,func_str);
		return NULL;
	}
    return (void *)recv_bss;
}


APIRET wtcoreapi_parse_http_get_cmd(const char *url, const char *upload_data,int upload_data_size){
	APIRET ret = {
		.status  =-1,
		.data = NULL
	};
	WTSL_LOG_INFO(MODULE_NAME, "wtcoreapi_parse_http_get_cmd url:%s,upload_data:%s,upload_data_size:%d",url,upload_data,upload_data_size);
	List *url_list = list_create(sizeof(UrlData));
	g_url_list = url_list;
	char *tmp_url = strdup(url);
	char *token = strtok(tmp_url, "/");
	while(token != NULL){
		UrlData url_data = {.str = strdup(token)};
		list_insert_tail(url_list, &url_data);
		token = strtok(NULL, "/");
	}
	WTSL_LOG_DEBUG(MODULE_NAME,"find tests by index");
    for (int i = 0; i < list_size(url_list); i++) {
        const UrlData *sd = (const UrlData*)list_get(url_list, i);
        if (sd) {
            WTSL_LOG_DEBUG(MODULE_NAME,"index %d value:%s", i, sd->str);
        } else {
            WTSL_LOG_DEBUG(MODULE_NAME,"index %d find failed", i);
        }
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "url_list size:%d",url_list->size);
	
#ifdef OLD_API
	char geturl[128]={0};
	for (int i = 0; get_func_mappings[i].name != NULL; i++) {
		sprintf(geturl,"/%s/v%d/%s",RESTFUL_API_BASE,RESTFUL_API_VERSION,get_func_mappings[i].name);
		WTSL_LOG_INFO(MODULE_NAME, "geturl:%s",geturl);
		if(strncmp(geturl,url,strlen(url))==0 && strlen(url) == strlen(geturl)){
			WTSL_LOG_INFO(MODULE_NAME, "执行: %s -> ", get_func_mappings[i].name);

			
			ret.data = get_func_mappings[i].function((void *)upload_data);
			if(ret.data != NULL){
				ret.status = 0;
			}
			return ret;
		}
	}
#endif
	if(url_list->size < 3){
		WTSL_LOG_ERROR(MODULE_NAME, "Do not Support [%s]",url);
		return ret;
	}
	
	for (int i = 0; wtapi_get_func_mappings[i].apiname != NULL; i++) {
		char geturl[128]={0};
		sprintf(geturl,"/%s/v%d/%s",RESTFUL_API_BASE,RESTFUL_API_VERSION,wtapi_get_func_mappings[i].apiname);
		char *api_name_in_url = ((UrlData *)list_get(url_list,2))->str;
		WTSL_LOG_INFO(MODULE_NAME, "url:%s,geturl:%s,api_name_in_url:%s,url_len: %d,geturl_len: %d,strncmp:%d\n",url,geturl,api_name_in_url,strlen(url),strlen(geturl),strncmp(url,geturl,strlen(geturl)));
		if(strncmp(api_name_in_url,wtapi_get_func_mappings[i].apiname,strlen(api_name_in_url))==0 && strncmp(url,geturl,strlen(geturl))==0){
			WTSL_LOG_INFO(MODULE_NAME, "执行: %s -> ", wtapi_get_func_mappings[i].apiname);
			ret.data = wtapi_get_func_mappings[i].pfunc(url,(void *)upload_data,upload_data_size);
			if(ret.data != NULL){
				ret.status = 0;
			}
			list_destroy(url_list, NULL);
			return ret;
		}
	}
	WTSL_LOG_ERROR(MODULE_NAME, "Do not Support [%s]",url);
	return ret;
}
APIRET wtcoreapi_parse_http_post_cmd(const char *url, const char *data,int size){
	APIRET ret = {
		.status  =-1,
		.data = NULL
	};
	//WTSL_LOG_INFO(MODULE_NAME, "wtcoreapi_parse_http_post_cmd url:%s,data:%s,size:%d",url,data,size);
	char posturl[128]={0};

	List *url_list = list_create(sizeof(UrlData));
	g_url_list = url_list;
	char *tmp_url = strdup(url);
	char *token = strtok(tmp_url, "/");
	while(token != NULL){
		UrlData url_data = {.str = strdup(token)};
		list_insert_tail(url_list, &url_data);
		token = strtok(NULL, "/");
	}
	WTSL_LOG_DEBUG(MODULE_NAME,"按索引查找测试：\n");
    for (int i = 0; i < list_size(url_list); i++) {
        const UrlData *sd = (const UrlData*)list_get(url_list, i);
        if (sd) {
            WTSL_LOG_DEBUG(MODULE_NAME,"索引 %d 的值：%s\n", i, sd->str);
        } else {
            WTSL_LOG_DEBUG(MODULE_NAME,"索引 %d 查找失败\n", i);
        }
    }
	WTSL_LOG_DEBUG(MODULE_NAME, "url_list size:%d",url_list->size);

#ifdef OLD_API
	if(strcmp(url,"/api/v1/uploadFirmware")==0){
		//WTSL_LOG_INFO(MODULE_NAME, "######## upgradeFirmware 11111111 url:%s",url);
		ret.data = wtsl_core_uploadfirmware(data,size);
		if(ret.data != NULL){
			ret.status = 0;
		}
		return ret;		
	}
	for (int i = 0; post_func_mappings[i].name != NULL; i++) {
		sprintf(posturl,"/%s/v%d/%s",RESTFUL_API_BASE,RESTFUL_API_VERSION,post_func_mappings[i].name);
		WTSL_LOG_INFO(MODULE_NAME, "posturl:%s",posturl);
		if(strncmp(posturl,url,strlen(url))==0){
			WTSL_LOG_INFO(MODULE_NAME, "执行: %s -> ", post_func_mappings[i].name);
			ret.data = post_func_mappings[i].function((void *)data);
			if(ret.data != NULL){
				ret.status = 0;
			}
			return ret;
		}
	}

#endif

	for (int i = 0; wtapi_post_func_mappings[i].apiname != NULL; i++) {
		sprintf(posturl,"/%s/v%d/%s",RESTFUL_API_BASE,RESTFUL_API_VERSION,wtapi_post_func_mappings[i].apiname);
		WTSL_LOG_INFO(MODULE_NAME, "posturl:%s",posturl);
		if(strncmp(posturl,url,strlen(posturl))==0){
			WTSL_LOG_INFO(MODULE_NAME, "执行: %s -> ", wtapi_post_func_mappings[i].apiname);
			ret.data = wtapi_post_func_mappings[i].pfunc(url,(void *)data,size);
			if(ret.data != NULL){
				ret.status = 0;
			}
			return ret;
		}
	}
	WTSL_LOG_ERROR(MODULE_NAME, "Do not Support [%s]",url);
	return ret;
}

WTAPI_FUNC_MAP wtapi_get_func_mappings[] = {
	{"nodes",wtsl_core_get_nodes_functions},
	{NULL, NULL}      // 结束标志
};
WTAPI_FUNC_MAP wtapi_post_func_mappings[] = {
	{"nodes", wtsl_core_post_nodes_functions},
	{NULL, NULL}      // 结束标志
};