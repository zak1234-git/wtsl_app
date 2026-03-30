#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <dirent.h>
//#include <linux/wireless.h>
#include "wtsl_core_slb_interface.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_dataparser.h"
#include "wtsl_cfg_manager.h"
#include "wtsl_core_api.h"
#include "wtsl_log_manager.h"

extern char *js_cfg_path;
extern SPLINK_INFO global_node_info;
extern int sync_ip_to_js_cfg(const char *path,WTSLNodeBasicInfo* param);
static int load_kernel_module(char *module_path);
static bool is_valid_device_name(char* name);


int slb_set_msc_bound(char *dev_name, mcs_bound_t mcs_bound);
int slb_set_mib_params(char *dev_name, mib_params_t mib_params);
int slb_get_mib_params(char *dev_name, mib_params_t *mib_params);


#define MODULE_NAME "slb_inf"



char *modules[] ={
	"ksecurec.ko",
	"plat_gf61.ko",
	"gt_gf61.ko",
	"hi_cipherserver.ko",
	NULL
};

char *bak_modules[] ={
	"/lib/mys/ko/hi_pcie.ko",
	"ksecurec.ko",
	"plat_gf61.ko",
	"gt_gf61.ko",
	"hi_cipherserver.ko",
	NULL
};

int get_wt_vap_interface_count(void)
{
	const char *net_dir = "/sys/class/net";
	DIR *dir;
	struct dirent *entry;
	int count = 0;

	dir = opendir(net_dir);
	if (!dir) {
		// 无法访问网络接口目录，视为无接口
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		// 检查是否以 "wt_vap" 开头
		if (strncmp(entry->d_name, "wt_vap", 6) == 0) {
			const char *p = entry->d_name + 6;

			// 确保后缀是单个数字 '0' 到 '5'
			if (*p >= '0' && *p <= '5' && p[1] == '\0') {
				count++;
			}
		}
	}

	closedir(dir);

	if (count == 0) {
		return -1;
	} else if (count == 1) {
		return 0;
	} else {
		return count;
	}
}

int init_node() 
{
	int ret = -1;
	const char *firmware_dir_old = "/home/firmware";
	const char *fallback_dir = "/home/wt/firmware";
	const char *target_dir = "/tmp";

	struct stat st;

	if (stat(firmware_dir_old, &st) == 0 && S_ISDIR(st.st_mode)) {
		WTSL_LOG_DEBUG(MODULE_NAME, "%s firmware dir exist", firmware_dir_old);
		char command[128];
		snprintf(command, sizeof(command), "cp -rf %s/* %s/", firmware_dir_old, target_dir);
		ret = system(command);
		if (ret != 0) {
			WTSL_LOG_ERROR(MODULE_NAME, "COPY firmware %s to %s failed", firmware_dir_old, target_dir);
		} else {
			WTSL_LOG_INFO(MODULE_NAME, "COPY firmware %s to %s success", firmware_dir_old, target_dir);
		}
	} else {
		WTSL_LOG_DEBUG(MODULE_NAME, "%s firwmare dir not exist, Try to use a new directory to copy firmware", firmware_dir_old);
		char command[128];
		snprintf(command, sizeof(command), "cp -rf %s/* %s/", fallback_dir, target_dir);
		int ret = system(command);
		if (ret != 0) {
			WTSL_LOG_ERROR(MODULE_NAME, "COPY firmware %s to %s failed", fallback_dir, target_dir);
		} else {
			WTSL_LOG_INFO(MODULE_NAME, "COPY firmware %s to %s success", fallback_dir, target_dir);
		}
	}

	if(chdir("/tmp") == -1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Failed to change directory to /home/wt/firmware");
		return -1;
	}


	ret = system("cp preResForSLDev.bin /home/cli/");	
	ret |= system("chmod +x /home/cli/preResForSLDev.bin");

	int found = 0;
	FILE *file = fopen("/etc/rc.d/rc.local", "r");
	if (!file) {
		WTSL_LOG_ERROR(MODULE_NAME, "not found /etc/rc.d/rc.local");
	} else {
		char *line = NULL;
		size_t len = 0;
		ssize_t read;

		while ((read = getline(&line, &len, file)) != -1) {
			if (line[read - 1] == '\n') {
				line[read - 1] = '\0';
			}

			if (strstr(line, "insmod /lib/mys/ko/hi_pcie.ko") != NULL) {
				found = 1;
				break;
			}
		}

		free(line);
		fclose(file);
	}

	for(int i = 0; modules[i]; i++)
	{
		if (found == 1) {
			if(load_kernel_module(modules[i]) != 0)
			{
				//return -1; //任一模块加载失败则终止
			}
		} else if (found == 0) {
			if (load_kernel_module(bak_modules[i]) != 0)
			{
				//return -1; //任一模块加载失败则终止
			}
		}
	}
	
	WTSLNodeBasicInfo* node_params = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	
	slb_create_device(NET_VAP_NAME, node_params->type);
	sleep(1);
	WTSL_LOG_INFO(MODULE_NAME, "node_params->mac: %s", node_params->mac);
	set_mac_address(NET_VAP_NAME, node_params->mac);

	ret |= system("chmod +x uspd");

	ret |= system("./uspd &");

	//初始化usb虚拟网口
	//ret |= system("/etc/network/usb_eth_init.sh");
	
	slb_ifconfig(NET_VAP_NAME, 0);

	slb_set_channel(NET_VAP_NAME, node_params->channel);
		
	slb_set_bw(NET_VAP_NAME, node_params->bw);
		
	slb_set_tfc_bw(NET_VAP_NAME, node_params->tfc_bw);
	
	for(int i = 0; strlen(adv_info->mcs_bound_table[i].user_mac) != MAC_ADDR_LEN -1; i++)
	{	
		slb_set_msc_bound(NET_VAP_NAME, adv_info->mcs_bound_table[i]);
	}
	
	slb_set_cc_start_pos(NET_VAP_NAME, adv_info->cc_offset);
	
	slb_set_cell_id(NET_VAP_NAME, adv_info->cell_id);
	
	slb_set_essid(NET_VAP_NAME, node_params->name);

	slb_set_mib_params(NET_VAP_NAME, adv_info->mib_params);

	slb_set_tx_power(NET_VAP_NAME, adv_info->power);

	slb_set_sec_auth_pwd(NET_VAP_NAME, adv_info->password);

	if(node_params->type == NODE_TYPE_SLB_G)
	{
		slb_set_acs_enable(NET_VAP_NAME, node_params->aifh_enable);
	}

	init_net_bridge(NET_BRIDGE_NAME, node_params->ip);
		
	slb_ifconfig(NET_VAP_NAME, 1);

	if (node_params->type == 1) {
		slb_set_sec_exch_cap(NET_VAP_NAME, adv_info->sec_exch_cap);
	}

	adv_info->devid = get_wt_vap_interface_count();

	if(ret != 0){
		WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
	}	
		
	return 0;
}

/**
* @brief 创建SLB接入层网络设备

* @param name 网络设备（1-16字节，只含可见字符）
* @param type SLB角色（0:G, 1:T） 
* @return int 0表示成功， -1表示失败，errno设置错误原因
*/
int slb_create_device(char* name, int type)
{
	char command[64] = {0};
	char slb_role[6];

	//参数有效性检查
	if(!is_valid_device_name(name))
	{
		errno = EINVAL;
		WTSL_LOG_ERROR(MODULE_NAME, "invalid device name: must be 1-16 visable characters");
		return -1;
	}
	
	switch(type)
	{
		case NODE_TYPE_SLB_G:
			snprintf(slb_role, sizeof(slb_role), "%s", "gnode");
			break;
		case NODE_TYPE_SLB_T:
			snprintf(slb_role, sizeof(slb_role), "%s", "tnode");
			break;
		default:
			snprintf(slb_role, sizeof(slb_role), "%s", "tnode");
			break;			
	}

	//构建字符串命令
	snprintf(command, sizeof(command), GT0_CMD_FORMAT, name, slb_role, GT0_FILE_PATH);

	//执行命令
	int ret = system(command);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to create %s device: %s", slb_role, name);
		return -1;
	}

	return ret;
}

/**
* @brief 注销已经创建的SLB接入层网络设备

* @param name 网络设备（1-16字节，只含可见字符） 
* @return int 0表示成功， -1表示失败，errno设置错误原因
*/
int slb_destory_device(char* name)
{
	char command[64] = {0};

    //参数有效性检查
	if(!is_valid_device_name(name))
	{
		errno = EINVAL;
		WTSL_LOG_ERROR(MODULE_NAME, "invalid device name: must be 1-16 visable characters");
		return -1;
	}
	
	//构建字符串命令
	snprintf(command, sizeof(command), DESTROY_CMD_FORMAT, name, GT0_FILE_PATH);

	//执行命令
	int ret = system(command);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to destory device: %s", name);
		return -1;
	}
	
	return ret;
}

/**
* @brief 验证设备名称是否合法

* @param name 待验证的设备名称
* @return true 名称合法
* @return false 名称非法
*/
static bool is_valid_device_name(char* name)
{
	WTSL_LOG_INFO(MODULE_NAME, "%s: %d", name, strlen(name));
	if(name == NULL || strlen(name) == 0 || strlen(name) > MAX_DEVICE_NAME_LEN)
		return false;

	//检查是否包含可见字符
	for(char* p = name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			return false;
		}
	}

	return true;
}

/**
* @brief 设置网络设备的MAC地址

* @param dev_name 网络设备名称
* @param mac_addr MAC地址字符串 格式为: "xx:xx:xx:xx:xx:xx" 
* @return int 0表示成功， -1表示失败
*/
int set_mac_address(char *dev_name, char *mac_addr)
{
	if(dev_name == NULL || mac_addr == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Device name or MAC address is NULL");
		return -1;
	}

	//验证MAC地址格式
	if(strlen(mac_addr) != MAC_ADDR_LEN -1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid MAC address format. Expected format: xx:xx:xx:xx:xx:xx");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);

	char cmd[MAX_CMD_LEN];
	snprintf(cmd, MAX_CMD_LEN, "ifconfig %s hw ether %s", dev_name, mac_addr);
	WTSL_LOG_INFO(MODULE_NAME, "%s", cmd);
	//执行命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set MAC address for device %s", dev_name);
		return -1;
	}

	return 0;

}

/**
* @brief 获取网络设备的MAC地址

* @param dev_name 网络设备名称
* @param mac_addr 存储MAC地址的缓冲区，至少需要MAC_ADDR_LEN大小
* @return int 0表示成功， -1表示失败
*/

int get_mac_address(char *dev_name, char *mac_addr)
{
	if(dev_name == NULL || mac_addr == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Device name or MAC address is NULL");
		return -1;
	}

	char cmd[MAX_CMD_LEN];
	snprintf(cmd, MAX_CMD_LEN, "ifconfig %s | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit]]{1,2}'", dev_name);

	//使用popen执行命令并读取输出
	FILE *fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to execute command");
		return -1;	
	}

	if(fgets(mac_addr, MAC_ADDR_LEN, fp) == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to get MAC address for device %s", dev_name);
		pclose(fp);
		return -1;
	}

	//去除可能的换行符
	mac_addr[strcspn(mac_addr, "\n")] = '\0';

	pclose(fp);
	return 0;	

}

/**
* @brief 设置网络设备的IP

* @param dev_name 网络设备名称
* @param ip_addr IP地址字符串
* @return int 0表示成功，-1表示失败
*/
int set_ip_address(char *dev_name, char *ip_addr)
{
	if(dev_name == NULL || ip_addr == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Device name or IP address is NULL");
		return -1;
	}

	char cmd[MAX_CMD_LEN];
	snprintf(cmd, MAX_CMD_LEN, "ifconfig %s %s", dev_name, ip_addr);

	//执行命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set IP address for device %s", dev_name);
		return -1;
	}

	return 0;

}

/**
* @brief 获取网络设备的MAC地址

* @param dev_name 网络设备名称
* @param ip_addr 存储IP地址的缓冲区
* @param buf_size 缓冲区大小
* @return int 0表示成功， -1表示失败
*/
int get_ip_address(char *dev_name, char *ip_addr, size_t buf_size)
{
	if(dev_name == NULL || ip_addr == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Device name or IP address is NULL");
		return -1;
	}

	char cmd[MAX_CMD_LEN];
	snprintf(cmd, MAX_CMD_LEN, "ifconfig %s | grep -w 'inet' | awk '{print $2}'", dev_name);

	//使用popen执行命令并读取输出
	FILE *fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to execute command");
		return -1;	
	}

	if(fgets(ip_addr, buf_size, fp) == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to get IP address for device %s", dev_name);
		pclose(fp);
		return -1;
	}

	//去除可能的换行符
	ip_addr[strcspn(ip_addr, "\n")] = '\0';

	pclose(fp);
	return 0;	

}

/**
* @brief 设备网口启动/停止

* @param dev_name 网络设备名称
* @param up 0: down, 1: up
* @return int 0表示成功， -1表示失败
*/
int slb_ifconfig(char *dev_name, bool up)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "ifconfig %s %s", dev_name, up ? "up" : "down");

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to execute ifconfig");
		return -1;
	}

	return 0;
}

int slb_set_channel(char *dev_name, int channel)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

    if(channel != 41 && channel != 125 && channel != 209 && channel != 291 && channel != 375 && channel != 459 && channel != 541 && channel != 625
	 && channel != 709 && channel != 791 && channel != 1375 && channel != 1459 && channel != 1541 && channel != 1625 && channel != 1709 && channel != 1791
	  && channel != 1875 && channel != 1959 && channel != 2041 && channel != 2125 && channel != 2209 && channel != 2291 && channel != 2479 && channel != 2563
	   && channel != 2645 && channel != 2729 && channel != 2813)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid channel");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"channel %d\"", dev_name, channel);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set channel");
		return -1;
	}

	//确保打开网口
	//slb_ifconfig(dev_name, 1);
	
	return 0;
}

int slb_get_channel(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}	

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_channel", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	int channel = 0;
	while(fgets(line, sizeof(line), fp))
    {        
        //解析'='
        char* equals = strchr(line, '=');
        if(!equals)
        {
            continue;
        }
        channel = atoi(equals + 1);   
    }
	pclose(fp);
	return channel;
}


int slb_set_bw(char *dev_name, int bw)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(bw != 20 && bw != 40 && bw != 80)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid bw");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"bw %d\"", dev_name, bw);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set bw");
		return -1;
	}

	return 0;
}

int slb_get_bw(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}	

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_bw", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	int bw = 0;
	while(fgets(line, sizeof(line), fp))
    {        
        //解析'='
        char* equals = strchr(line, '=');
        if(!equals)
        {
            continue;
        }
        bw = atoi(equals + 1);
    }
	pclose(fp);
	return bw;
}


int slb_set_tfc_bw(char *dev_name, int tfc_bw)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(tfc_bw != 20 && tfc_bw!= 40 && tfc_bw != 80)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid tfc_bw");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"tfc_bw %d\"", dev_name, tfc_bw);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set tfc_bw");
		return -1;
	}

	return 0;
}

int slb_get_tfc_bw(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}	

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_tfc_bw", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	int tfc_bw = 0;
	while(fgets(line, sizeof(line), fp))
    {        
        //解析'='
        char* equals = strchr(line, '=');
        if(!equals)
        {
            continue;
        }
        tfc_bw = atoi(equals + 1);   
    }
	pclose(fp);
	return tfc_bw;
}


int slb_set_msc_bound(char *dev_name, mcs_bound_t mcs_bound)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//验证user MAC地址格式
	if(strlen(mcs_bound.user_mac) != MAC_ADDR_LEN -1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid %s format. Expected format: xx:xx:xx:xx:xx:xx", mcs_bound.user_mac);
		return -1;
	}
	
	if(mcs_bound.min_mcs < 0 || mcs_bound.min_mcs > 31)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid %d. Expected value [0,31]", mcs_bound.min_mcs);
		return -1;
	}

    if(mcs_bound.max_mcs < 0 || mcs_bound.max_mcs > 31)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: %d. Expected value [0,31]", mcs_bound.max_mcs);
		return -1;
	}
	
	
	//构建命令
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"set_mcs_bound %s %d %d\"", dev_name, mcs_bound.user_mac, mcs_bound.min_mcs, mcs_bound.max_mcs);
	
	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set mcs_bound %s %d %d", mcs_bound.user_mac, mcs_bound.min_mcs, mcs_bound.max_mcs);
		return -1;
	}
	
	return 0;
}


int slb_set_cc_start_pos(char *dev_name, int cc_offset)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(cc_offset < 0 || cc_offset > 3)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid cc_offset. Expected value [0,3]");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"cc_start_pos %d\"", dev_name, cc_offset);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set cc start pos");
		return -1;
	}

	return 0;
}

int slb_get_cc_start_pos(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_cc_start_pos", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	int cc_offset = 0;
	while(fgets(line, sizeof(line), fp))
    {        
        //解析'='
        char* equals = strchr(line, '=');
        if(!equals)
        {
            continue;
        }
        cc_offset = atoi(equals + 1);   
    }
	pclose(fp);
	return cc_offset;
}

int slb_get_cell_id(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_cell_id", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	int cell_id = 0;
	while(fgets(line, sizeof(line), fp))
    {        
        //解析'='
        char* equals = strchr(line, '=');
        if(!equals)
        {
            continue;
        }
        cell_id = atoi(equals + 1);   
    }
	pclose(fp);
	return cell_id;

}

int slb_set_cell_id(char *dev_name, int id)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(id < 0 || id > 19)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid id. Expected value [0,19]");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);


	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"cell_id %d\"", dev_name, id);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set cell id");
		return -1;
	}

	return 0;
}

int slb_set_essid(char *dev_name, char *essid_str)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(essid_str == NULL || strlen(essid_str) == 0 || strlen(essid_str) > MAX_ESSID_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid essid");
		return -1;
	}
		
#ifdef  ESSID_ASCII
	//检查是否包含可见字符
	for(char* p = essid_str; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid essid. Invisible ASCII characters");
			return -1;
		}
	}
#endif

	//确保关闭网口
	slb_ifconfig(dev_name, 0);


	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"essid %s\"", dev_name, essid_str);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set cell id");
		return -1;
	}

	return 0;
}

int slb_get_acs_enable(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_acs_enable", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[128] = {0};
	unsigned int value = 0;
	if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "[SUCC]acs_enable");
		if (pos) {
			int v;
			if (sscanf(pos, "[SUCC]acs_enable = %d", &v) == 1) {
				value = (unsigned int)v;
			}
		}
	}
	pclose(fp);
	return value;
}

int slb_set_acs_enable(char *dev_name, int enable)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	if(enable != 0 && enable != 1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid power. Expected value [0, 1]");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_acs_enable %d\"", dev_name, enable);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set_acs_enable");
		return -1;
	}

	return 0;
}


int slb_set_mib_params(char *dev_name, mib_params_t mib_params)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(mib_params.cp_type < 0 || mib_params.cp_type > 3)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid cp_type. Expected value [0,3]");
		return -1;
	}

	if(mib_params.cp_type == 0 || mib_params.cp_type == 1)
	{
		if(mib_params.s_cfg_idx == 0 || mib_params.s_cfg_idx == 1)
		{
			
			if(mib_params.symbol_type < 2 || mib_params.symbol_type > 5)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [2,5],symbol_type:%d",mib_params.symbol_type);
				return -1;
			}
		}
		else if (mib_params.s_cfg_idx == 2)
		{
			if(mib_params.symbol_type < 1 || mib_params.symbol_type > 5)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [1,5]");
				return -1;
			}			
		}			
	}

	if(mib_params.cp_type == 2)
	{
		if(mib_params.s_cfg_idx == 0 || mib_params.s_cfg_idx == 1)
		{
			if(mib_params.symbol_type < 2 || mib_params.symbol_type > 4)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [2,4]");
				return -1;
			}
		}
		else if (mib_params.s_cfg_idx == 2)
		{
			if(mib_params.symbol_type < 1 || mib_params.symbol_type > 4)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [1,4]");
				return -1;
			}			
		}			
	}

	if(mib_params.cp_type == 3)
	{
		if(mib_params.s_cfg_idx == 0 || mib_params.s_cfg_idx == 1)
		{
			if(mib_params.symbol_type < 2 || mib_params.symbol_type > 3)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [2,3]");
				return -1;
			}
		}
		else if (mib_params.s_cfg_idx == 2)
		{
			if(mib_params.symbol_type < 1 || mib_params.symbol_type > 3)
			{
				WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid symbol_type. Expected value [1,3]");
				return -1;
			}			
		}			
	}
	if(mib_params.sysmsg_period < 0 || mib_params.sysmsg_period > 3)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid sysmsg_period. Expected value [0,3]");
		return -1;
	}

	if(mib_params.s_cfg_idx < 0 || mib_params.s_cfg_idx > 2)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid s_cfg_idx. Expected value [0,2]");
		return -1;
	}


	//确保关闭网口
	slb_ifconfig(dev_name, 0);


	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_mib_params %d %d %d %d\"", dev_name, mib_params.cp_type, mib_params.symbol_type, mib_params.sysmsg_period, mib_params.s_cfg_idx);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set mib params");
		return -1;
	}
	//确保打开网口
	slb_ifconfig(dev_name, 1);

	return 0;
}

int slb_get_mib_params(char *dev_name, mib_params_t *mib_params)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_mib_params", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {        
        pclose(fp);
		return -1;
    }
	pclose(fp);

	char *pos = strstr(line, "[SUCC]");
	if (!pos) {
		return -1;
	}

	// 跳过 [SUCC]开始解析
	pos += 6;
	int cp_type, symbol_cfg, sysmsg_period, s_cfg_idx;
	int matched = sscanf(pos,
						"cp_type = %d, symbol_cfg = %d, sysmsg_period = %d, s_cfg_idx = %d",
						&cp_type, &symbol_cfg, &sysmsg_period, &s_cfg_idx);

	if (matched != 4) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: parse mib params failed");
		return -1;
	}

	// 填充结构体
	mib_params->cp_type			= cp_type;
	mib_params->symbol_type 	= symbol_cfg;
	mib_params->sysmsg_period 	= sysmsg_period;
	mib_params->s_cfg_idx		= s_cfg_idx;

	return 0;
}

int slb_set_sec_auth_pwd(char *dev_name, char *password)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
	
	if(password == NULL || strlen(password) > MAX_PASSWORD_LEN || strlen(password) < MIN_PASSWORD_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid password");
		return -1;
	}
		

	//检查是否包含可见字符
	for(char* p = password; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid password. Invisible ASCII characters");
			return -1;
		}
	}

    //构建命令
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_sec_auth_pwd %s\"", dev_name, password);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set sec auth pwd");
		return -1;
	}

	return 0;
}

int slb_t_node_scan(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}
	
	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"scan\"", dev_name);
		
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to t_node scan");
		return -1;		
	}
	
	return 0;
}

char* slb_t_node_show_bss(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return NULL;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return NULL;
		}
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"show_bss\"", dev_name);

	//执行系统命令并返回输出的字符串
	return system_output_to_string(cmd);
}



int slb_t_node_join_bssid(char *dev_name, char *gnode_mac_addr)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}
	
	//验证MAC地址格式
	if(strlen(gnode_mac_addr) != MAC_ADDR_LEN -1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid GNode MAC address format. Expected format: xx:xx:xx:xx:xx:xx");
		return -1;
	}
	
	//构建命令
	char cmd[64];	
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"join_bssid %s\"", dev_name, gnode_mac_addr);	
	//sleep(2);
	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to t_node join_bssid");
		return -1;
	}
	return 0;
}

int slb_t_node_start_join(char *dev_name, int bss_idx)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}
	
	//构建命令
	char cmd[64];	
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"start_join %d\"", dev_name, bss_idx);	

	//确保网口起来
	slb_ifconfig(dev_name, 1);
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to t_node start_join");
		return -1;
	}
	return 0;
}

int slb_view_user(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"view_users\"", dev_name);
	//sleep(2);
	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to view_user");
		return -1;
	}

	return 0;
}

int slb_set_tx_power(char *dev_name, int power)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	if(power < -310 || power > 250)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid power. Expected value [-310,250]");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg \"set_tx_power %d\"", dev_name, power);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set_tx_power");
		return -1;
	}

	return 0;
}

int slb_get_real_power(char *dev_name, real_power_t *real_power)
{
		//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg get_real_pow", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {        
        pclose(fp);
		return -1;
    }
	pclose(fp);

	char *pos = strstr(line, "[SUCC]");
	if (!pos) {
		return -1;
	}

	// 跳过 [SUCC]开始解析
	pos += 6;
	int real_pow, exp_pow;
	int matched = sscanf(pos,
						"real_pow=%dexp_pow=%d",
						&real_pow, &exp_pow);

	if (matched != 2) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: parse slb timestamp failed");
		return -1;
	}

	// 填充结构体
	real_power->real_pow= real_pow;
	real_power->exp_pow = exp_pow;

	return 0;
}

int slb_set_sec_auth_psk(char *dev_name, char *auth_psk)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	if(auth_psk == NULL || strlen(auth_psk) == 0 || strlen(auth_psk) > FIXED_PSK_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid essid");
		return -1;
	}

	//构建命令
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_sec_auth_psk %s\"", dev_name, auth_psk);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set sec auth psk");
		return -1;
	}

	return 0;
}

int slb_set_sec_exch_cap(char *dev_name, int exch_cap)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	if (exch_cap < 1 || exch_cap > 33) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid input param. param[34](index[0]) range should be [1, 33]");
		return -1;
	}

	//构建命令
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"set_sec_exch_cap %d\"", dev_name, exch_cap);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set sec auth psk");
		return -1;
	}

	return 0;
}

int slb_get_sec_exch_cap(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_sec_exch_cap", dev_name);

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return -1;
	}

	char line[128] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {
		pclose(fp);
		return -1;
	}
	pclose(fp);

	char *pos = strstr(line, "key_exch_cap[0x");
	if (!pos) {
		return -1;
	}

	pos += 16;

	// 提取十六进制字符串 (直到遇到']')
	char hex_str[16] = {0};
	char *end = strchr(pos, ']');
	if (!end) {
		return -1;
	}

	size_t hex_len = end - pos;
	if (hex_len >= sizeof(hex_str)) {
		return -1;
	}

	strncpy(hex_str, pos, hex_len);
	hex_str[hex_len] = '\0';

	// 转换为整数
	char *hex_end;
	long val = strtol(hex_str, &hex_end, 16);

	// 检查转换是否完整
	if (*hex_end != '\0' || val < 0) {
		return -1;
	}

	return (int)val;
}

int slb_get_sec_sec_cap(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_sec_sec_cap", dev_name);

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return -1;
	}

	char line[128] = {0};
	unsigned int value = 0;
	if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "sec_capabilities[0x");
		if (pos) {
			int v;
			if (sscanf(pos, "sec_capabilities[0x%8x]", &v) == 1) {
				value = (unsigned int)v;
			}
		}
	}
	pclose(fp);
	return value;
}

int slb_get_lce_mode(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_lce_mode", dev_name);

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return -1;
	}

	char line[128] = {0};
	int mode = -1;
    if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "defaut data lce mode:");
		if (pos) {
			if (sscanf(pos, "defaut data lce mode: %d", &mode) == 1) {
				// 成功解析	
			} else {
				return -1;
			}
		}
	}

	pclose(fp);
	return mode;
}

int slb_get_pps_switch(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_pps_switch", dev_name);

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return -1;
	}

	char line[128] = {0};
	int pps_enable = -1;
	if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "pps_switch =");
		if (pos) {
			if (sscanf(pos, "pps_switch = %d", &pps_enable) == 1) {
				// 成功解析
			} else {
				return -1;
			}
		}
	}

	pclose(fp);
	return pps_enable;
}

int slb_get_timestamp_cnt(char *dev_name, slb_timestamp_t *timestamp)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_slb_timestamp", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {        
        pclose(fp);
		return -1;
    }
	pclose(fp);

	char *pos = strstr(line, "[SUCC]");
	if (!pos) {
		return -1;
	}

	// 跳过 [SUCC]开始解析
	pos += 6;
	int slb_cnt, glb_cnt;
	int matched = sscanf(pos,
						"slb_cnt = %d, glb_cnt = %d",
						&slb_cnt, &glb_cnt);

	if (matched != 2) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: parse slb timestamp failed");
		return -1;
	}

	// 填充结构体
	timestamp->slb_cnt = slb_cnt;
	timestamp->glb_cnt = glb_cnt;

	return 0;
}

int slb_get_wds_mode(char *dev_name, slb_wds_mode_t *wds_mode)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_wds_enable", dev_name);

	FILE* fp = popen(cmd, "r");
	if(fp == NULL)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[1024] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {        
        pclose(fp);
		return -1;
    }
	pclose(fp);

	char *pos = strstr(line, "[SUCC]");
	if (!pos) {
		return -1;
	}

	// 跳过 [SUCC]开始解析
	pos += 6;
	int wdsenable,wdsmode;
	int matched = sscanf(pos,
						"wds_enable=%d, wds_mode=%d",
						&wdsenable, &wdsmode);

	if (matched != 2) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: parse wds mode failed");
		return -1;
	}

	// 填充结构体
	wds_mode->wds_enable = wdsenable;
	wds_mode->wds_mode = wdsmode;

	return 0;
}

int slb_get_view_mcs_info(char *dev_name, mcs_data_t *mcs_data)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg view_mcs", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char buffer[1024] = {0};
	char line[256];
	size_t total_len = 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		size_t line_len = strlen(line);
		if (total_len + line_len < sizeof(buffer) - 1) {
			strcat(buffer, line);
			total_len += line_len;
		}
	}
	pclose(fp);

	memset(mcs_data, 0, sizeof(mcs_data_t));

	char *pos = strstr(buffer, "[SUCC]");
	if (!pos) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: command execution failed, no [SUCC] found");
		return -1;
	}

	pos +=6;

	char *line_start = pos;
	char *next_line = line_start;
	int user_count = 0;

	while ((next_line = strchr(line_start, '\n')) != NULL) {
		// 临时终止当前行
		*next_line = '\0';

		// 检查是否是user行
		if (strncmp(line_start, "user=", 5) == 0) {
			int user_idx, ul_mcs, dl_mcs;
			int matched = sscanf(line_start, "user=%d:ul_mcs=%d, dl_mcs=%d",
								&user_idx, &ul_mcs, &dl_mcs);
			if (matched == 3 && user_count < 256) {
				mcs_data->user[user_count].user_idx = user_idx;
				mcs_data->user[user_count].ul_mcs = ul_mcs;
				mcs_data->user[user_count].dl_mcs = dl_mcs;
				user_count++;
			}
		}

		// 恢复换行符并移动到下一行
		*next_line = '\n';
		line_start = next_line + 1;

		// 跳过空行
		while (*line_start == '\n' || *line_start == '\r') {
			line_start++;
		}

		if (*line_start == '\0') break;
	}

	mcs_data->count = user_count;

	if (user_count == 0) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: No user MCS data found");
		return -1;
	}

	return 0;
}

int slb_get_rssi_info(char *dev_name, rssi_data_t *rssi_data)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_rssi", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char buffer[1024] = {0};
	char line[256];
	size_t total_len = 0;

	// 读取所有输出
	while (fgets(line, sizeof(line), fp) != NULL) {
		size_t line_len = strlen(line);
		if (total_len + line_len < sizeof(buffer) - 1) {
			strcat(buffer, line);
			total_len += line_len;
		}
	}
	pclose(fp);

	memset(rssi_data, 0, sizeof(rssi_data_t));

	char *pos = strstr(buffer, "[SUCC]");
	if (!pos) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: command execution failed, no [SUCC] found");
		return -1;
	}

	// 跳过[SUCC]
	pos += 6;

	// 跳过表头行（user_idx rssi rsrp snr）
	char *header_end = strchr(pos, '\n');
	if (header_end) {
		pos = header_end + 1;
	}

	//解析每一行的数据
	char *line_start = pos;
	char *next_line = line_start;
	int user_count = 0;

	while ((next_line = strchr(line_start, '\n')) != NULL) {
		// 临时终止当前行
		*next_line = '\0';

		// 跳过空行或只有空白字符的行
		char *trim_start = line_start;
		while (*trim_start == ' ' || *trim_start == '\t') {
			trim_start++;
		}
		if (*trim_start == '\0') {
			*next_line = '\n';
			line_start = next_line + 1;
			continue;
		}

		// 解析数据行
		int user_idx, rssi, rsrp, snr;
		int matched = sscanf(line_start, "%d %d %d %d", &user_idx, &rssi, &rsrp, &snr);

		if (matched == 4 && user_count < 256) {
			rssi_data->user[user_count].user_idx = user_idx;
			rssi_data->user[user_count].rssi = rssi;
			rssi_data->user[user_count].rsrp = rsrp;
			rssi_data->user[user_count].snr = snr;
			user_count++;
		}

		// 恢复换行符并移动到下一行
		*next_line = '\n';
		line_start = next_line + 1;
	}

	rssi_data->count = user_count;

	if (user_count == 0) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: No user MCS data found");
		return -1;
	}
	return 0;
}

const char* slb_fem_check(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return NULL;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg fem_check", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return NULL;
	}

	char line[1024] = {0};
	if (fgets(line, sizeof(line), fp) == NULL) {
		pclose(fp);
		return NULL;
	}
	pclose(fp);

	// 查找[SUCC]标记
	char *pos = strstr(line, "[SUCC]");
	if (!pos) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: command execution failed, no [SUCC] found");
		return NULL;
	}

	// 检查是否有"device process success"
	if (strstr(line, "device process success") != NULL) {
		return "normal";
	} else {
		return "abnormal";
	}

	return NULL;
}

int slb_get_chip_temperature(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s alg read_temperature", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[128] = {0};
	unsigned int value = 0;
	if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "[SUCC]0x");
		if (pos) {
			int v;
			if (sscanf(pos, "[SUCC]0x%8x", &v) == 1) {
				value = (unsigned int)v;
			}
		}
	}
	pclose(fp);
	return value;
}

int slb_set_range_opt(char *dev_name, int range_opt_enable)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return -1;
		}
	}

	if(range_opt_enable < 0 || range_opt_enable > 1)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid range_opt_enable. Expected value [0,1]");
		return -1;
	}

	//确保关闭网口
	slb_ifconfig(dev_name, 0);


	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"range_opt %d\"", dev_name, range_opt_enable);

	//执行系统命令
	int ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to set cell id");
		return -1;
	}

	return 0;
}

int slb_get_range_opt(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return -1;
	}	

    //构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg get_range_opt", dev_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid command %s", cmd);
		return -1;
	}

	char line[128] = {0};
	unsigned int value = 0;
	if (fgets(line, sizeof(line), fp) != NULL) {
		char *pos = strstr(line, "[SUCC]en");
		if (pos) {
			int v;
			if (sscanf(pos, "[SUCC]en = %d", &v) == 1) {
				value = (unsigned int)v;
			}
		}
	}
	pclose(fp);
	return value;
}

static int load_kernel_module(char *module_path)
{
	char cmd[MAX_CMD_LEN];
	snprintf(cmd, sizeof(cmd),"insmod %s", module_path);

	return system(cmd);
}


void generate_random_mac(char* mac_buffer)
{
	//设置随机种子
	srand(time(NULL));

	//生成6个随机字节
	for(int i = 0 ; i < 6; i++)
	{
		unsigned char byte = rand() % 256;

		//格式化到缓冲区，使用十六进制表示
		if(i == 0)
		{
			//确保第一个字节的第二个位为0（非组播地址）
			byte &= 0xFE; //确保最低位为0（单播地址）
			byte |= 0x02; //设置第二位为1（本地管理地址）
		}

		//将字节格式化为两位十六进制字符串
		sprintf(mac_buffer + i * 3, "%02X:", byte);
	}
	
	//去掉最后一个冒号
	mac_buffer[17] = '\0';

}


void param_default(void)
{
	//节点参数	
	WTSLNodeBasicInfo *node = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	char* mac_address = (char*)malloc(MAC_ADDR_LEN * sizeof(char));
	if(mac_address == NULL) {
		WTSL_LOG_ERROR(MODULE_NAME, "memory allocation failed");
	}
	generate_random_mac(mac_address);
	WTSL_LOG_INFO(MODULE_NAME, "Randomly Generated MAC Address: %s", mac_address);

	node->bw = 20;
	node->tfc_bw = 20;
	node->channel = 2479;
	strcpy(node->mac, mac_address);
	node->type = NODE_TYPE_SLB_T;	
	global_node_info.node_type = SP_TNODE;
	strcpy(node->ip, "192.168.99.1");
	
	strcpy(adv_info->mcs_bound_table[0].user_mac, "00:00:00:00:00:00");
	adv_info->mcs_bound_table[0].min_mcs = 6;
	adv_info->mcs_bound_table[0].max_mcs = 30;
	strcpy(node->name, "tnode_000");

	strcpy(adv_info->password, "12345678");
	adv_info->power = 250;
	node->log_port = 6025;
	strcpy(node->net_manage_ip, "192.168.1.1");
	node->auto_join_net_flag = 0;

	strcpy(node->bridge_interfaces[0], "wt_vap0");
	strcpy(node->bridge_interfaces[1], "usb0");
	strcpy(node->bridge_interfaces[2], "eth0");
	node->bridge_interface_num = 3;
	node->dhcp_enable = 1;
	node->aifh_enable = 1;
	node->sle_type = NODE_TYPE_SLE_T;
	strcpy(node->sle_name, "sle_t001");

	// 高级信息
	// 默认单接口
	adv_info->devid = 0;
	adv_info->cell_id = 9;
	adv_info->cc_offset = 0;
	adv_info->mib_params.cp_type = 0;
	adv_info->mib_params.symbol_type = 2;
	adv_info->mib_params.sysmsg_period = 1;
	adv_info->mib_params.s_cfg_idx = 2;
	adv_info->sec_exch_cap = 0x00000021;
	

	//其他参数

}

int set_node() 
{
	int ret = -1;
	WTSLNodeBasicInfo* node_params = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	//关闭网口才能注销设备
/*	slb_ifconfig(node_params.dev_name, 0);
	
	if(slb_destory_device(node_params.dev_name) == -1)
		return -1;
		
	if(slb_create_device(node_params.dev_name, node_params.slb_role) == -1)
		return -1;
	sleep(1);
*/
	//if(set_mac_address("vap0", node_params->mac) == -1)
		//return -1;
	slb_ifconfig(NET_VAP_NAME, 0);
	
	slb_set_channel(NET_VAP_NAME, node_params->channel);

	int bw = slb_get_bw(NET_VAP_NAME);
	if(node_params->bw >= bw)
	{
		slb_set_bw(NET_VAP_NAME, node_params->bw);
		slb_set_tfc_bw(NET_VAP_NAME, node_params->tfc_bw);
	}
	else
	{
		slb_set_tfc_bw(NET_VAP_NAME, node_params->tfc_bw);
		slb_set_bw(NET_VAP_NAME, node_params->bw);
	}
	
	for(int i = 0; strlen(adv_info->mcs_bound_table[i].user_mac) != MAC_ADDR_LEN -1; i++)
	{	

		slb_set_msc_bound(NET_VAP_NAME, adv_info->mcs_bound_table[i]);
	}
	
	slb_set_cc_start_pos(NET_VAP_NAME, adv_info->cc_offset);
	
	slb_set_cell_id(NET_VAP_NAME, adv_info->cell_id);
	
	slb_set_essid(NET_VAP_NAME, node_params->name);
	
	slb_set_mib_params(NET_VAP_NAME, adv_info->mib_params);

	slb_set_sec_auth_pwd(NET_VAP_NAME, adv_info->password);
	
	if (node_params->type == 1) {
		slb_set_sec_exch_cap(NET_VAP_NAME, adv_info->sec_exch_cap);
	}

	slb_set_sec_exch_cap(NET_VAP_NAME, adv_info->sec_exch_cap);

	slb_set_range_opt(NET_VAP_NAME, adv_info->range_opt);

	if (node_params->type == NODE_TYPE_SLB_G) {
		slb_set_acs_enable(NET_VAP_NAME, node_params->aifh_enable);
	}
	
	init_net_bridge(NET_BRIDGE_NAME, node_params->ip);

	slb_ifconfig(NET_VAP_NAME, 1);

	sync_ip_to_js_cfg(js_cfg_path, node_params);

	adv_info->devid = get_wt_vap_interface_count();

	slb_set_tx_power(NET_VAP_NAME, adv_info->power);

	int type;
	config_result_t result = config_get_int("TYPE", &type);
	if(result == CONFIG_ERROR_NOT_FOUND)
		type = NODE_TYPE_SLB_T;	
	WTSL_LOG_INFO(MODULE_NAME, "type: %d, node_params->type: %d", type, node_params->type);	
	if(type != node_params->type)
	{
		WTSL_LOG_INFO(MODULE_NAME, "G/T online switching is not supported, and the device will restart after setting");
		ret = system("reboot");
		if(ret != 0){
			WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
		}
	}
	
	return 0;
}

void sync_advinfo(void)
{
	WTSLNodeBasicInfo* node_params = &global_node_info.node_info.basic_info;
	WTSLNodeAdvInfo* adv_info = (WTSLNodeAdvInfo*)(uintptr_t)global_node_info.node_info.basic_info.adv_info;
	adv_info->cc_offset = slb_get_cc_start_pos(NET_VAP_NAME);
	adv_info->cell_id = slb_get_cell_id(NET_VAP_NAME);
	adv_info->devid = get_wt_vap_interface_count();
	adv_info->chip_temperature = slb_get_chip_temperature(NET_VAP_NAME);
	adv_info->range_opt = slb_get_range_opt(NET_VAP_NAME);
	node_params->aifh_enable = slb_get_acs_enable(NET_VAP_NAME);

	mib_params_t params = {0};
	int ret = slb_get_mib_params(NET_VAP_NAME, &params);
	if (ret == 0) {
		adv_info->mib_params.cp_type = params.cp_type;
		adv_info->mib_params.symbol_type = params.symbol_type;
		adv_info->mib_params.sysmsg_period = params.sysmsg_period;
		adv_info->mib_params.s_cfg_idx = params.s_cfg_idx;
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo mib params failed",__FUNCTION__,__LINE__);
	}

	adv_info->sec_exch_cap = slb_get_sec_exch_cap(NET_VAP_NAME);
	adv_info->sec_sec_cap = slb_get_sec_sec_cap(NET_VAP_NAME);
	if (node_params->type == NODE_TYPE_SLB_G) {
		adv_info->lce_mode = slb_get_lce_mode(NET_VAP_NAME);
	}

	adv_info->pps_enable = slb_get_pps_switch(NET_VAP_NAME);

	slb_timestamp_t timestamp = {0};
	ret = slb_get_timestamp_cnt(NET_VAP_NAME, &timestamp);
	if (ret == 0) {
		adv_info->timestamp.slb_cnt = timestamp.slb_cnt;
		adv_info->timestamp.glb_cnt = timestamp.glb_cnt;
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo slb timestamp failed",__FUNCTION__,__LINE__);
	}

	slb_wds_mode_t wds_mode = {0};
	ret = slb_get_wds_mode(NET_VAP_NAME, &wds_mode);
	if (ret == 0) {
		adv_info->wds_mode.wds_enable = wds_mode.wds_enable;
		adv_info->wds_mode.wds_mode = wds_mode.wds_mode;
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo slb wds failed",__FUNCTION__,__LINE__);
	}

	mcs_data_t user_mcs = {0};
	ret = slb_get_view_mcs_info(NET_VAP_NAME, &user_mcs);
	if (ret == 0) {
		adv_info->user_mcs.count = user_mcs.count;
		for (int i = 0; i < user_mcs.count; i++)
		{
			adv_info->user_mcs.user[i].user_idx = user_mcs.user[i].user_idx;
			adv_info->user_mcs.user[i].ul_mcs = user_mcs.user[i].ul_mcs;
			adv_info->user_mcs.user[i].dl_mcs = user_mcs.user[i].dl_mcs;
		}
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo user view_mcs failed",__FUNCTION__,__LINE__);
	}

	real_power_t real_power = {0};
	ret = slb_get_real_power(NET_VAP_NAME, &real_power);
	if (ret == 0) {
		adv_info->real_power.real_pow = real_power.real_pow;
		adv_info->real_power.exp_pow = real_power.exp_pow;
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo real power failed",__FUNCTION__,__LINE__);
	}

	rssi_data_t rssi_data = {0};
	ret = slb_get_rssi_info(NET_VAP_NAME, &rssi_data);
	if (ret == 0) {
		adv_info->user_rssi.count = rssi_data.count;
		for (int i = 0;i < rssi_data.count; i++) 
		{
			adv_info->user_rssi.user[i].user_idx = rssi_data.user[i].user_idx;
			adv_info->user_rssi.user[i].rssi = rssi_data.user[i].rssi;
			adv_info->user_rssi.user[i].rsrp = rssi_data.user[i].rsrp;
			adv_info->user_rssi.user[i].snr = rssi_data.user[i].snr;
		}
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo user rssi_info failed",__FUNCTION__,__LINE__);
	}

	const char *fem_status = slb_fem_check(NET_VAP_NAME);
	if (fem_status != NULL) {
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] sync advinfo fem check status: %s",__FUNCTION__,__LINE__,fem_status);
		strncpy(adv_info->fem_check, fem_status, sizeof(adv_info->fem_check) - 1);
		adv_info->fem_check[sizeof(adv_info->fem_check) - 1] = '\0';
	} else {
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] sync advinfo fem check failed",__FUNCTION__,__LINE__);
	}
}


char* slb_node_self_test(void)
{
/*
	node_params_t* param= get_node_params();
	cJSON *response = cJSON_CreateObject();
	
	//查询版本
	char host_driver_version[8] = {0};
	char ver_infor[128]={0};
	parse_host_version(host_driver_version);
	snprintf(ver_infor, sizeof(ver_infor), "v%d.%d.%d_%d.%s", HARDWARE_VERSION, SYSTEM_VERSION, VersionCode,WTCORE_VERSION, host_driver_version);
	cJSON_AddItemToObject(response, "version", cJSON_CreateString(ver_infor));
	
	//查询slb_role
	cJSON_AddItemToObject(response, "slb_role", cJSON_CreateString(param->slb_role));

	//查询device_name
	cJSON_AddItemToObject(response, "device_name", cJSON_CreateString(param->dev_name));

	//查询channel
	char str[20];
	snprintf(str, sizeof(str), "%d", param->channel);
	cJSON_AddItemToObject(response, "channel", cJSON_CreateString(str));

	//查询bw
	snprintf(str, sizeof(str), "%d", param->bw);
	cJSON_AddItemToObject(response, "bw", cJSON_CreateString(str));

	//查询tfc_bw
	snprintf(str, sizeof(str), "%d", param->tfc_bw);
	cJSON_AddItemToObject(response, "tfc_bw", cJSON_CreateString(str));

	//查询tx_power
	snprintf(str, sizeof(str), "%d", param->power);
	cJSON_AddItemToObject(response, "tx_power", cJSON_CreateString(str));

    //查询cell_id
	snprintf(str, sizeof(str), "%d", param->ceil_id);
	cJSON_AddItemToObject(response, "cell_id", cJSON_CreateString(str));

	//查询essid
	cJSON_AddItemToObject(response, "essid", cJSON_CreateString(param->essid_str));

	//查询password
	cJSON_AddItemToObject(response, "password", cJSON_CreateString(param->password));

	//查询设备ip
	cJSON_AddItemToObject(response, "ipaddr", cJSON_CreateString(param->ip_addr));

	//查询msc_bound
	cJSON* mcsbound = cJSON_AddArrayToObject(response, "mcsbound");
	for(int i = 0; strlen(param->mcs_bound_table[i].user_mac) == MAC_ADDR_LEN -1; i++)
	{
		cJSON* item = cJSON_CreateObject(); 
		cJSON_AddItemToObject(item, "user_mac", cJSON_CreateString(param->mcs_bound_table[i].user_mac));
		cJSON_AddItemToObject(item, "min_mcs", cJSON_CreateNumber(param->mcs_bound_table[i].min_mcs));
		cJSON_AddItemToObject(item, "max_mcs", cJSON_CreateNumber(param->mcs_bound_table[i].max_mcs));
		cJSON_AddItemToArray(mcsbound, item);
	}


	//查询mib_params
	cJSON* mib_params = cJSON_AddArrayToObject(response, "mib_params");
	cJSON* item = cJSON_CreateObject(); 
	cJSON_AddItemToObject(item, "cp_type", cJSON_CreateNumber(param->mib_params.cp_type));
	cJSON_AddItemToObject(item, "symbol_type", cJSON_CreateNumber(param->mib_params.symbol_type));
	cJSON_AddItemToObject(item, "sysmsg_period", cJSON_CreateNumber(param->mib_params.sysmsg_period));
	cJSON_AddItemToObject(item, "s_cfg_idx", cJSON_CreateNumber(param->mib_params.s_cfg_idx));
	cJSON_AddItemToArray(mib_params, item);

	char *res = cJSON_Print(response);
	char* jsonstr = malloc(strlen(res)+1);
	strcpy(jsonstr,res);
	cJSON_Delete(response);
	return jsonstr;
	*/
	return NULL;
}

char* slb_node_view_users(char *dev_name)
{
	//参数验证
	if(dev_name == NULL || strlen(dev_name) == 0 || strlen(dev_name) > MAX_DEVICE_NAME_LEN)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid device name");
		return NULL;
	}
			
	//检查是否包含可见字符
	for(char* p = dev_name; *p != '\0'; p++)
	{
		if(*p < 0x20 || *p > 0x7E) //非可见的ASCII字符
		{
			WTSL_LOG_ERROR(MODULE_NAME, "Error: Invalid dev_name. Invisible ASCII characters");
			return NULL;
		}
	}

	//构建命令
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "iwpriv %s cfg \"view_users\"", dev_name);

	//执行系统命令并返回输出的字符串
	return system_output_to_string(cmd);


}

char* slb_node_get_hw_resources_info(void)
{
	char *json = malloc(BUFFER_CPU_USAGEINFO_SIZE);
	if (!json) return NULL;

	time_t now = time(NULL);

	// CPU 信息
	double overall_usage = 0.0;
	double core_usage[4] = {0.0, 0.0, 0.0, 0.0};
	double load1 = 0.0;
	double load5 = 0.0;
	double load15 = 0.0;

	// 内存信息
	unsigned long mem_total = 0;
	unsigned long mem_used = 0;
	unsigned long mem_free = 0;
	double mem_usage_percent = 0.0;

	// 磁盘信息
	unsigned long disk_total = 0;
	unsigned long disk_used = 0;
	unsigned long disk_free = 0;
	double disk_usage_percent = 0.0;

	// 获取CPU使用率
	int cpu_result = get_cpu_usage_stats(&overall_usage, core_usage);
	// 获取系统负载
	get_load_average(&load1, &load5, &load15);
	
	get_memory_usage(&mem_total, &mem_used, &mem_free, &mem_usage_percent);

	get_disk_usage(&disk_total, &disk_used, &disk_free, &disk_usage_percent);

	// 如果mpstat失败，使用负载平均值作为近似值
	if (cpu_result != 0) {
		overall_usage = load1 * 25.0; // 启发式转换
		if (overall_usage > 100.0) overall_usage = 100.0;

		for (int i = 0; i < 4; i++) {
			core_usage[i] = overall_usage * (0.9 + (rand() % 20) / 100.0);
			if (core_usage[i] > 100.0) core_usage[i] = 100.0;
		}
	}

	// 格式化内存和磁盘大小
	char mem_total_str[32] = {0};
	char mem_used_str[32] = {0};
	char mem_free_str[32] = {0};

	char disk_total_str[32] = {0};
	char disk_used_str[32] = {0};
	char disk_free_str[32] = {0};

	format_bytes(mem_total, mem_total_str, sizeof(mem_total_str));
	format_bytes(mem_used, mem_used_str, sizeof(mem_used_str));
	format_bytes(mem_free, mem_free_str, sizeof(mem_free_str));

	format_bytes(disk_total, disk_total_str, sizeof(disk_total_str));
	format_bytes(disk_used, disk_used_str, sizeof(disk_used_str));
	format_bytes(disk_free, disk_free_str, sizeof(disk_free_str));

	snprintf(json, BUFFER_CPU_USAGEINFO_SIZE,
			"{"
			"\"data\": {"
				"\"timestamp\": %lld,"
				"\"cpu\": {"
					"\"overall\": {\"usage_percent\": %.2f},"
					"\"loadavg\": {"
						"\"load1\": %.2f,"
						"\"load5\": %.2f,"
						"\"load15\": %.2f"
					"},"
					"\"cores\": ["
						"{\"core\": 0, \"usage_percent\": %.2f},"
						"{\"core\": 1, \"usage_percent\": %.2f},"
						"{\"core\": 2, \"usage_percent\": %.2f},"
						"{\"core\": 3, \"usage_percent\": %.2f}"
					"]"
				"},"
				"\"memory\": {"
					"\"total\": %lu,"
					"\"used\": %lu,"
					"\"free\": %lu,"
					"\"usage_percent\": %.2f,"
					"\"total_str\": \"%s\","
					"\"used_str\": \"%s\","
					"\"free_str\": \"%s\""
				"},"
				"\"disk\": {"
					"\"total\": %lu,"
					"\"used\": %lu,"
					"\"free\": %lu,"
					"\"usage_percent\": %.2f,"
					"\"total_str\": \"%s\","
					"\"used_str\": \"%s\","
					"\"free_str\": \"%s\""
				"}"
			"}"
			"}",
            (long long)now,
			// cpu数据
			overall_usage,
			load1, load5, load15,
			core_usage[0], core_usage[1], core_usage[2], core_usage[3],
			// memory数据
			mem_total, mem_used, mem_free, mem_usage_percent,
			mem_total_str, mem_used_str, mem_free_str,
			// disk数据
			disk_total, disk_used, disk_free, disk_usage_percent,
			disk_total_str, disk_used_str, disk_free_str
	);

    return json;
}

int init_net_bridge(char* net_bridge_name, char* ip)
{
	int ret = -1;
	WTSLNodeBasicInfo* node = &global_node_info.node_info.basic_info;

	char cmd[128];
	snprintf(cmd, sizeof(cmd), "brctl addbr %s", net_bridge_name);
	ret |= system(cmd);
	
	for(int i = 0; i < node->bridge_interface_num; i++)
	{
		snprintf(cmd, sizeof(cmd), "ifconfig %s 0.0.0.0", node->bridge_interfaces[i]);
		ret |= system(cmd);

		snprintf(cmd, sizeof(cmd), "brctl addif %s %s", net_bridge_name, node->bridge_interfaces[i]);
		ret |= system(cmd);

	}

	snprintf(cmd, sizeof(cmd), "ip link set dev %s address $(cat /sys/class/net/%s/address)", net_bridge_name, NET_VAP_NAME);
	ret |= system(cmd);

	snprintf(cmd, sizeof(cmd), "ifconfig %s %s", net_bridge_name, ip);
	ret = system(cmd);
	if(ret != 0)
	{
		WTSL_LOG_ERROR(MODULE_NAME, "Error: Failed to ifconfig %s %s", net_bridge_name, ip);
		// return -1;
	}

	return 0;
}