#ifndef __WTSL_CORE_DATAPARSER_H
#define __WTSL_CORE_DATAPARSER_H
#include "wtsl_core_slb_interface.h"
#include <cjson/cJSON.h>

typedef struct
{
	char essid[128];
	int cell_id;
	int channel;
	int rssi;
	int domain_cnt;
}show_bss_t;

/*
	slb set node
*/

int wtsl_core_parse_json_setnode(const char *args);

/*
	slb set adv info
*/
int wtsl_core_parse_json_set_node_advinfo(const char *args);

/*
	slb tnode scan
*/
int wtsl_core_parse_json_scan(void *args);

/*
	slb join net
*/
int wtsl_core_parse_json_tnode_join_net(void *args);

/*
	slb show link info
*/
int wtsl_core_parse_json_tnode_show_bss(void *args);


/*
	restful api response
*/
char *data_response_to_json(char status,const char *data);
int wtsl_core_parse_json_self_test(void *args);
int wtsl_core_parse_json_view_users(void *args);
int wtsl_core_parse_json_get_hw_resources_usage(void *args);
void* wtsl_core_parse_json_extract_file_header(void *args);
int wtsl_core_parse_json_do_shortrange(const char *args);
int wtsl_core_parse_json_do_remoterange(const char *args);
int wtsl_core_parse_json_do_lowpow(const char *args);
int wtsl_core_parse_json_do_lowlatency(const char *args);
void* wtsl_core_firmware_upgrade(void *args);
void parse_line_param(cJSON* item, const char* line, char* param, char symol);
void parse_param(cJSON* response, const char* data,  char* param, char symol);
void parse_strings_show_bss(cJSON* item, show_bss_t* bss_info);
int get_cpu_usage_stats(double *overall_usage, double core_usage[4]);
int get_load_average(double *load1, double *load5, double*load15);
int get_memory_usage(unsigned long *total, unsigned long *used, unsigned long *free, double *usage_percent);
int get_disk_usage(unsigned long *total, unsigned long *used, unsigned long *free, double *usage_percent);
void format_bytes(unsigned long bytes, char *buffer, size_t buffer_size);
void parse_host_version(char* host_verison);
char* system_output_to_string(const char* command);
char* parse_view_users_output(const char *data) ;



#endif
