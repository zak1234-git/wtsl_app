/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */

#include <stdio.h>
#include <string.h>
#include "stdint.h"
#include "utils_tool.h"

uint32_t get_ota_file_size(void)
{
    FILE *fp;
    uint32_t len;

    fp = fopen("fota.fwpkg", "rb");
    if (fp == NULL) {
        SAMPLE_LOG("file open fail");
        return 0;
    }

    int32_t ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) {
        SAMPLE_LOG("fseek failed");
        fclose(fp);
        return 0;
    }

    len = ftell(fp);
    ret = fclose(fp);
    if (ret != 0) {
        SAMPLE_LOG("fclose failed");
        return 0;
    }

    SAMPLE_LOG("file size = %d\n", len);

    return len;
}

FILE *get_ota_fp(void)
{
    FILE *fp = fopen("fota.fwpkg", "rb"); // 使用二进制模式
    if (fp == NULL) {
        SAMPLE_LOG("fopen ota file failed"); // 打印系统错误信息
        return NULL;
    }
    SAMPLE_LOG("fopen ota file succ"); // 打印系统错误信息
    return fp;
}