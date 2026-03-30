/**
 * Copyright (c) @CompanyNameMagicTag 2022. All rights reserved.
 *
 * Description: SLE private service register sample of client.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "sle_transmition_manager.h"
#include "sle_device_discovery.h"
#include "sle_device_manager.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_errcode.h"
#include "sle_common.h"
#include "wtsl_core_node_manager.h"
#include "wtsl_core_api.h"
#include "sle_conn_client.h"


#define SLE_SEEK_INTERVAL_DEFAULT        160  // 160
#define SLE_SEEK_WINDOW_DEFAULT          40  // 40

#define SLE_CONN_SERVER_NAME            "sle_server"

#define sample_at_log_print(fmt, args...) printf("--++--" fmt"\r\n", ##args)


static uint16_t g_connect_id = 0;
static sle_addr_t g_addr = {0};
static sle_send_data_state_t g_send_flag = SEND_STATE_IDLE;
static pthread_t g_send_thread = 0;
static uint8_t g_max_server_cnt = 5; // 默认5个server
sle_seek_result_info_t seek_result_info[SLE_MAX_CONN] = {0};
uint8_t scan_num = 0;
extern int connected_num;
connect_manage_t connect_manage[SLE_MAX_CONN] = {0};
extern SPLINK_INFO global_node_info;
extern int announce_id[20];
extern int announce_id_num;

server_info_t g_server_info[SLE_MAX_CONN] = { 0 };

typedef struct {
    uint8_t server_cnt;
    sle_task_state_t state;
} sle_task_context_t;

sle_task_context_t g_sle_context;

ssapc_write_param_t g_sle_send_param = { 0 };

void sle_conn_set_max_server_cnt(int cnt)
{
    g_max_server_cnt = cnt;
}

static server_info_t *sle_get_server_by_addr(const sle_addr_t *addr)
{
    if (addr->addr[0] != 0xD) {
        return NULL;
    }

    if (addr->addr[0x5] < 1 || addr->addr[0x5] > SLE_MAX_CONN) {
        return NULL;
    }

    return &g_server_info[addr->addr[0x5] - 1];
}

void sle_start_scan(void)
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = SLE_SEEK_FILTER_ALLOW_ALL;
    param.seek_phys = SLE_SEEK_PHY_1M;
    param.seek_type[0] = SLE_SEEK_ACTIVE;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);

    sle_start_seek();
    return;
}

static void sle_sample_sle_power_on_cbk(uint8_t status)
{
    sample_at_log_print("sle power on: %d.\r\n", status);
    enable_sle();
}

static void sle_sample_sle_enable_cbk(uint8_t status)
{
    if (status != ERRCODE_SLE_SUCCESS) {
        sample_at_log_print("sle sample sle enable failed, status: %02x", status);
        return;
    }

    sample_at_log_print("sle sample sle enable success.\r\n");
    //sle_start_scan();
}

static void sle_sample_dev_cbk_register(void)
{
    sle_dev_manager_callbacks_t sle_dev_mgr_cbk = { 0 };
    sle_dev_mgr_cbk.sle_power_on_cb = sle_sample_sle_power_on_cbk;
    sle_dev_mgr_cbk.sle_enable_cb = sle_sample_sle_enable_cbk;
    sle_dev_manager_register_callbacks(&sle_dev_mgr_cbk);
}

static void sle_sample_seek_enable_cbk(errcode_t status)
{
    sample_at_log_print("sle sample seek enable status: %02x.\r\n", status);
}

static void sle_sample_seek_disable_cbk(errcode_t status)
{
    sample_at_log_print("sle sample seek disable status: %02x.\r\n", status);
    sample_at_log_print("[ssap client] connect remote [%02x:%02x:%02x:%02x:%02x:%02x]",
        g_addr.addr[0], g_addr.addr[1], g_addr.addr[2], g_addr.addr[3], g_addr.addr[4], g_addr.addr[5]);
}

void sle_create_connection(const sle_addr_t *remote_addr)
{
    sle_default_connect_param_t private_connect_param = {
        .enable_filter_policy = 0, // 过滤功能：0 关闭，1 打开
        .initiate_phys = 1, // 通信带宽：  1：1M，2：2M
        .gt_negotiate = 1, // 是否进行G和T交互： 0：否，1：是
        .scan_interval = 800,
        .scan_window = 800,
        .min_interval = 400,
        .max_interval = 400,
        .timeout = 500 // 500 * 10ms = 5s
    };
    sle_default_connection_param_set(&private_connect_param);
    sle_connect_remote_device(remote_addr);
}

/*
static void sle_create_broadcast(void)
{
	sle_announce_param_t param = {0};
	uint8_t index;
	unsigned char local_addr[SLE_ADDR_LEN] = { 0x00, 0x02, 0x03, 0x04, 0x0A, 0x06 };
	param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
	param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
	param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
	param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
	param.announce_channel_map = 0;
	param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT;
	param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT;
	param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT;
	param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT;
	param.conn_max_latency = SLE_CONN_MAX_LATENCY;
	param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
	param.own_addr.type = 0;
	strncpy(param.own_addr.addr, local_addr, SLE_ADDR_LEN);


	sle_set_announce_param(param.announce_handle, &param);

    sle_start_announce(param.announce_handle);

}*/


static void sle_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{	
	int ret = 1;
    if (seek_result_data != NULL && seek_result_data->data != NULL) {
        //server_info_t *server = sle_get_server_by_addr(&seek_result_data->addr);
         printf("seek_result_data->addr.addr: %x:%x:%x:%x:%x:%x\n", seek_result_data->addr.addr[0], seek_result_data->addr.addr[1], seek_result_data->addr.addr[2],
         seek_result_data->addr.addr[3], seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);
        //if (server != NULL) {
            //if (g_sle_context.state == IDLE) {
                //sample_at_log_print("find device, connecting, mac = %d\r\n", seek_result_data->addr.addr[0x5]);
                //sle_create_connection(&seek_result_data->addr);
               // g_sle_context.state == BUSY;
            //} else {
               // sample_at_log_print("find device, wait for connect, mac = %d\r\n", seek_result_data->addr.addr[0x5]);
           // }
        //}

		if(scan_num == 0)
		{
			memcpy(&seek_result_info[scan_num], seek_result_data, sizeof(sle_seek_result_info_t));
			sample_at_log_print("find device, mac = %x:%x:%x:%x:%x:%x\n", seek_result_data->addr.addr[0], seek_result_data->addr.addr[1], seek_result_data->addr.addr[2],
					seek_result_data->addr.addr[3], seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);
			scan_num++;
		}
		else
		{
			for(int i = 0; i < scan_num; i++)
			{
				ret = ret && (strncmp(seek_result_info[i].addr.addr, seek_result_data->addr.addr, SLE_ADDR_LEN));
			}
			if(ret)
			{
				sample_at_log_print("find device, mac = %x:%x:%x:%x:%x:%x\n", seek_result_data->addr.addr[0], seek_result_data->addr.addr[1], seek_result_data->addr.addr[2],
					seek_result_data->addr.addr[3], seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);				
				memcpy(&seek_result_info[scan_num], seek_result_data, sizeof(sle_seek_result_info_t));
				scan_num++;
			}
		}
    }
}

static void sle_sample_seek_cbk_register(void)
{
    sle_announce_seek_callbacks_t sle_seek_cbk = {0};
    sle_seek_cbk.seek_enable_cb = sle_sample_seek_enable_cbk;
    sle_seek_cbk.seek_disable_cb = sle_sample_seek_disable_cbk;
    sle_seek_cbk.seek_result_cb = sle_sample_seek_result_info_cbk;
    sle_announce_seek_register_callbacks(&sle_seek_cbk);
}

static uint16_t g_conn_id = 0;


static void sle_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
    sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{	
		connect_manage[conn_id].server_info.conn_id = conn_id;
		connect_manage[conn_id].server_info.state = conn_state;
		snprintf(connect_manage[conn_id].mac, sizeof(connect_manage[conn_id].mac), "%02x:%02x:%02x:%02x:%02x:%02x", addr->addr[0], addr->addr[1], addr->addr[2],
			addr->addr[3], addr->addr[4], addr->addr[5]);
        			
		if(conn_state == SLE_ACB_STATE_CONNECTED)
			printf("SLE connected %s\n", connect_manage[conn_id].mac);
		else if(conn_state == SLE_ACB_STATE_DISCONNECTED)
			printf("SLE disconnected %s  disc_reason:0x%x\n", connect_manage[conn_id].mac, disc_reason);
		else if(conn_state == SLE_ACB_STATE_NONE)
			printf("SLE not connect %s\n", connect_manage[conn_id].mac);

}

void sle_sample_connect_cbk_register(void)
{
    sle_connection_callbacks_t sle_connect_cbk = {0};
    sle_connect_cbk.connect_state_changed_cb = sle_sample_connect_state_changed_cbk;
    sle_connection_register_callbacks(&sle_connect_cbk);
}

void fast_data_recv_cb(uint16_t rmt_hdl, uint8_t *data, uint16_t data_len)
{
    sample_at_log_print("conn: %d, data: %d, len = %d", rmt_hdl, *data, data_len);
    sle_tcp_server_send(data,data_len);
}
void sle_client_init(void)
{
	WTSLNodeBasicInfo* basicinfo = &global_node_info.node_info.basic_info;
    sle_sample_dev_cbk_register();
    sle_sample_seek_cbk_register();
    sle_sample_connect_cbk_register();
    enable_sle();
    sle_tm_fast_register_cbks(fast_data_recv_cb);

	sle_addr_t addr;
	sle_set_local_name(basicinfo->sle_name, sizeof(basicinfo->sle_name));
	sscanf(basicinfo->mac, "%x:%x:%x:%x:%x:%x", &addr.addr[0], &addr.addr[1], &addr.addr[2], &addr.addr[3], &addr.addr[4], &addr.addr[5]);
 	sle_set_local_addr(&addr); 

    return;
}

void sle_client_senddata(uint8_t *data, uint16_t data_len){
    if(announce_id_num == 0) //不设置特定通道发送，默认发所有已连接的通道
    {
        for(int i = 0; i < SLE_MAX_CONN; i++)
        {
            if(connect_manage[i].server_info.state == SLE_ACB_STATE_CONNECTED)
            {
                sample_at_log_print("g_conn_id: %d. data: %d, data_len:%d", connect_manage[i].server_info.conn_id, *data, data_len);
                sle_tm_send_fast_data(connect_manage[i].server_info.conn_id, data, data_len);
            }
        }
    }
    else //按特定通道发送
    {
        for(int i = 0; i < announce_id_num; i++)
        {
            sample_at_log_print("g_conn_id: %d. data: %d, data_len:%d", announce_id[i], *data, data_len);
            sle_tm_send_fast_data(announce_id, data, data_len);
        }
    }
    announce_id_num = 0; //发送完初始化为0，默认发所有已连接的通道
}

void sle_client_deinit(void)
{
    sle_stop_seek();
    disable_sle();

    return;
}

/*
void sle_server_init(void)
{
	enable_sle();
    sle_sample_dev_cbk_register();
    sle_create_broadcast();
    
    sle_tm_fast_register_cbks(fast_data_recv_cb);

    return;
}*/
