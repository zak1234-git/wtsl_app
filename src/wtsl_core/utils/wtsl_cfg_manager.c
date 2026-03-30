#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include "wtsl_cfg_manager.h"
#include "wtsl_core_slb_interface.h"
#include "wtsl_core_api.h"

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
extern SPLINK_INFO global_node_info;

// 字符串工具函数
static char* trim_whitespace(char* str) {
    if(!str) return NULL;
    
    char* end;
    
    // 去除首部空白
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    // 去除尾部空白
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

static int is_valid_key(const char* key) {
    if(!key || strlen(key) == 0) return 0;
    
    // 检查长度
    if(strlen(key) >= CONFIG_MAX_KEY_LENGTH) return 0;
    
    // 检查非法字符
    for(const char* p = key; *p; p++) {
        if (*p == '=' || *p == '#' || *p == '\n' || *p == '\r') {
            return 0;
        }
    }
    
    return 1;
}

static int is_valid_value(const char* value) {
    if(!value) return 0;
    
    // 检查长度
    if(strlen(value) >= CONFIG_MAX_VALUE_LENGTH) return 0;
    
    // 检查换行符
    if(strchr(value, '\n') != NULL || strchr(value, '\r') != NULL) {
        return 0;
    }
    
    return 1;
}

// 文件解析函数
static bool parse_config_line(char* line, char* key, char* value) {
    if(!line || !key || !value) {
        return false;
    }

	char work_line[MAX_LINE_LENGTH];
	strncpy(work_line, line, sizeof(work_line) - 1);
	work_line[sizeof(work_line) - 1] = '\0';
    
    char* trimmed = trim_whitespace(work_line);
    
    // 跳过空行和注释
    if(strlen(trimmed) == 0 || trimmed[0] == '#') {
        return false;
    }
    
    // 查找等号
    char* equals = strchr(trimmed, '=');
    if(!equals) {
        return false;
    }
    
    // 分割键值
    *equals = '\0';
    char* temp_key = trim_whitespace(trimmed);
    char* temp_value = trim_whitespace(equals + 1);
    
    // 验证键值
    if(!is_valid_key(temp_key) || !is_valid_value(temp_value)) {
        return false;
    }
    
    strncpy(key, temp_key, CONFIG_MAX_KEY_LENGTH - 1);
    strncpy(value, temp_value, CONFIG_MAX_VALUE_LENGTH - 1);
    
    return true;
}


int config_set(const char* key, char* value)
{
    if(!is_valid_key(key) || !is_valid_value(value))
    {
		return CONFIG_ERROR_INVALID_PARAM;
    }    	
    
	pthread_mutex_lock(&file_mutex);

	char temp_file[1024];
	snprintf(temp_file, sizeof(temp_file), "%s.tmp", ENV_CONFIG);
	FILE* temp_fp = fopen(temp_file, "w");
	if(!temp_fp)
	{
		pthread_mutex_unlock(&file_mutex);
		return CONFIG_ERROR_IO;
	}

	FILE* orig_fp = fopen(ENV_CONFIG, "a+");
	if(orig_fp)
		fseek(orig_fp, 0, SEEK_SET);

	int key_updated = 0;
	char line[CONFIG_MAX_LINE_LENGTH];

	if(orig_fp)
	{
		while(fgets(line, sizeof(line), orig_fp))
		{
			char file_key[CONFIG_MAX_KEY_LENGTH];
			char file_value[CONFIG_MAX_VALUE_LENGTH];

			if(parse_config_line(line, file_key, file_value))
			{	
				if(strcmp(file_key, key) == 0)
				{
					//找到现有键，替换为新值
					fprintf(temp_fp, "%s=%s\n", key, value);
					key_updated = 1;
				}
				else
				{
					//保持其他键不变
					fprintf(temp_fp, "%s", line);
				}
			}
			else
			{
				//保持注释和空行不变
				fprintf(temp_fp, "%s", line);
			}
		}
		fclose(orig_fp);
	}

	if(!key_updated)
	{
		fprintf(temp_fp, "%s=%s\n", key, value);
	}
		

	fflush(temp_fp);
	fclose(temp_fp);

	if(rename(temp_file, ENV_CONFIG) != 0)
	{
		remove(temp_file);
		pthread_mutex_unlock(&file_mutex);
		return CONFIG_ERROR_IO;
	}
	
	pthread_mutex_unlock(&file_mutex);	
    return CONFIG_SUCCESS;
}

int config_get(const char* key, char* value, size_t value_len)
{
    if(!is_valid_key(key) || !is_valid_value(value) || value_len == 0)
    {
		return CONFIG_ERROR_INVALID_PARAM;
    }
        
        
	pthread_mutex_lock(&file_mutex);


    //value[0] = '\0';

    FILE* file = fopen(ENV_CONFIG, "r");
    if(!file)
    {
		pthread_mutex_unlock(&file_mutex);
		return CONFIG_ERROR_NOT_FOUND;
    }

    char line[CONFIG_MAX_LINE_LENGTH];
    config_result_t result = CONFIG_ERROR_NOT_FOUND;

    //逐行查找目标键
    while(fgets(line, sizeof(line), file))
    {
    	char file_key[CONFIG_MAX_KEY_LENGTH];
    	char file_value[CONFIG_MAX_VALUE_LENGTH];

    	if(parse_config_line(line, file_key, file_value))
    	{
			if(strcmp(file_key, key) == 0)
			{
				strncpy(value, file_value, value_len - 1);
				value[value_len - 1] = '\0';
				result = CONFIG_SUCCESS;
				break; //找到第一个匹配项就返回
			}
    	}
    }
    
	fclose(file);
	pthread_mutex_unlock(&file_mutex);
    return result; 
}

int config_get_int(const char* key, int* value)
{
	if(!value)
		return CONFIG_ERROR_INVALID_PARAM;

	char str_value[CONFIG_MAX_VALUE_LENGTH];
	config_result_t result = config_get(key,str_value, sizeof(str_value));
	if(result != CONFIG_SUCCESS)
		return result;

	char* endptr;
	long num = strtol(str_value, &endptr, 10);

	//验证是否转化成功
	if(endptr == str_value || *endptr != '\0')
		return CONFIG_ERROR_FORMAT;
    
    *value = (int)num;	
	return CONFIG_SUCCESS;
}

static STORAGE_TYPE detect_storage_type(void)
{
	if(access("/sys/block/mtdblock0", R_OK) == 0)
	{
		return STORAGE_TYPE_NANDFLASH;
	}
	if(access("/sys/block/mmcblk0", R_OK) == 0)
	{
		return STORAGE_TYPE_EMMC;
	}

	printf("not detected storage type\n");
	return STORAGE_TYPE_UNKNOW;
}

static void read_vap0_mac(char* mac, int size)
{

	STORAGE_TYPE type = detect_storage_type();
	switch(type)
	{
		case STORAGE_TYPE_NANDFLASH:
			system("nanddump -n -l 4096 /dev/mtd10 | strings | grep vap0_mac | sed 's/vap0_mac=//' > /home/wt/vap0_mac");
			break;
		case STORAGE_TYPE_EMMC:
			//system();
			break;
		default:
			break;
	}
	FILE* file = fopen("/home/wt/vap0_mac", "r");
	if(!file)
	{
		perror("open /home/wt/vap0_mac failed");
		return;
	}
	fread(mac, 1, size - 1, file);
	mac[17] = '\0';

	fclose(file);
}

int config_read()
{
	//配置文件读取节点基本参数
	WTSLNodeBasicInfo* node = &global_node_info.node_info.basic_info;
	
	config_get_int("BW", &node->bw);
	config_get_int("TFC_BW", &node->tfc_bw);
	config_get_int("CHANNEL", &node->channel);
	config_get_int("TYPE", &node->type);
	config_get_int("TYPE", (int*)&global_node_info.node_type);
	config_get_int("LOG_PORT", &node->log_port);
	config_get_int("AUTO_JOIN_NET", &node->auto_join_net_flag);
	config_get_int("DHCP_ENABLE", &node->dhcp_enable);
	config_get_int("AIFH_ENABLE", &node->aifh_enable);

	config_get("NAME", node->name, sizeof(node->name));
	config_get("IP", node->ip, sizeof(node->ip));
	//config_get("MAC", node->mac, sizeof(node->mac));
	read_vap0_mac(node->mac, sizeof(node->mac));
	config_get("NET_MANAGE_IP", node->net_manage_ip, sizeof(node->net_manage_ip));
	config_get("AUTO_JOIN_NET_MAC0", node->auto_join_net_mac[0], sizeof( node->auto_join_net_mac[0]));
	config_get("AUTO_JOIN_NET_MAC1", node->auto_join_net_mac[1], sizeof( node->auto_join_net_mac[1]));
	config_get("AUTO_JOIN_NET_MAC2", node->auto_join_net_mac[2], sizeof( node->auto_join_net_mac[2]));

	config_get_int("BRIDGE_IF_NUM", &node->bridge_interface_num);
	for(int i = 0; i < node->bridge_interface_num; i++)
	{
		char str[32];	
		snprintf(str, sizeof(str), "BRIDGE_IF%d", i);
		config_get(str, node->bridge_interfaces[i], sizeof(node->bridge_interfaces[i]));
		printf("get bridge_interfaces[%d]: %s\n", i, node->bridge_interfaces[i]);
	}
	
	//配置文件读取节点高级参数
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	config_get_int("DEVID", &adv_info->devid);
	config_get_int("CELL_ID", &adv_info->cell_id);
	config_get_int("CC_SPOS", &adv_info->cc_offset);
	config_get_int("CP_TYPE", &adv_info->mib_params.cp_type);
	config_get_int("SYMBOL_TYPE", &adv_info->mib_params.symbol_type);
	config_get_int("SYSMSG_PERIOD", &adv_info->mib_params.sysmsg_period);
	config_get_int("S_CFG_IDX", &adv_info->mib_params.s_cfg_idx);	

	//配置SLE节点参数信息
	config_get_int("SLE_TYPE", &node->sle_type);
	config_get("SLE_NAME", node->sle_name, sizeof(node->sle_name));

	//配置文件读取其他参数
	//.......
	
    return 0;
}

int config_set_int(const char* key, int value)
{
	char str_value[32];
	snprintf(str_value, sizeof(str_value), "%d", value);
	return config_set(key, str_value);
} 
    