#ifndef QOS_CORE_H
#define QOS_CORE_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>

#define QOS_MAX_CMD_LEN 4096       // 单条命令最大长度
#define MAX_SNAPSHOT_RULES 256 // 最多保存的规则数量

// 全局状态结构体
typedef struct {
    bool enabled;              // QoS 总开关状态
    char snapshot_rules[MAX_SNAPSHOT_RULES][QOS_MAX_CMD_LEN]; // 规则快照栈
    int snapshot_count;        // 当前快照数量
    char default_device[32];   // 默认网络设备 (如 br0)
} QosGlobalState;

// 全局变量声明
extern QosGlobalState g_state;

// 初始化状态
void qos_init_state(const char *default_dev);

// 切换开关 (true=开, false=关)
int qos_toggle_switch(bool enable);


// --- 独立 API 处理函数 ---

/**
 * @brief 处理 TC (Traffic Control) 请求
 * @param action: add, delete, replace, change, show
 * @param obj_type: qdisc, class, filter
 * @param params: JSON 参数
 */
int tc_handle_request(const char *action, const char *obj_type, cJSON *params);


/**
 * @brief 处理 IPTables 请求
 * @param action: add (append), delete, insert, replace, show
 * @param chain: INPUT, OUTPUT, FORWARD
 * @param params: JSON 参数
 */
int iptables_handle_request(const char *action, const char *chain, cJSON *params);


// int restore_rules(QosGlobalState *state);

#endif