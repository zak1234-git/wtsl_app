/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */
#ifndef UTILS_TOOL_H
#define UTILS_TOOL_H

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define EOK 0
#define SAMPLE_LOG(fmt, ...) \
    printf("--++--" fmt " @%s line %d\r\n", ##__VA_ARGS__, __FUNCTION__, __LINE__)

#endif