/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */
#ifndef SLE_SIMPLE_OTA_CLI_H
#define SLE_SIMPLE_OTA_CLI_H

#include <stdint.h>
#include <stddef.h>

#include "errcode.h"

#define SEND_OTA_FILE_MAX_LEN 200

typedef struct {
    uint32_t opcode;  // enum simple_ota_messages_opcode
    uint8_t data[];
} sle_ota_comm_msg_t;

typedef struct {
    uint32_t file_size;
} sle_ota_file_param_t;

typedef struct {
    uint32_t send_num;
    uint32_t data_len;
    uint8_t data[SEND_OTA_FILE_MAX_LEN];
} sle_ota_send_data_t;

typedef struct {
    uint32_t req_data_num;
    uint32_t req_data_len;
} sle_ota_data_req_t;

errcode_t process_cli_cmd(const char *cmd, const int *args, int arg_num);
errcode_t recv_ota_ack(uint8_t *data, uint16_t data_len);
#endif