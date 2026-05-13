#ifndef QOS_CORE_H
#define QOS_CORE_H

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define QOS_MAX_CMD_LEN 4096        // 单条命令最大长度
#define MAX_SNAPSHOT_RULES 256      // 最多保存的规则数量
#define MAX_QOS_RULES 128           // 最多管理的规则数量
#define QOS_RULE_ID_LEN 16          // 规则 ID 长度 "rule_001"
#define QOS_RULE_NAME_LEN 64        // 规则名称长度

// 规则类型
typedef enum {
    QOS_TYPE_TC = 0,
    QOS_TYPE_IPTABLES = 1
} QosRuleType;

// 单条规则
typedef struct {
    char rule_id[QOS_RULE_ID_LEN];       // 规则 ID，如 "rule_001"
    char name[QOS_RULE_NAME_LEN];        // 规则名称
    QosRuleType type;                    // tc 或 iptables
    char action[16];                     // add / delete / replace / change / insert
    char obj_type[16];                   // tc: qdisc/class/filter
    char chain[16];                      // iptables: INPUT/OUTPUT/FORWARD
    char raw_cmd[QOS_MAX_CMD_LEN];       // 生成的原始命令
    char *params_json;                   // 请求参数的 JSON 字符串
    time_t created_at;                   // 创建时间
    bool active;                         // 是否生效
} QosRuleEntry;

// 全局状态结构体
typedef struct {
    bool enabled;                                         // QoS 总开关状态
    char default_device[32];                              // 默认网络设备 (如 br0)
    QosRuleEntry rules[MAX_QOS_RULES];                    // 规则管理表
    int rules_count;                                      // 当前规则数量
    char snapshot_rules[MAX_SNAPSHOT_RULES][QOS_MAX_CMD_LEN]; // 规则快照栈
    int snapshot_count;                                   // 当前快照数量
    int next_rule_index;                                  // 下一条规则的序号
} QosGlobalState;

// 全局变量声明
extern QosGlobalState g_state;

// 初始化状态
void qos_init_state(const char *default_dev);

// 切换开关 (true=开, false=关)
int qos_toggle_switch(bool enable);

// --- 规则管理 API ---

/**
 * @brief 生成下一个规则 ID
 */
void qos_gen_rule_id(char *out_id, int len);

/**
 * @brief 添加规则到管理表（不执行命令，仅记录）
 */
int qos_add_rule_entry(QosRuleEntry *entry);

/**
 * @brief 根据 rule_id 查找规则
 */
QosRuleEntry *qos_find_rule(const char *rule_id);

/**
 * @brief 根据 rule_id 删除规则
 */
int qos_delete_rule(const char *rule_id);

/**
 * @brief 清除所有规则
 */
int qos_clear_all_rules(void);

/**
 * @brief 获取所有规则的 JSON 表示（写入 out_buf）
 */
int qos_list_rules_json(char *out_buf, int buf_size, const char *filter_type);

/**
 * @brief 获取单条规则的 JSON 表示
 */
int qos_get_rule_json(const char *rule_id, char *out_buf, int buf_size);

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

/**
 * @brief 执行 tc show 命令，输出写入 out_buf
 */
int tc_show_all(char *out_buf, int buf_size);

/**
 * @brief 执行 tc -s show 命令（带统计），输出写入 out_buf
 * 包含 Sent/bytes/dropped/overlimits 和 iptables 计数
 */
int tc_show_stats(char *out_buf, int buf_size);

/**
 * @brief 安全校验：检测参数中是否包含命令注入字符
 */
bool qos_is_safe_param(const char *param);

// ==========================================
// T 节点访问控制（防火墙）功能
// ==========================================

/**
 * @brief 访问控制开关状态
 */
int acl_get_enabled_json(char *out_buf, int buf_size);

/**
 * @brief 设置访问控制开关（true=启用, false=关闭）
 */
int acl_set_enabled(bool enable);

/**
 * @brief 获取访问控制规则列表
 */
int acl_list_rules_json(char *out_buf, int buf_size);

/**
 * @brief 添加访问控制规则
 */
int acl_add_rule(cJSON *rule_json, char *out_rule_id, int id_buf_size);

/**
 * @brief 删除访问控制规则
 */
int acl_delete_rule(const char *rule_id);

/**
 * @brief 获取单条访问控制规则
 */
int acl_get_rule_json(const char *rule_id, char *out_buf, int buf_size);

/**
 * @brief 清除所有访问控制规则（同时从 iptables 中删除）
 */
int acl_clear_all_rules(void);

// ==========================================
// QoS 场景 API（预置模板，用户无需了解 tc/iptables）
// ==========================================

/**
 * @brief 获取所有可用场景列表（JSON 写入 out_buf）
 */
int qos_scene_list_json(char *out_buf, int buf_size);

/**
 * @brief 场景 1：带宽限速
 * 自动创建：qdisc root → class(限速) → filter(匹配端口)
 * @param class_id 类 ID（需唯一，建议从 10 起递增）
 * @param port 目标端口
 * @param protocol "tcp" 或 "udp"
 * @param rate 限制速率，如 "100mbit"
 * @param ceil 峰值上限，如 "200mbit"
 */
int qos_scene_bandwidth_limit(int class_id, int port, const char *protocol,
                              const char *rate, const char *ceil);

/**
 * @brief 场景 2：流量阻断
 * 自动创建：iptables -A FORWARD -p <proto> --dport <port> -j DROP
 */
int qos_scene_traffic_block(int port, const char *protocol);

/**
 * @brief 场景 3：流量放行（高优先级）
 * 自动创建：iptables + tc 高优先级队列
 */
int qos_scene_traffic_allow(int port, const char *protocol);

/**
 * @brief 场景 4：设备限速（按 IP）
 * 自动创建：tc class + filter 匹配源 IP
 * @param ip 目标设备 IP
 * @param rate 限制速率
 * @param ceil 峰值上限
 */
int qos_scene_device_limit(const char *ip, const char *rate, const char *ceil);

/**
 * @brief 场景 5：游戏加速
 * 自动放行常见游戏端口并设置最高优先级队列
 * @param port 游戏端口（0 表示使用默认端口列表）
 */
int qos_scene_gaming_boost(int port);

/**
 * @brief 场景 6：视频流畅
 * 自动为视频流端口（80, 443, 1935, 5000-5100）设置保障带宽
 */
int qos_scene_video_smooth(void);

/**
 * @brief 场景 7：IoT 设备保障
 * 为指定 IP 的 IoT 设备保障最低带宽
 * @param ip IoT 设备 IP
 * @param rate 保障速率
 */
int qos_scene_iot_qos(const char *ip, const char *rate);

/**
 * @brief 场景 8：网页浏览保障
 * 自动为 HTTP/HTTPS 端口设置保障带宽
 */
int qos_scene_web_browse(void);

#endif