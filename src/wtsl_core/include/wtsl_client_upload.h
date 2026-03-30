/*
 * LogManager.h
 *
 *  Create and modify on:  2025年10月27日
 *      Author: vitoyang
 */

#ifndef INTERFACE_CLIENTUPLOAD_H_
#define INTERFACE_CLIENTUPLOAD_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TCP_BUFFER_SIZE                 8192    // TCP 缓冲区大小
#define UPLOAD_RETRY_COUNT              1
#define UPLOAD_CHECK_INTERVAL           1
#define WTSL_EXTERNAL_PORT              6150    // 星闪对外传输端口号
#define WTSL_UPLOAD_TIMEOUT             10
#define WTSL_RECONNECT_INTERVAL         3

// 协议定义
#define FRAME_HEADER                    0x5A5A
#define FRAME_FOOTER                    0x0D0A
#define DATA_IDENTIFIER_FILE            0x02        // 文件数据格式标识
#define DATA_IDENTIFIER_STRUCT          0x03        // 结构体数据格式标识
#define EXTENDED_IDENTIFIER             0x0000      // 默认扩展标识

// 数据类型定义
#define DATA_TYPE_LOG_UPLOAD            0x87000000  // 日志上传数据类型
#define DATA_INFO_LOG_UPLAOD            0x82000000  // 日志文件信息数据类型

// ACK类型定义
#define ACK_INFO_VERIFY                 "wt_info_ACK"
#define ACK_FILEDATA_VERIFY             "wt_filedata_ACK"

// 地址结构
typedef struct {
    uint16_t reserved:2;        // 保留位
    uint16_t subnet_id:14;      // 子网号/G节点标识(1-16100)
    uint8_t t_node_id;          // T节点编号(1-255)
    uint8_t device_id;          // 设备编号(1-255)
} address_32bit_t;

typedef struct {
    char server_ip[16];             // server ip
    int server_port;                // server port
    address_32bit_t source_addr;    // src address
    address_32bit_t dest_addr;      // dest address
    int timeout_seconds;            // connect timeout
    int reconnect_interval;         // reconnect time interval
} tcp_upload_config_t;

typedef struct {
    char filename[256];
    size_t file_size;
    bool is_dir;
} file_info_t;

// exec log upload and clearup
int upload_and_clear_logs(void);

// 检查是否需要上传并清空日志文件
int need_upload_and_clear(void);

// 手动触发日志上传
int wtsl_logger_upload_logs(void);

// 设置最大文件大小
void wtsl_logger_set_max_file_size(size_t max_size);

// 获取当前日志文件大小
size_t wtsl_logger_get_current_file_size(void);

// 设置TCP上传配置
void wtsl_logger_set_upload_config_ex(const char* server_ip, int server_port, 
                                    uint16_t src_subnet, uint8_t src_tnode, uint8_t src_device,
                                    uint16_t dest_subnet, uint8_t det_tnode, uint8_t dest_device,
                                    int timeout, int reconnect_interval);

void wtsl_logger_set_upload_config(const char* server_ip, int server_port,
                                    int timeout, int reconnect_interval);

// 测试TCP连接
int wtsl_logger_test_connection(void);

// backstage upload thread function
void* upload_thread_func(void *arg);

#endif