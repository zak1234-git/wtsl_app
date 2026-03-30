/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */
#ifndef SLE_SIMPLE_OTA_FOPS_H
#define SLE_SIMPLE_OTA_FOPS_H

#include <stdint.h>
#include <stddef.h>
#include "errcode.h"

uint32_t get_ota_file_size(void);
FILE *get_ota_fp(void);

#endif