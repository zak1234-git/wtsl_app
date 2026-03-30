#include <stdio.h>
#include <string.h>

#include "wtsl_core_api.h"
#include "version.h"
#include "wtsl_log_manager.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include "wtsl_cfg_manager.h"

#define MODULE_NAME "core_api"

extern pthread_mutex_t auto_join_net_mutex;
extern pthread_cond_t auto_join_net_cond;

int wtsl_log_manager(int level){
	switch(level){
		case 7:
			WTSL_LOG_ERROR("SYSTEM" ,"[%s][%s][%d]\n",__FILE__,__func__,__LINE__);
			break;
		case 6:
			WTSL_LOG_WARNING("SYSTEM" ,"[%s][%s][%d]\n",__FILE__,__func__,__LINE__);
			break;
		default:
			WTSL_LOG_INFO("SYSTEM", "[%s][%s][%d]\n",__FILE__,__func__,__LINE__);
			break;
	}
	return 0;
}


int wtsl_core_get_version(int *vercode,char *commitid){
	*vercode = WTCORE_VERSION;
	strcpy(commitid,WTCORE_VERSION_STR);
	return 0;
}

int wtsl_core_get_node_type(){
	return 0;
}
/*
static int get_bss_info(SPLINK_INFO *info){
	FILE *fp = NULL;
	char buffer[64]={0};
	char cmd[64] ={0};
	// char domain_name[128]={0};
	strcpy(cmd,"iwpriv vap0 cfg show_bss");
	fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		if(strstr(buffer,"DOMAIN_NAME") != 0){
			if(fgets(buffer, sizeof(buffer), fp)){
				sscanf(buffer,"%s      %d        %d      %d   %d",info->node_info.basic_info.domain_name,&info->node_info.basic_info.cell_id,&info->node_info.basic_info.channel,&info->node_info.basic_info.rssi,&info->node_info.basic_info.domain_cnt);
				// fprintf(stderr,"[%s][%d] cell_id:%d, channel:%d, rssi:%d\n",__FUNCTION__,__LINE__,info->node_info.cell_id,info->node_info.channel,info->node_info.rssi);
			}else{
				fprintf(stderr,"[%s][%d] read DOMAIN_NAME fail\n",__FUNCTION__,__LINE__);
			}
			break;
		}
	}
	pclose(fp);
	return 0;
}

static int get_bw_info(SPLINK_INFO *info){
	FILE *fp = NULL;
	char buffer[64]={0};
	char cmd[64] ={0};
	char *ptr = NULL;
	strcpy(cmd,"iwpriv vap0 cfg get_bw");
	fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		ptr = strstr(buffer,"bw = ");
		if(ptr != NULL){
			sscanf(ptr,"bw = %d",&info->node_info.bw);
			// WTSL_LOG_DEBUG("SYSTEM", "[%s][%d] bw:%d\n",__FUNCTION__,__LINE__,info->node_info.bw);
			break;
		}
	}
	pclose(fp);
	return 0;
}


static int get_mac_by_cmd(const char *ifname, char *mac) {
    char cmd[64];
    FILE *fp;
    char line[256];
    // char *ptr;

    // 构造命令：通过ifconfig筛选ether字段
    snprintf(cmd, sizeof(cmd), "ifconfig %s | grep -o 'HWaddr [0-9a-fA-F:]*' | cut -d' ' -f2", ifname);
	// printf("get_mac_by_cmd cmd: %s\n", cmd);
    // 执行命令并读取输出
    fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen failed");
        return -1;
    }

    // 读取MAC地址
    if (fgets(line, sizeof(line), fp) == NULL) {
        perror("fgets failed");
        pclose(fp);
        return -1;
    }

    // 去除换行符并复制结果
    line[strcspn(line, "\n")] = '\0';
    strncpy(mac, line, 17);
    mac[17] = '\0';

    pclose(fp);
    return 0;
}

static int get_mac_addr(SPLINK_INFO *info){

	char mac[24]={0};
    const char *ifname = "vap0";

    if (get_mac_by_cmd(ifname, mac) != 0){
        fprintf(stderr,"get mac failed\n");
    }
	strcpy(info->node_info.mac,mac);
	info->node_info.mac[strlen(mac)] = '\0';
	// WTSL_LOG_DEBUG("SYSTEM", "[%s][%d] mac addr:%s\n",__FUNCTION__,__LINE__,info->node_info.mac);
	return 0;
}
*/

static int get_firmware_version(char *version, size_t size){

	if (!version || size == 0) {
		return -1;
	}

	FILE *fp = NULL;
	char buffer[128]={0};
	char cmd[128] = {0};
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_version", NET_VAP_NAME);

	fp = popen(cmd, "r");
	if (!fp) {
		return -1;
	}

	*version = '\0';
	while(fgets(buffer, sizeof(buffer), fp))
	{
		char *ptr = strstr(buffer,"device version:");
		if(ptr){
			char temp[32] = {0};
			if (sscanf(ptr, "device version:%31s", temp) == 1) {
				temp[4] = '\0';
				strcpy(version, temp);
				version[4] = '\0';
				break;
			}
		}
	}
	pclose(fp);
	return (*version != '\0') ? 0 : -1;
}	

static int get_hardware_version(){
	return 1;
}
static int get_os_version(){
	return 1;
}

static int get_software_app_version(){
	return VersionCode;
}

static int get_software_lib_version(){
	return WTCORE_VERSION;
}

static char *get_all_version(SPLINK_INFO *info){
	char firmware_ver[16];
	if (get_firmware_version(firmware_ver, sizeof(firmware_ver)) == 0) {
		//WTSL_LOG_INFO(MODULE_NAME, "firmware version : %s", firmware_ver);
	} else {
		WTSL_LOG_WARNING(MODULE_NAME, "Failed to get firmware version");
	}
	sprintf(info->node_info.basic_info.version,"v%d.%d.%d_%d.%s",
		get_hardware_version(),
		get_os_version(),
		get_software_app_version(),
		get_software_lib_version(),
		firmware_ver
	);
	// WTSL_LOG_DEBUG("SYSTEM", "[%s][%d] version:%s\n",__FUNCTION__,__LINE__,info->node_info.version);
	return info->node_info.basic_info.version;
}

/*
static int get_tfc_bw_info(SPLINK_INFO *info){
	FILE *fp = NULL;
	char buffer[64]={0};
	char cmd[64] ={0};
	char *ptr = NULL;
	strcpy(cmd,"iwpriv vap0 cfg get_tfc_bw");
	fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		ptr = strstr(buffer,"tfc_bw = ");
		if(ptr != NULL){
			sscanf(ptr,"tfc_bw = %d",&info->node_info.tfc_bw);
			break;
		}
	}
	pclose(fp);
	// WTSL_LOG_DEBUG("SYSTEM", "[%s][%d] tfc_bw:%d\n",__FUNCTION__,__LINE__,info->node_info.tfc_bw);
	return 0;
}
*/

static int get_view_users_info(SPLINK_INFO *info){
	int role = -1,state = -1,num = -1,scan = -1,roam = -1;
	static int last_user_num = 0;
	static bool add_mac = true;
	FILE *fp = NULL;
	char buffer[128]={0};
	char cmd[64] ={0};
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg view_users", NET_VAP_NAME);
	fp = popen(cmd, "r");

	while(fgets(buffer, sizeof(buffer), fp))
	{	
		//printf("buffer:%s\n", buffer);
		if(strstr(buffer,"vap_role") != 0){
			sscanf(buffer,"vap_role[%d], vap_state[%d], user_num[%d], scanning[%d], roamming[%d]",&role,&state,&num,&scan,&roam);
			//fprintf(stderr,"PPP vap_role[%d], vap_state[%d], user_num[%d], scanning[%d], roamming[%d]\n",role,state,num,scan,roam);
			if(last_user_num != 0 && last_user_num != num){
				info->need_refresh = 1;
			}else{
				info->need_refresh = 0;
			}
			last_user_num = num;
			info->link_state = state;
			//info->node_type = (role == 2) ? SP_GNODE : SP_TNODE;
			info->user_num = num;
		}		
		if(strstr(buffer, "ms") != NULL && info->node_type == SP_TNODE && add_mac)
		{
			sscanf(buffer,"%d %s", &info->uid, info->mac);
			//printf("info->uid: %d, info->mac: %s\n", info->uid, info->mac);			
			if(strcmp(info->mac, info->node_info.basic_info.auto_join_net_mac[0]) != 0)
			{	
				memcpy(info->node_info.basic_info.auto_join_net_mac[2], info->node_info.basic_info.auto_join_net_mac[1], 18);
				memcpy(info->node_info.basic_info.auto_join_net_mac[1], info->node_info.basic_info.auto_join_net_mac[0], 18);
				memcpy(info->node_info.basic_info.auto_join_net_mac[0], info->mac, 18);
				config_set("AUTO_JOIN_NET_MAC0", info->node_info.basic_info.auto_join_net_mac[0]);
				config_set("AUTO_JOIN_NET_MAC1", info->node_info.basic_info.auto_join_net_mac[1]);
				config_set("AUTO_JOIN_NET_MAC2", info->node_info.basic_info.auto_join_net_mac[2]);				
			}
			add_mac = false;	

		}
		if(info->link_state != 1 && info->node_type == SP_TNODE)
		{
			pthread_mutex_lock(&auto_join_net_mutex);
			pthread_cond_signal(&auto_join_net_cond);
			pthread_mutex_unlock(&auto_join_net_mutex);
			add_mac = true;
		}
	}
	pclose(fp);
	return 0;
}

static int get_rssi_info(SPLINK_INFO *info)
{
	FILE *fp = NULL;
	char buffer[64]={0};
	char cmd[64] ={0};
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_rssi", NET_VAP_NAME);
	fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		sscanf(buffer,"%d      %d",&info->uid,&info->node_info.basic_info.rssi);
	}
	pclose(fp);
	return 0;
}
/*
static int get_channel_info(SPLINK_INFO *info){
	FILE *fp = NULL;
	char buffer[64]={0};
	char cmd[64] ={0};
	char *ptr = NULL;
	strcpy(cmd,"iwpriv vap0 cfg get_channel");
	fp = popen(cmd, "r");
	while(fgets(buffer, sizeof(buffer), fp))
	{
		ptr = strstr(buffer,"channel = ");
		if(ptr != NULL){
			sscanf(ptr,"channel = %d",&info->node_info.channel);
			break;
		}
	}
	pclose(fp);
	// WTSL_LOG_DEBUG("SYSTEM", "[%s][%d] channel:%d\n",__FUNCTION__,__LINE__,info->node_info.channel);
	return 0;
}
*/
int wtsl_core_get_splink_info(SPLINK_INFO *info){
	get_view_users_info(info);
	//get_local_ip_info(info);
	//get_essid_info(info);
	//get_bw_info(info);
	//get_tfc_bw_info(info);
	int tmp_channel = slb_get_channel(NET_VAP_NAME);
	if(tmp_channel != 0 && info->node_info.basic_info.channel != tmp_channel)
	{
		WTSL_LOG_INFO(MODULE_NAME, "update channel %d", tmp_channel);
		info->node_info.basic_info.channel = tmp_channel;
		config_set_int("CHANNEL", info->node_info.basic_info.channel);
	}
	//get_mac_addr(info);
	get_all_version(info);
	if(info->node_type == SP_TNODE && info->link_state == 1){
		//get_bss_info(info);
		get_rssi_info(info);
	}	
	return info->link_state;
}