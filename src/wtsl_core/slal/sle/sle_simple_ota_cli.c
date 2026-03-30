/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */
#include <stdio.h>
#include <string.h>
#include "utils_tool.h"
#include "sle_errcode.h"
#include "sle_simple_ota_server.h"
#include "sle_simple_ota_fops.h"
#include "sle_simple_ota_cli.h"

typedef errcode_t (*input_cmd_handler)(const int *args, int arg_num);

uint32_t g_ota_total_cnt; // OTA文件总包数
sle_ota_send_data_t g_ota_send_data; // 用于接收fread读取的数据
uint32_t g_ota_send_cnt_record = 0; // 默认发送OTA文件的第一包，用于后续升级包校验及fseek
FILE *g_fp; // 全局fp，方便升级包校验及fseek操作

static errcode_t send_ota_prepare_cmd(const int *args, int arg_num)
{
    sle_ota_file_param_t param;
    uint32_t ret = ERRCODE_SLE_SUCCESS;
    uint32_t file_size = get_ota_file_size();
    g_ota_total_cnt = file_size / SEND_OTA_FILE_MAX_LEN;
    if (file_size % SEND_OTA_FILE_MAX_LEN) {
        g_ota_total_cnt++;
    }
    param.file_size = file_size;
    uint32_t data_len = sizeof(sle_ota_comm_msg_t) + sizeof(sle_ota_file_param_t);
    sle_ota_comm_msg_t *msg = (sle_ota_comm_msg_t *)malloc(data_len);
    if (msg) {
        msg->opcode = SIMPLE_OTA_START;
        memcpy_s(msg->data, sizeof(sle_ota_file_param_t), &param, sizeof(sle_ota_file_param_t));
        ret = sle_uart_server_send_report_by_handle((uint8_t *)msg, data_len);
        free(msg);
    }
    return ret;
}

static errcode_t send_ota_package(const int *args, int arg_num)
{
    g_fp = get_ota_fp(); // 使用二进制模式
    if (g_fp == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    size_t ret = 0;
    ret = fseek(g_fp, 0, SEEK_SET);
    if (ret != 0) {
        SAMPLE_LOG("fseek failed");
        return ERRCODE_SLE_FAIL;
    }
    ret = fread(g_ota_send_data.data, 1, SEND_OTA_FILE_MAX_LEN, g_fp);
    if (ret == 0) {
        SAMPLE_LOG("read ota file failed");
        return ERRCODE_SLE_FAIL;
    }
    g_ota_send_data.send_num = g_ota_send_cnt_record;
    g_ota_send_data.data_len = SEND_OTA_FILE_MAX_LEN;
    uint32_t data_len = sizeof(sle_ota_comm_msg_t) + sizeof(sle_ota_send_data_t);
    sle_ota_comm_msg_t *msg = (sle_ota_comm_msg_t *)malloc(data_len);
    if (msg) {
        msg->opcode = SIMPLE_OTA_GOING;
        memcpy_s(msg->data, sizeof(sle_ota_send_data_t), &g_ota_send_data, sizeof(sle_ota_send_data_t));
        ret = sle_uart_server_send_report_by_handle((uint8_t *)msg, data_len);
        free(msg);
    }
    return ret;
}

static errcode_t send_ota_start_cmd(const int *args, int arg_num)
{
    int32_t ret = fclose(g_fp);
    if (ret != 0) {
        SAMPLE_LOG("fclose failed");
    }
    g_fp = NULL;
    sle_ota_comm_msg_t *msg = (sle_ota_comm_msg_t *)malloc(sizeof(sle_ota_comm_msg_t));
    if (msg) {
        msg->opcode = SIMPLE_OTA_END;
        ret = sle_uart_server_send_report_by_handle((uint8_t *)msg, sizeof(sle_ota_comm_msg_t));
        free(msg);
    }
    return ret;
}

typedef struct {
    char *cli_cmd;
    input_cmd_handler handler;
    int min_arg_num;
} simple_ota_cmd_t;

simple_ota_cmd_t g_cmd_table[] = {
    {"ota_prepare", send_ota_prepare_cmd, 0},
    {"send_ota", send_ota_package, 0},
    {"start_ota", send_ota_start_cmd, 0}
};

errcode_t process_cli_cmd(const char *cmd, const int *args, int arg_num)
{
    for (int i = 0; i < ARRAY_SIZE(g_cmd_table); i++) {
        simple_ota_cmd_t *cmd_entry = &g_cmd_table[i];
        if (strcmp(cmd, cmd_entry->cli_cmd) == 0) {
            if (arg_num < cmd_entry->min_arg_num) {
                SAMPLE_LOG("command %s arg num %d, required %d", cmd_entry->cli_cmd, arg_num, cmd_entry->min_arg_num);
                return ERRCODE_SLE_PARAM_ERR;
            }

            return cmd_entry->handler(args, arg_num);
        }
    }

    SAMPLE_LOG("UNSUPPORT command %s", cmd);
    return ERRCODE_SLE_UNSUPPORTED;
}

errcode_t recv_ota_ack(uint8_t *data, uint16_t data_len)
{
    sle_ota_comm_msg_t *ota_comm_data = (sle_ota_comm_msg_t *)data;
    sle_ota_data_req_t *ota_data_req = (sle_ota_data_req_t *)ota_comm_data->data;
    g_ota_send_cnt_record = ota_data_req->req_data_num;
    if (g_ota_send_cnt_record == g_ota_total_cnt) {
        SAMPLE_LOG("ota file send done");
        return ERRCODE_SLE_SUCCESS;
    }
    size_t ret = 0;
    ret = fseek(g_fp, (g_ota_send_cnt_record * SEND_OTA_FILE_MAX_LEN), SEEK_SET);
    if (ret != 0) {
        SAMPLE_LOG("fseek failed");
        return ERRCODE_SLE_FAIL;
    }
    ret = fread(g_ota_send_data.data, 1, ota_data_req->req_data_len, g_fp);
    if (ret == 0) {
        SAMPLE_LOG("read ota file failed");
        return ERRCODE_SLE_FAIL;
    }
    g_ota_send_data.send_num = g_ota_send_cnt_record;
    g_ota_send_data.data_len = ota_data_req->req_data_len;
    uint32_t datalen = sizeof(sle_ota_comm_msg_t) + sizeof(sle_ota_send_data_t);
    sle_ota_comm_msg_t *msg = (sle_ota_comm_msg_t *)malloc(datalen);
    if (msg) {
        msg->opcode = SIMPLE_OTA_GOING;
        memcpy_s(msg->data, sizeof(sle_ota_send_data_t), &g_ota_send_data, sizeof(sle_ota_send_data_t));
        ret = sle_uart_server_send_report_by_handle((uint8_t *)msg, datalen);
        free(msg);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}
