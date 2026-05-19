#ifndef ACL_CORE_H
#define ACL_CORE_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>

#define ACL_MAX_CMD_LEN 4096
#define ACL_MAX_RULES 256

// 全局状态结构体
typedef struct {
    bool enabled;              // 防火墙总开关状态
    char rules[ACL_MAX_RULES][ACL_MAX_CMD_LEN]; // 规则快照
    int rule_count;            // 当前规则数量
    char default_device[32];   // 默认网络设备
} AclGlobalState;

// 全局变量声明
extern AclGlobalState g_acl_state;

// 初始化状态
void acl_init_state(const char *default_dev);

// 切换开关
int acl_toggle_switch(bool enable);

// 获取状态 (JSON 格式)
int acl_get_status(cJSON *resp);

// --- 独立 API 处理函数 ---

/**
 * @brief 处理 IPTables 规则请求
 * @param action: add, delete, replace, list
 * @param chain: INPUT, OUTPUT, FORWARD
 * @param params: JSON 参数
 * @param resp: JSON 响应
 */
int acl_handle_request(const char *action, const char *chain, cJSON *params, cJSON *resp);

#endif
