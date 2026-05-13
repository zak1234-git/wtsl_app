/*
 * Copyright (c) @CompanyNameMagicTag 2025. All rights reserved.
 * Description: simple ota sample
 * Author: @CompanyNameTag
 * Create: 2025-09-17
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "utils_tool.h"
#include "sle_simple_ota_cli.h"
#include "wtsl_core_api.h"
#include "sle_common.h"

#define CLI_MAX_ARG_NUM 10
extern void fast_data_recv_cb(uint16_t rmt_hdl, uint8_t *data, uint16_t data_len);
extern SPLINK_INFO global_node_info;


static void parse_cli(void)
{
    for (;;) {
        // Parse command line from stdin
        char input[100] = {0};
        char *command;
        char *temp;
        int args[CLI_MAX_ARG_NUM];
        int num_args = 0;

        printf("Enter command> \n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            continue;
        }

        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';
        }

        // 命令解析
        command = strtok(input, " ");
        if (command == NULL || strlen(command) == 0 || command[0] == '\n') {
            continue;
        }

        do {
            temp = strtok(NULL, " ");
            if (temp != NULL) {
                args[num_args] = atoi(temp);
                num_args++;
            }
        } while (temp != NULL && num_args < CLI_MAX_ARG_NUM);

        // 输出结果
        printf("Command: %s. ", command);
        printf("Arguments %d: ", num_args);
        for (int i = 0; i < num_args; i++) {
            printf("%d ", args[i]);
        }
        printf("\n");

        process_cli_cmd(command, args, num_args);
    }
}

int sle_server_init(int argc, char *argv[])
{
    SAMPLE_LOG("spark link main start");

	WTSLNodeBasicInfo* basicinfo = &global_node_info.node_info.basic_info;
	
    sle_simple_ota_init();

    enable_sle();
    
	sle_tm_fast_register_cbks(fast_data_recv_cb);

	sle_addr_t addr;
	sle_set_local_name(basicinfo->sle_name, sizeof(basicinfo->sle_name));
	sscanf(basicinfo->mac, "%x:%x:%x:%x:%x:%x", &addr.addr[0], &addr.addr[1], &addr.addr[2], &addr.addr[3], &addr.addr[4], &addr.addr[5]);
	sle_set_local_addr(&addr);
    
    //parse_cli();
}
