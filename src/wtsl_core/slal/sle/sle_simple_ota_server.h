/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */

#ifndef SLE_SIMPLE_OTA_SERVER_H
#define SLE_SIMPLE_OTA_SERVER_H

#include <stdint.h>
#include "sle_ssap_server.h"
#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* Service UUID */
#define SLE_UUID_SERVER_SERVICE        0x2222

/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT     0x2323

/* Property Property */
#define SLE_UUID_TEST_PROPERTIES  (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

/* Operation indication */
#define SLE_UUID_TEST_OPERATION_INDICATION \
    (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE | SSAP_OPERATE_INDICATION_BIT_NOTIFY)

/* Descriptor Property */
#define SLE_UUID_TEST_DESCRIPTOR   (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

#define SLE_UART_SEND_DATA_FLOW_DELAY_MS 10

enum simple_ota_messages_opcode {
    SIMPLE_OTA_START,
    SIMPLE_OTA_GOING,
    SIMPLE_OTA_ACK,
    SIMPLE_OTA_END,
};

typedef struct sle_link_state_info {
    uint16_t conn_id;               /* 链路Id */
    uint8_t link_state;             /* 链路状态 */
} sle_link_state_info_t;

extern sle_link_state_info_t g_qos_link_info;
errcode_t sle_uart_server_init(ssaps_read_request_callback ssaps_read_callback, ssaps_write_request_callback
    ssaps_write_callback);

errcode_t sle_uart_server_send_report_by_uuid(const uint8_t *data, uint8_t len);

errcode_t sle_uart_server_send_report_by_handle(const uint8_t *data, uint32_t len);

uint16_t sle_uart_client_is_connected(void);

errcode_t sle_enable_server_cbk(void);

void sle_uart_server_sample_set_mcs(uint16_t conn_id);

uint16_t get_connect_id(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif