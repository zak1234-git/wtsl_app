#ifndef _WTSL_CORE_DATAPARSER_OTA_H_
#define _WTSL_CORE_DATAPARSER_OTA_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <microhttpd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cjson/cJSON.h>

#define UPGRADE_PATH_SIZE 256
#define CMD_SIZE 256
#define MD5_LEN 32
#define KO_NAME_LEN 32
#define MAX_KO_FILES 10
#define REBOOT_FLAG_FILE "/tmp/reboot_flag"
#define UPGRADE_SUCCESS_FILE "/tmp/upgrade_success"

typedef struct {
    char wt_koname[KO_NAME_LEN];
    char md5[MD5_LEN + 1];
}wtko_md5_t;

// 根据提供的JSON格式定义新的头结构体
typedef struct {
    char filename[64];        // 文件名
    char md5sum[33];          // MD5校验和
    char filesize[16];        // 文件大小
    char version[32];         // 版本号 v1.1.01.B212
    char filetype[16];        // 文件类型 tar.gz
    char firmwaretype[16];            // 类型 app;firmware;os
    char up_method[16];       // 升级方法 kill;reboot
    char process[32];         // 进程名
    char targetarch[16];      // 平台架构

    wtko_md5_t ko_files[4];
} firmware_header_t;

// 文件上传处理函数
int parse_firmware_header(const char *header_data, firmware_header_t *header);
int calculate_file_md5(const char *filename, char *md5_sum);
int extract_and_verify_firmware(const char *upload_file, const firmware_header_t *header);
int execute_command(const char *command);
int handle_firmware_upgrade(const firmware_header_t *header, const char *version_number);
void mark_upgrade_success(void);
int Platform_verify(firmware_header_t *header);

#endif