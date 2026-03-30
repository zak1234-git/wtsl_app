#ifndef SLB_INTERFACE_H
#define SLB_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "wtsl_core_node_manager.h"
#include "../../api/version.h"
#define MAX_DEVICE_NAME_LEN 16
#define FIXED_PSK_LEN 64
#define MAX_ESSID_LEN 128
#define GT0_CMD_FORMAT	"echo \"GT0 create %s %s\" > %s"
#define DESTROY_CMD_FORMAT  "echo \"%s destroy\" > %s"
#define GT0_FILE_PATH "/sys/hisys/hipriv_gt"
#define MAX_CMD_LEN 256
#define MAC_ADDR_LEN 18
#define MIN_PASSWORD_LEN 8
#define MAX_PASSWORD_LEN 32
#define SLB_ROLE_LEN 6
#define MAX_IP_LEN 32
#define MAX_USER_NUMBER 32
#define NET_BRIDGE_NAME "wt_br0"
#define NET_VAP_NAME "wt_vap0"
#define HARDWARE_VERSION 1
#define SYSTEM_VERSION 1
#define APP_VERSION 1
#define ENV_CONFIG "/home/wt/env_config"
#define MAX_LINE_LENGTH 1024
//#define ESSID_ASCII

#define BUFFER_CPU_USAGEINFO_SIZE 1024

void param_default(void);
int init_node(void);
int  t_node_join_net(char* dev_name, char *gnode_mac_addr, int power);
int slb_create_device(char* name, int type);
int slb_destory_device(char* name);
int set_mac_address(char *dev_name, char *mac_addr);
int get_mac_address(char *dev_name, char *mac_addr);
int set_ip_address(char *dev_name, char *ip_addr);
int get_ip_address( char *dev_name, char *ip_addr, size_t buf_size);
int slb_ifconfig( char *dev_name, bool up);
int slb_set_channel(char *dev_name, int channel);
int slb_get_channel(char *dev_name);
int slb_set_bw(char *dev_name, int bw);
int slb_get_bw(char *dev_name);
int slb_set_tfc_bw(char *dev_name, int tfc_bw);
int slb_get_tfc_bw(char *dev_name);
int slb_set_cc_start_pos(char *dev_name, int cc_offset);
int slb_get_cc_start_pos(char *dev_name);
int slb_set_cell_id(char *dev_name, int id);
int slb_get_cell_id(char *dev_name);
int slb_set_essid(char *dev_name, char *essid_str);
int slb_set_acs_enable(char *dev_name, int enable);
int slb_get_acs_enable(char *dev_name);

int slb_set_sec_auth_pwd(char *dev_name, char *password);
int slb_t_node_scan(char *dev_name);
char* slb_t_node_show_bss(char *dev_name);
int slb_t_node_join_bssid(char *dev_name, char *gnode_mac_addr);
int slb_t_node_start_join(char *dev_name, int bss_idx);
int slb_view_user(char *dev_name);
int slb_set_tx_power(char *dev_name, int power);
int set_node(void);
char* slb_node_self_test(void);
char* slb_node_view_users(char *dev_name);
char* slb_node_get_hw_resources_info(void);
int init_net_bridge(char* net_bridge_name, char* ip);
int get_wt_vap_interface_count(void);
void sync_advinfo(void);

int slb_get_real_power(char *dev_name, real_power_t *real_power);
int slb_set_sec_auth_psk(char *dev_name, char *auth_psk);
int slb_set_sec_exch_cap(char *dev_name, int exch_cap);
int slb_get_sec_exch_cap(char *dev_name);
int slb_get_sec_sec_cap(char *dev_name);
int slb_get_lce_mode(char *dev_name);
int slb_get_pps_switch(char *dev_name);
int slb_get_timestamp_cnt(char *dev_name, slb_timestamp_t *timestamp);
int slb_get_wds_mode(char *dev_name, slb_wds_mode_t *wds_mode);
int slb_get_view_mcs_info(char *dev_name, mcs_data_t *mcs_data);
int slb_get_rssi_info(char *dev_name, rssi_data_t *rssi_data);
const char* slb_fem_check(char *dev_name);
int slb_get_chip_temperature(char *dev_name);
int slb_set_range_opt(char *dev_name, int range_opt_enable);
int slb_get_range_opt(char *dev_name);
int slb_set_msc_bound(char *dev_name, mcs_bound_t mcs_bound);
int slb_set_mib_params(char *dev_name, mib_params_t mib_params);

#endif