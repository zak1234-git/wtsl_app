/**
 * Copyright (c) @CompanyNameMagicTag 2022. All rights reserved.
 *
 * Description: SLE private service register sample of client.
 */
#ifndef SLE_UART_CLIENT_H
#define SLE_UART_CLIENT_H
#include "sle_connection_manager.h"
typedef struct {
    uint16_t conn_id;
    sle_acb_state_t state;
} server_info_t;

typedef struct {
	server_info_t server_info;
	char mac[18];
} connect_manage_t;

typedef enum {
    SEND_STATE_IDLE = 0,
    SEND_STATE_SENDING,
    SEND_STATE_EXIT,
} sle_send_data_state_t;

typedef enum {
    IDLE,
    BUSY,
} sle_task_state_t;

void sle_client_init(void);
void sle_server_init(void);
void sle_client_deinit(void);
void sle_conn_set_max_server_cnt(int cnt);
#endif