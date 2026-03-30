#ifndef _WTSL_CFG_MANAGER_H
#define _WTSL_CFG_MANAGER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slb/wtsl_core_slb_interface.h"

// 配置限制
#define CONFIG_MAX_KEY_LENGTH   64
#define CONFIG_MAX_VALUE_LENGTH 256
#define CONFIG_MAX_LINE_LENGTH  512
#define CONFIG_DEFAULT_FILE     "app.conf"

// 错误码定义
typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_ERROR_IO = -1,
    CONFIG_ERROR_MEMORY = -2,
    CONFIG_ERROR_NOT_FOUND = -3,
    CONFIG_ERROR_INVALID_PARAM = -4,
    CONFIG_ERROR_PARSE = -5,
    CONFIG_ERROR_LOCK = -6,
    CONFIG_ERROR_FORMAT = -7
} config_result_t;

// 存储设备定义
typedef enum {
	STORAGE_TYPE_UNKNOW = -1,
    STORAGE_TYPE_NANDFLASH = 0,
	STORAGE_TYPE_EMMC
	
} STORAGE_TYPE;


int config_set(const char* key, char* value);
int config_set_int(const char* key, int value);
int config_get(const char* key, char* value, size_t value_len);
int config_get_int(const char* key, int* value);
int config_read(void);



#endif