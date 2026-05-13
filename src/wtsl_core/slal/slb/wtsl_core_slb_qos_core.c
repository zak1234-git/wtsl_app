#include "wtsl_core_slb_qos_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "wtsl_log_manager.h"

#define MODULE_NAME "qos_core"

// 定义全局状态
QosGlobalState g_state;

/**
 * @brief 初始化全局状态
 * @param default_dev 默认管理的网络接口，例如 "br0"
 */
void qos_init_state(const char *default_dev) {
    g_state.enabled = true; // 默认认为规则是生效的
    g_state.snapshot_count = 0;
    g_state.rules_count = 0;
    g_state.next_rule_index = 1;
    memset(g_state.snapshot_rules, 0, sizeof(g_state.snapshot_rules));
    memset(g_state.rules, 0, sizeof(g_state.rules));
    // 安全拷贝默认设备名
    strncpy(g_state.default_device, default_dev, sizeof(g_state.default_device) - 1);
    g_state.default_device[sizeof(g_state.default_device) - 1] = '\0';

    WTSL_LOG_INFO(MODULE_NAME, "[QoS] Engine Initialized. Default Device: %s", g_state.default_device);
}

/**
 * @brief 执行 Shell 命令
 * @param cmd 要执行的完整命令
 * @return 0 表示成功，-1 表示失败
 */
static int ex_command(const char *cmd) {
    WTSL_LOG_DEBUG(MODULE_NAME, "[EXEC] %s", cmd);

    // 使用 popen 执行命令
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "popen failed");
        return -1;
    }

    // 读取输出 (主要用于 show 命令)
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(cmd, "show") || strstr(cmd, "-L")) {
            WTSL_LOG_DEBUG(MODULE_NAME, "[OUT] %s", buffer);
        }
    }

    int status = pclose(fp);
    if (WEXITSTATUS(status) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[Error] Command failed with exit code %d", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

// 记录快照
static void record_rule(const char *cmd) {
    if (g_state.snapshot_count >= MAX_SNAPSHOT_RULES) return;
    // 简单去重
    for (int i = 0; i <g_state.snapshot_count; i++) {
        if (strcmp(g_state.snapshot_rules[i], cmd) == 0) return;
    }
    strncpy(g_state.snapshot_rules[g_state.snapshot_count], cmd, QOS_MAX_CMD_LEN - 1);
    g_state.snapshot_count++;
    WTSL_LOG_DEBUG(MODULE_NAME, "[SNAPSHOT] Recorded #%d\n", g_state.snapshot_count);
}

/**
 * @brief 安全校验：检测参数中是否包含命令注入字符
 */
bool qos_is_safe_param(const char *param) {
    if (!param) return false;
    const char *dangerous[] = {
        ";", "|", "&&", "||", "$(", "`", ">", "<", "\n", "\r",
        "&", "(", ")", "{", "}", "!", "~"
    };
    for (int i = 0; i < (int)(sizeof(dangerous)/sizeof(dangerous[0])); i++) {
        if (strstr(param, dangerous[i]) != NULL) {
            return false;
        }
    }
    return true;
}

// ==========================================
// 规则管理 API 实现
// ==========================================

void qos_gen_rule_id(char *out_id, int len) {
    snprintf(out_id, len, "rule_%03d", g_state.next_rule_index++);
}

int qos_add_rule_entry(QosRuleEntry *entry) {
    if (g_state.rules_count >= MAX_QOS_RULES) {
        WTSL_LOG_ERROR(MODULE_NAME, "[RULE] Max rules (%d) reached", MAX_QOS_RULES);
        return -1;
    }
    QosRuleEntry *rule = &g_state.rules[g_state.rules_count];
    memcpy(rule, entry, sizeof(QosRuleEntry));
    rule->created_at = time(NULL);
    rule->active = true;
    g_state.rules_count++;
    WTSL_LOG_INFO(MODULE_NAME, "[RULE] Added %s: %s", rule->rule_id, rule->name);
    return g_state.rules_count - 1; // 返回索引
}

QosRuleEntry *qos_find_rule(const char *rule_id) {
    for (int i = 0; i < g_state.rules_count; i++) {
        if (strcmp(g_state.rules[i].rule_id, rule_id) == 0) {
            return &g_state.rules[i];
        }
    }
    return NULL;
}

int qos_delete_rule(const char *rule_id) {
    for (int i = 0; i < g_state.rules_count; i++) {
        if (strcmp(g_state.rules[i].rule_id, rule_id) == 0) {
            // 如果规则有 raw_cmd 且是 add/insert 操作，尝试执行反向删除
            QosRuleEntry *rule = &g_state.rules[i];
            if (strlen(rule->raw_cmd) > 0) {
                char del_cmd[QOS_MAX_CMD_LEN];
                if (rule->type == QOS_TYPE_TC) {
                    // TC 删除: 根据 obj_type 和 handle/parent 构造删除命令
                    snprintf(del_cmd, sizeof(del_cmd), "tc %s del dev %s", rule->obj_type, g_state.default_device);
                    // 附加 handle/parent
                    if (rule->params_json) {
                        cJSON *p = cJSON_Parse(rule->params_json);
                        if (p) {
                            cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(p, "handle");
                            cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(p, "parent");
                            if (j_handle) {
                                char buf[32]; snprintf(buf, sizeof(buf), "%s", j_handle->valuestring);
                                strncat(del_cmd, " handle ", sizeof(del_cmd) - strlen(del_cmd) - 1);
                                strncat(del_cmd, buf, sizeof(del_cmd) - strlen(del_cmd) - 1);
                            }
                            if (j_parent) {
                                char buf[32]; snprintf(buf, sizeof(buf), "%s", j_parent->valuestring);
                                strncat(del_cmd, " parent ", sizeof(del_cmd) - strlen(del_cmd) - 1);
                                strncat(del_cmd, buf, sizeof(del_cmd) - strlen(del_cmd) - 1);
                            }
                            cJSON_Delete(p);
                        }
                    }
                    ex_command(del_cmd);
                } else if (rule->type == QOS_TYPE_IPTABLES) {
                    // iptables 删除: 将 -A/-I 替换为 -D
                    strncpy(del_cmd, rule->raw_cmd, sizeof(del_cmd) - 1);
                    char *pos;
                    if ((pos = strstr(del_cmd, "iptables -A"))) memcpy(pos, "iptables -D", 13);
                    else if ((pos = strstr(del_cmd, "iptables -I"))) memcpy(pos, "iptables -D", 13);
                    ex_command(del_cmd);
                }
            }
            // 释放 params_json 字符串
            if (rule->params_json) {
                free(rule->params_json);
                rule->params_json = NULL;
            }
            // 移动后面的规则覆盖这个位置
            for (int j = i; j < g_state.rules_count - 1; j++) {
                g_state.rules[j] = g_state.rules[j + 1];
            }
            memset(&g_state.rules[g_state.rules_count - 1], 0, sizeof(QosRuleEntry));
            g_state.rules_count--;
            WTSL_LOG_INFO(MODULE_NAME, "[RULE] Deleted %s", rule_id);
            return 0;
        }
    }
    return -1;
}

int qos_clear_all_rules(void) {
    for (int i = 0; i < g_state.rules_count; i++) {
        if (g_state.rules[i].params_json) {
            free(g_state.rules[i].params_json);
        }
    }
    g_state.rules_count = 0;
    g_state.next_rule_index = 1;
    memset(g_state.rules, 0, sizeof(g_state.rules));
    WTSL_LOG_INFO(MODULE_NAME, "[RULE] All rules cleared");
    return 0;
}

int qos_list_rules_json(char *out_buf, int buf_size, const char *filter_type) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *rules_arr = cJSON_CreateArray();

    for (int i = 0; i < g_state.rules_count; i++) {
        QosRuleEntry *rule = &g_state.rules[i];
        // 类型过滤
        if (filter_type) {
            if (strcmp(filter_type, "tc") == 0 && rule->type != QOS_TYPE_TC) continue;
            if (strcmp(filter_type, "iptables") == 0 && rule->type != QOS_TYPE_IPTABLES) continue;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "rule_id", rule->rule_id);
        cJSON_AddStringToObject(item, "name", rule->name);
        cJSON_AddStringToObject(item, "type", rule->type == QOS_TYPE_TC ? "tc" : "iptables");
        cJSON_AddStringToObject(item, "action", rule->action);
        cJSON_AddBoolToObject(item, "active", rule->active);
        cJSON_AddNumberToObject(item, "created_at", (double)rule->created_at);

        if (strlen(rule->raw_cmd) > 0) {
            cJSON_AddStringToObject(item, "raw_command", rule->raw_cmd);
        }

        // 解析 params JSON 还原
        if (rule->params_json) {
            cJSON *params = cJSON_Parse(rule->params_json);
            if (params) {
                cJSON_AddItemToObject(item, "params", params);
            }
        }

        cJSON_AddItemToArray(rules_arr, item);
    }

    cJSON_AddNumberToObject(data, "total", cJSON_GetArraySize(rules_arr));
    cJSON_AddItemToObject(data, "rules", rules_arr);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

int qos_get_rule_json(const char *rule_id, char *out_buf, int buf_size) {
    QosRuleEntry *rule = qos_find_rule(rule_id);
    if (!rule) return -1;

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddStringToObject(data, "rule_id", rule->rule_id);
    cJSON_AddStringToObject(data, "name", rule->name);
    cJSON_AddStringToObject(data, "type", rule->type == QOS_TYPE_TC ? "tc" : "iptables");
    cJSON_AddStringToObject(data, "action", rule->action);
    cJSON_AddStringToObject(data, "obj_type", rule->obj_type);
    if (rule->chain[0]) cJSON_AddStringToObject(data, "chain", rule->chain);
    cJSON_AddBoolToObject(data, "active", rule->active);
    cJSON_AddNumberToObject(data, "created_at", (double)rule->created_at);

    if (strlen(rule->raw_cmd) > 0) {
        cJSON_AddStringToObject(data, "raw_command", rule->raw_cmd);
    }

    if (rule->params_json) {
        cJSON *params = cJSON_Parse(rule->params_json);
        if (params) {
            cJSON_AddItemToObject(data, "params", params);
        }
    }

    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

/**
 * @brief 执行 tc show 命令，输出写入 out_buf
 */
int tc_show_all(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *qdiscs = cJSON_CreateArray();
    cJSON *classes = cJSON_CreateArray();
    cJSON *filters = cJSON_CreateArray();

    const char *dev = g_state.default_device;

    // tc qdisc show
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc show dev %s 2>/dev/null", dev);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) {
                cJSON_AddItemToArray(qdiscs, cJSON_CreateString(buf));
            }
        }
        pclose(fp);
    }

    // tc class show
    snprintf(cmd, sizeof(cmd), "tc class show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) {
                cJSON_AddItemToArray(classes, cJSON_CreateString(buf));
            }
        }
        pclose(fp);
    }

    // tc filter show
    snprintf(cmd, sizeof(cmd), "tc filter show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) {
                cJSON_AddItemToArray(filters, cJSON_CreateString(buf));
            }
        }
        pclose(fp);
    }

    cJSON_AddItemToObject(data, "qdiscs", qdiscs);
    cJSON_AddItemToObject(data, "classes", classes);
    cJSON_AddItemToObject(data, "filters", filters);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        return -1;
    }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

/**
 * @brief 执行 tc -s show 命令（带统计信息），输出写入 out_buf
 * 返回 qdisc/class/filter 的 Sent/byte/dropped/overlimits 等统计
 * 以及 iptables -L -v -n -x 计数信息
 */
int tc_show_stats(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *qdiscs = cJSON_CreateArray();
    cJSON *classes = cJSON_CreateArray();
    cJSON *filters = cJSON_CreateArray();

    const char *dev = g_state.default_device;
    char cmd[256];

    // tc -s qdisc show（带统计：Sent bytes packets dropped overlimits）
    snprintf(cmd, sizeof(cmd), "tc -s qdisc show dev %s 2>/dev/null", dev);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) cJSON_AddItemToArray(qdiscs, cJSON_CreateString(buf));
        }
        pclose(fp);
    }

    // tc -s class show
    snprintf(cmd, sizeof(cmd), "tc -s class show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) cJSON_AddItemToArray(classes, cJSON_CreateString(buf));
        }
        pclose(fp);
    }

    // tc -s filter show
    snprintf(cmd, sizeof(cmd), "tc -s filter show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) cJSON_AddItemToArray(filters, cJSON_CreateString(buf));
        }
        pclose(fp);
    }

    // iptables 计数统计（-x 显示精确字节数）
    cJSON *ipt_stats = cJSON_CreateArray();
    snprintf(cmd, sizeof(cmd), "iptables -L FORWARD -v -n -x 2>/dev/null");
    fp = popen(cmd, "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (strlen(buf) > 0) cJSON_AddItemToArray(ipt_stats, cJSON_CreateString(buf));
        }
        pclose(fp);
    }

    cJSON_AddItemToObject(data, "qdiscs", qdiscs);
    cJSON_AddItemToObject(data, "classes", classes);
    cJSON_AddItemToObject(data, "filters", filters);
    cJSON_AddItemToObject(data, "iptables_stats", ipt_stats);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

int tc_handle_request(const char *action, const char *obj_type, cJSON *params) {
    char cmd[QOS_MAX_CMD_LEN];
    cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(params, "device");
    const char *dev = (j_dev && cJSON_IsString(j_dev)) ? j_dev->valuestring : g_state.default_device;

    // 安全校验
    if (!qos_is_safe_param(dev)) {
        WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe device param: %s", dev);
        return -1;
    }

    // 基础命令： tc <action> <type> dev <dev>
    snprintf(cmd, sizeof(cmd), "tc %s %s dev %s", action, obj_type, dev);

    // 公共参数
    cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(params, "parent");
    cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(params, "handle");
    cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(params, "kind");
    cJSON *j_flowid = cJSON_GetObjectItemCaseSensitive(params, "flowid");
    cJSON *j_args = cJSON_GetObjectItemCaseSensitive(params, "args");

    // 安全校验公共参数
    if (j_parent && cJSON_IsString(j_parent) && !qos_is_safe_param(j_parent->valuestring)) {
        WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe parent param");
        return -1;
    }
    if (j_handle && cJSON_IsString(j_handle) && !qos_is_safe_param(j_handle->valuestring)) {
        WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe handle param");
        return -1;
    }

    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) {
        if (j_parent) { strncat(cmd, " parent ", sizeof(cmd) - strlen(cmd) - 1); strncat(cmd, j_parent->valuestring, sizeof(cmd) - strlen(cmd) - 1); }
        if (j_handle) { strncat(cmd, " handle ", sizeof(cmd) - strlen(cmd) - 1); strncat(cmd, j_handle->valuestring, sizeof(cmd) - strlen(cmd) - 1); }
    } else if (strcmp(action, "show") == 0) {
        // show 不需要额外参数
    } else {
        if (j_parent) { strncat(cmd, " parent ", sizeof(cmd) - strlen(cmd) - 1); strncat(cmd, j_parent->valuestring, sizeof(cmd) - strlen(cmd) - 1); }
        if (j_handle) { strncat(cmd, " handle ", sizeof(cmd) - strlen(cmd) - 1); strncat(cmd, j_handle->valuestring, sizeof(cmd) - strlen(cmd) - 1); }

        if (j_kind && (strcmp(action, "add") == 0 || strcmp(action, "replace") == 0 || strcmp(action, "change") == 0)) {
            if (!qos_is_safe_param(j_kind->valuestring)) {
                WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe kind param");
                return -1;
            }
            strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, j_kind->valuestring, sizeof(cmd) - strlen(cmd) - 1);
        }

        // 处理动态 Args
        if (j_args && cJSON_IsObject(j_args)) {
            cJSON *item;
            cJSON_ArrayForEach(item, j_args) {
                const char *val = NULL;
                static char num_buf[32];
                if (cJSON_IsString(item)) {
                    if (!qos_is_safe_param(item->valuestring)) {
                        WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe args value: %s", item->string);
                        return -1;
                    }
                    val = item->valuestring;
                } else if (cJSON_IsNumber(item)) {
                    snprintf(num_buf, sizeof(num_buf), "%g", item->valuedouble);
                    val = num_buf;
                }

                if (val) {
                    strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
                    strncat(cmd, item->string, sizeof(cmd) - strlen(cmd) - 1);
                    strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
                    strncat(cmd, val, sizeof(cmd) - strlen(cmd) - 1);
                }
            }
        }

        // Filter 特有参数
        if (strcmp(obj_type, "filter") == 0) {
            // handle: 用于匹配 iptables MARK 值
            // 例: tc filter add dev br0 parent 1:0 protocol ip handle 0x10 flowid 1:10
            cJSON *j_match_handle = cJSON_GetObjectItemCaseSensitive(params, "match_handle");
            if (j_match_handle && cJSON_IsString(j_match_handle)) {
                strncat(cmd, " handle ", sizeof(cmd) - strlen(cmd) - 1);
                strncat(cmd, j_match_handle->valuestring, sizeof(cmd) - strlen(cmd) - 1);
            }

            // dsfield: 用于匹配 DSCP 值
            // 例: tc filter add dev br0 parent 1:0 protocol ip dsfield 0x2e flowid 1:20
            cJSON *j_dsfield = cJSON_GetObjectItemCaseSensitive(params, "dsfield");
            if (j_dsfield && cJSON_IsString(j_dsfield)) {
                strncat(cmd, " dsfield ", sizeof(cmd) - strlen(cmd) - 1);
                strncat(cmd, j_dsfield->valuestring, sizeof(cmd) - strlen(cmd) - 1);
            }

            if (j_flowid) {
                strncat(cmd, " flowid ", sizeof(cmd) - strlen(cmd) - 1);
                strncat(cmd, j_flowid->valuestring, sizeof(cmd) - strlen(cmd) - 1);
            }
        }
    }

    int ret = ex_command(cmd);
    if (ret == 0 && (strcmp(action, "add") == 0 || strcmp(action, "replace") == 0)) {
        record_rule(cmd);
    }
    return ret;
}

int iptables_handle_request(const char *action, const char *chain, cJSON *params) {
    char cmd[QOS_MAX_CMD_LEN];

    // 安全校验 chain
    if (!qos_is_safe_param(chain)) {
        WTSL_LOG_ERROR(MODULE_NAME, "[IPT] Unsafe chain param: %s", chain);
        return -1;
    }

    // 映射 action: add -> -A, delete -> -D, insert -> -I
    const char *flag = "-A";
    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) {
        flag = "-D";
    } else if (strcmp(action, "insert") == 0) {
        flag = "-I";
    } else if (strcmp(action, "replace") == 0) {
        flag = "-R";
    }

    snprintf(cmd, sizeof(cmd), "iptables %s %s", flag, chain);

    // 协议
    cJSON *j_proto = cJSON_GetObjectItemCaseSensitive(params, "protocol");
    if (j_proto && cJSON_IsString(j_proto)) {
        if (!qos_is_safe_param(j_proto->valuestring)) {
            WTSL_LOG_ERROR(MODULE_NAME, "[IPT] Unsafe protocol param");
            return -1;
        }
        strncat(cmd, " -p ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_proto->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 接口智能推断
    cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(params, "device");
    const char *dev = (j_dev && cJSON_IsString(j_dev)) ? j_dev->valuestring : g_state.default_device;

    // 检查是否用户显式指定了 -i 或 -o
    cJSON *j_in = cJSON_GetObjectItemCaseSensitive(params, "in_interface");
    cJSON *j_out = cJSON_GetObjectItemCaseSensitive(params, "out_interface");

    if (!j_in && !j_out) {
        // 自动推断
        if (strcmp(chain, "INPUT") == 0 || strcmp(chain, "FORWARD") == 0) {
            strncat(cmd, " -i ", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, dev, sizeof(cmd) - strlen(cmd) - 1);
        } else if (strcmp(chain, "OUTPUT") == 0) {
            strncat(cmd, " -o ", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, dev, sizeof(cmd) - strlen(cmd) - 1);
        }
    } else {
        if (j_in) {
            if (!qos_is_safe_param(j_in->valuestring)) return -1;
            strncat(cmd, " -i ", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, j_in->valuestring, sizeof(cmd) - strlen(cmd) - 1);
        }
        if (j_out) {
            if (!qos_is_safe_param(j_out->valuestring)) return -1;
            strncat(cmd, " -o ", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, j_out->valuestring, sizeof(cmd) - strlen(cmd) - 1);
        }
    }

    // 处理Args (映射到 iptables 参数)
    cJSON *j_args = cJSON_GetObjectItemCaseSensitive(params, "args");
    if (j_args && cJSON_IsObject(j_args)) {
        cJSON *item;
        cJSON_ArrayForEach(item, j_args) {
            const char *key = item->string;
            const char *val = NULL;
            static char num_buf[32];
            if (cJSON_IsString(item)) {
                if (!qos_is_safe_param(item->valuestring)) return -1;
                val = item->valuestring;
            } else if (cJSON_IsNumber(item)) {
                snprintf(num_buf, sizeof(num_buf), "%d", (int)item->valuedouble);
                val = num_buf;
            }
            if (!val) continue;

            strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
            if (strcmp(key, "source") == 0 || strcmp(key, "src") == 0) {
                strncat(cmd, "-s ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "destination") == 0 || strcmp(key, "dst") == 0) {
                strncat(cmd, "-d ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "sport") == 0) {
                strncat(cmd, "--sport ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "dport") == 0) {
                strncat(cmd, "--dport ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "jump") == 0 || strcmp(key, "target") == 0) {
                strncat(cmd, "-j ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "set_dscp") == 0 || strcmp(key, "dscp_class") == 0) {
                // T 节点打标: -j DSCP --set-dscp-class EF/AF41/CS3 等
                strncat(cmd, "-j DSCP --set-dscp-class ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "set_mark") == 0 || strcmp(key, "mark") == 0) {
                // T 节点打标: -j MARK --set-mark 0x10
                strncat(cmd, "-j MARK --set-mark ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "match_dscp") == 0) {
                // G 节点过滤: -m dscp --dscp EF
                strncat(cmd, "-m dscp --dscp ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "match_mark") == 0) {
                // G 节点过滤: -m mark --mark 0x10
                strncat(cmd, "-m mark --mark ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "src_mac") == 0 || strcmp(key, "source_mac") == 0) {
                // 访问控制：源 MAC 地址匹配
                strncat(cmd, "-m mac --mac-source ", sizeof(cmd) - strlen(cmd) - 1);
            } else if (strcmp(key, "dst_mac") == 0 || strcmp(key, "dest_mac") == 0) {
                // 访问控制：目的 MAC 地址匹配（iptables 无直接匹配目的 MAC 的标准模块，用 -m mac 模拟）
                strncat(cmd, "-m mac --mac-source ", sizeof(cmd) - strlen(cmd) - 1);
            } else {
                strncat(cmd, "--", sizeof(cmd) - strlen(cmd) - 1);
                strncat(cmd, key, sizeof(cmd) - strlen(cmd) - 1);
                strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
            }

            strncat(cmd, val, sizeof(cmd) - strlen(cmd) - 1);
        }
    }

    int ret = ex_command(cmd);
    if (ret == 0 && (strcmp(action, "add") == 0 || strcmp(action, "insert") == 0 || strcmp(action, "replace") == 0)) {
        record_rule(cmd);
    }
    return ret;
}

/**
 * @brief QoS 总开关逻辑
 * @param enable true=开启 (恢复规则), false=关闭 (清除规则)
 */
int qos_toggle_switch(bool enable) {
    if (g_state.enabled == enable) return 0;

    if (!enable) {
        WTSL_LOG_INFO(MODULE_NAME, "[SWITCH] Disabling QoS...");
        // 1. 清除 TC
        char cmd[QOS_MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root 2>/dev/null", g_state.default_device);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s ingress 2>/dev/null", g_state.default_device);
        system(cmd);

        // 2. 清除 IPTables (回放快照)
        for (int i = 0; i < g_state.snapshot_count; i++) {
            char *rule = g_state.snapshot_rules[i];
            if (strstr(rule, "iptables") != NULL) {
                char del_cmd[QOS_MAX_CMD_LEN];
                strcpy(del_cmd, rule);
                // 替换 -A/-I 为 -D
                if (strstr(del_cmd, "iptables -A")) memcpy(strstr(del_cmd, "iptables -A"), "iptables -D", 13);
                else if (strstr(del_cmd, "iptables -I")) memcpy(strstr(del_cmd, "iptables -I"), "iptables -D", 13);
                else if (strstr(del_cmd, "iptables -R")) continue;

                WTSL_LOG_DEBUG(MODULE_NAME, "[CLEAN] %s", del_cmd);
                system(del_cmd);
            }
        }
        g_state.enabled = false;
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "[SWITCH] Enabling QoS...");
        for (int i = 0; i < g_state.snapshot_count; i++) {
            WTSL_LOG_DEBUG(MODULE_NAME, "[RESTORE] %s", g_state.snapshot_rules[i]);
            system(g_state.snapshot_rules[i]);
        }
        g_state.enabled = true;
    }
    return 0;
}

// ==========================================
// QoS 场景 API 实现
// ==========================================

int qos_scene_list_json(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *scenes = cJSON_CreateArray();

    // 场景 1
    cJSON *s1 = cJSON_CreateObject();
    cJSON_AddStringToObject(s1, "scene", "bandwidth_limit");
    cJSON_AddStringToObject(s1, "title", "带宽限速");
    cJSON_AddStringToObject(s1, "description", "限制指定端口的带宽");
    cJSON *s1_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(s1_req, "class_id", 10);
    cJSON_AddNumberToObject(s1_req, "port", 80);
    cJSON_AddStringToObject(s1_req, "protocol", "tcp");
    cJSON_AddStringToObject(s1_req, "rate", "100mbit");
    cJSON_AddStringToObject(s1_req, "ceil", "200mbit");
    cJSON_AddItemToObject(s1, "request_example", s1_req);
    cJSON_AddItemToArray(scenes, s1);

    // 场景 2
    cJSON *s2 = cJSON_CreateObject();
    cJSON_AddStringToObject(s2, "scene", "traffic_block");
    cJSON_AddStringToObject(s2, "title", "流量阻断");
    cJSON_AddStringToObject(s2, "description", "阻断指定端口的流量");
    cJSON *s2_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(s2_req, "port", 445);
    cJSON_AddStringToObject(s2_req, "protocol", "tcp");
    cJSON_AddItemToObject(s2, "request_example", s2_req);
    cJSON_AddItemToArray(scenes, s2);

    // 场景 3
    cJSON *s3 = cJSON_CreateObject();
    cJSON_AddStringToObject(s3, "scene", "traffic_allow");
    cJSON_AddStringToObject(s3, "title", "流量放行");
    cJSON_AddStringToObject(s3, "description", "放行指定端口的流量并设置高优先级");
    cJSON *s3_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(s3_req, "port", 8080);
    cJSON_AddStringToObject(s3_req, "protocol", "tcp");
    cJSON_AddItemToObject(s3, "request_example", s3_req);
    cJSON_AddItemToArray(scenes, s3);

    // 场景 4
    cJSON *s4 = cJSON_CreateObject();
    cJSON_AddStringToObject(s4, "scene", "device_limit");
    cJSON_AddStringToObject(s4, "title", "设备限速");
    cJSON_AddStringToObject(s4, "description", "限制指定 IP 设备的带宽");
    cJSON *s4_req = cJSON_CreateObject();
    cJSON_AddStringToObject(s4_req, "ip", "192.168.1.100");
    cJSON_AddStringToObject(s4_req, "rate", "50mbit");
    cJSON_AddStringToObject(s4_req, "ceil", "100mbit");
    cJSON_AddItemToObject(s4, "request_example", s4_req);
    cJSON_AddItemToArray(scenes, s4);

    // 场景 5
    cJSON *s5 = cJSON_CreateObject();
    cJSON_AddStringToObject(s5, "scene", "gaming_boost");
    cJSON_AddStringToObject(s5, "title", "游戏加速");
    cJSON_AddStringToObject(s5, "description", "为游戏端口设置最高优先级");
    cJSON *s5_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(s5_req, "port", 0);
    cJSON_AddStringToObject(s5, "default_ports", "8080, 27015-27030, 4380");
    cJSON_AddItemToObject(s5, "request_example", s5_req);
    cJSON_AddItemToArray(scenes, s5);

    // 场景 6
    cJSON *s6 = cJSON_CreateObject();
    cJSON_AddStringToObject(s6, "scene", "video_smooth");
    cJSON_AddStringToObject(s6, "title", "视频流畅");
    cJSON_AddStringToObject(s6, "description", "为视频流端口设置保障带宽");
    cJSON *s6_req = cJSON_CreateObject();
    cJSON_AddItemToObject(s6, "request_example", s6_req);
    cJSON_AddStringToObject(s6, "default_ports", "80, 443, 1935, 5000-5100");
    cJSON_AddItemToArray(scenes, s6);

    // 场景 7
    cJSON *s7 = cJSON_CreateObject();
    cJSON_AddStringToObject(s7, "scene", "iot_qos");
    cJSON_AddStringToObject(s7, "title", "IoT 设备保障");
    cJSON_AddStringToObject(s7, "description", "为 IoT 设备保障最低带宽");
    cJSON *s7_req = cJSON_CreateObject();
    cJSON_AddStringToObject(s7_req, "ip", "192.168.1.50");
    cJSON_AddStringToObject(s7_req, "rate", "10mbit");
    cJSON_AddItemToObject(s7, "request_example", s7_req);
    cJSON_AddItemToArray(scenes, s7);

    // 场景 8
    cJSON *s8 = cJSON_CreateObject();
    cJSON_AddStringToObject(s8, "scene", "web_browse");
    cJSON_AddStringToObject(s8, "title", "网页浏览保障");
    cJSON_AddStringToObject(s8, "description", "为 HTTP/HTTPS 设置保障带宽");
    cJSON *s8_req = cJSON_CreateObject();
    cJSON_AddItemToObject(s8, "request_example", s8_req);
    cJSON_AddItemToArray(scenes, s8);

    cJSON_AddItemToObject(data, "scenes", scenes);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

int qos_scene_bandwidth_limit(int class_id, int port, const char *protocol,
                              const char *rate, const char *ceil) {
    const char *dev = g_state.default_device;
    int ret = 0;

    // 安全校验
    if (!qos_is_safe_param(rate) || !qos_is_safe_param(ceil)) return -1;

    // 1. 确保 root qdisc 存在（HTB）
    ret = system("tc qdisc show dev %s 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");
    if (ret != 0) WTSL_LOG_WARNING(MODULE_NAME, "[SCENE] root qdisc may already exist");

    // 2. 创建限速 class
    char cmd[QOS_MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s",
             dev, class_id, rate, ceil);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);
    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] bandwidth_limit class 1:%d created: rate=%s ceil=%s", class_id, rate, ceil);

    // 3. 添加 filter 匹配端口
    const char *proto_list[] = {"tcp", "udp", NULL};
    if (protocol == NULL || strcmp(protocol, "both") == 0) {
        // both: 同时添加 tcp 和 udp
        for (int i = 0; proto_list[i] != NULL; i++) {
            snprintf(cmd, sizeof(cmd),
                     "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d",
                     dev, proto_list[i], port, class_id);
            ex_command(cmd);
            record_rule(cmd);
        }
    } else {
        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d",
                 dev, protocol, port, class_id);
        ret = ex_command(cmd);
        if (ret != 0) return ret;
        record_rule(cmd);
    }

    return 0;
}

int qos_scene_traffic_block(int port, const char *protocol) {
    char cmd[QOS_MAX_CMD_LEN];
    const char *proto_list[] = {"tcp", "udp", NULL};
    int ret = 0;

    if (protocol == NULL || strcmp(protocol, "both") == 0) {
        for (int i = 0; proto_list[i] != NULL; i++) {
            snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s --dport %d -j DROP", proto_list[i], port);
            ret = ex_command(cmd);
            if (ret == 0) record_rule(cmd);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s --dport %d -j DROP", protocol, port);
        ret = ex_command(cmd);
        if (ret == 0) record_rule(cmd);
    }

    if (ret == 0) WTSL_LOG_INFO(MODULE_NAME, "[SCENE] traffic_block port=%d proto=%s", port, protocol ? protocol : "both");
    return ret;
}

int qos_scene_traffic_allow(int port, const char *protocol) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    // 1. iptables 放行
    const char *proto_list[] = {"tcp", "udp", NULL};
    if (protocol == NULL || strcmp(protocol, "both") == 0) {
        for (int i = 0; proto_list[i] != NULL; i++) {
            snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s -i %s --dport %d -j ACCEPT", proto_list[i], dev, port);
            ex_command(cmd);
            record_rule(cmd);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s -i %s --dport %d -j ACCEPT", protocol, dev, port);
        ex_command(cmd);
        record_rule(cmd);
    }

    // 2. tc 高优先级 class（prio 1，最高优先级）
    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 创建高优先级 class
    int class_id = port % 100 + 200; // 用端口生成唯一 class_id
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:%d htb rate 100mbit ceil 1000mbit prio 1",
             dev, class_id);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // filter 匹配
    const char *p = protocol ? protocol : "tcp";
    snprintf(cmd, sizeof(cmd),
             "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d",
             dev, p, port, class_id);
    ex_command(cmd);
    record_rule(cmd);

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] traffic_allow port=%d proto=%s", port, protocol ? protocol : "both");
    return 0;
}

int qos_scene_device_limit(const char *ip, const char *rate, const char *ceil) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    if (!qos_is_safe_param(ip) || !qos_is_safe_param(rate) || !qos_is_safe_param(ceil)) return -1;

    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 从 IP 生成 class_id（取最后一段）
    int last_octet = 0;
    sscanf(ip, "%*d.%*d.%*d.%d", &last_octet);
    int class_id = last_octet + 50;

    // 创建 class
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s",
             dev, class_id, rate, ceil);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // filter 匹配源 IP
    snprintf(cmd, sizeof(cmd),
             "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip src %s flowid 1:%d",
             dev, ip, class_id);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] device_limit ip=%s rate=%s ceil=%s", ip, rate, ceil);
    return 0;
}

int qos_scene_gaming_boost(int port) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 创建最高优先级 class (prio 1)
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:201 htb rate 50mbit ceil 1000mbit prio 1",
             dev);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // 游戏端口列表
    int default_ports[] = {8080, 27015, 27016, 27017, 27018, 27019, 27020, 4380, 0};
    int *ports = default_ports;
    int custom_port_arr[2] = {port, 0};
    if (port > 0) ports = custom_port_arr;

    for (int i = 0; ports[i] != 0; i++) {
        // iptables 放行
        snprintf(cmd, sizeof(cmd),
                 "iptables -A FORWARD -p udp -i %s --dport %d -j ACCEPT", dev, ports[i]);
        ex_command(cmd);
        record_rule(cmd);

        // tc filter 高优先级
        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip udp dport %d 0xffff flowid 1:201",
                 dev, ports[i]);
        ex_command(cmd);
        record_rule(cmd);
    }

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] gaming_boost port=%d", port);
    return 0;
}

int qos_scene_video_smooth(void) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 创建视频保障 class (prio 2)
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:202 htb rate 30mbit ceil 500mbit prio 2",
             dev);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // 视频端口：80, 443, 1935(RTMP), 5000-5100
    int video_ports[] = {80, 443, 1935, 0};
    for (int i = 0; video_ports[i] != 0; i++) {
        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s parent 1:0 protocol ip prio 2 u32 match ip tcp dport %d 0xffff flowid 1:202",
                 dev, video_ports[i]);
        ex_command(cmd);
        record_rule(cmd);
    }

    // iptables 放行
    for (int i = 0; video_ports[i] != 0; i++) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -A FORWARD -p tcp -i %s --dport %d -j ACCEPT", dev, video_ports[i]);
        ex_command(cmd);
        record_rule(cmd);
    }

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] video_smooth");
    return 0;
}

int qos_scene_iot_qos(const char *ip, const char *rate) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    if (!qos_is_safe_param(ip) || !qos_is_safe_param(rate)) return -1;

    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 从 IP 生成 class_id
    int last_octet = 0;
    sscanf(ip, "%*d.%*d.%*d.%d", &last_octet);
    int class_id = last_octet + 150;

    // 创建 IoT 保障 class (prio 3)
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil 100mbit prio 3",
             dev, class_id, rate);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // filter 匹配源 IP
    snprintf(cmd, sizeof(cmd),
             "tc filter add dev %s parent 1:0 protocol ip prio 3 u32 match ip src %s flowid 1:%d",
             dev, ip, class_id);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] iot_qos ip=%s rate=%s", ip, rate);
    return 0;
}

int qos_scene_web_browse(void) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    int ret = 0;

    // 确保 root qdisc 存在
    system("tc qdisc show dev br0 2>/dev/null | grep -q 'root.*htb' || tc qdisc add dev br0 root handle 1: htb default 99");

    // 创建网页保障 class (prio 2)
    snprintf(cmd, sizeof(cmd),
             "tc class add dev %s parent 1:1 classid 1:203 htb rate 20mbit ceil 200mbit prio 2",
             dev);
    ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);

    // HTTP/HTTPS filter
    int web_ports[] = {80, 443, 0};
    for (int i = 0; web_ports[i] != 0; i++) {
        snprintf(cmd, sizeof(cmd),
                 "tc filter add dev %s parent 1:0 protocol ip prio 2 u32 match ip tcp dport %d 0xffff flowid 1:203",
                 dev, web_ports[i]);
        ex_command(cmd);
        record_rule(cmd);
    }

    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] web_browse");
    return 0;
}

// ==========================================
// T 节点访问控制（防火墙）功能实现
// ==========================================

// 访问控制全局状态
static bool g_acl_enabled = false;  // 默认关闭
static QosRuleEntry g_acl_rules[MAX_QOS_RULES];
static int g_acl_rules_count = 0;
static int g_acl_next_rule_index = 1;

/**
 * @brief 获取访问控制开关状态
 */
int acl_get_enabled_json(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "success");
    cJSON_AddBoolToObject(root, "enabled", g_acl_enabled);
    cJSON_AddNumberToObject(root, "rules_count", g_acl_rules_count);

    char *json_str = cJSON_Print(root);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

/**
 * @brief 设置访问控制开关
 * true=启用（应用所有规则），false=关闭（清除所有 iptables ACL 规则）
 */
int acl_set_enabled(bool enable) {
    if (g_acl_enabled == enable) return 0;

    if (!enable) {
        // 关闭：删除所有 ACL iptables 规则
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Disabling access control, removing %d rules", g_acl_rules_count);
        for (int i = 0; i < g_acl_rules_count; i++) {
            if (strlen(g_acl_rules[i].raw_cmd) > 0) {
                // 将 -A 替换为 -D
                char del_cmd[QOS_MAX_CMD_LEN];
                strncpy(del_cmd, g_acl_rules[i].raw_cmd, sizeof(del_cmd) - 1);
                char *pos = strstr(del_cmd, "iptables -A");
                if (pos) memcpy(pos, "iptables -D", 13);
                ex_command(del_cmd);
            }
            if (g_acl_rules[i].params_json) {
                free(g_acl_rules[i].params_json);
                g_acl_rules[i].params_json = NULL;
            }
        }
        g_acl_rules_count = 0;
        g_acl_next_rule_index = 1;
        memset(g_acl_rules, 0, sizeof(g_acl_rules));
        g_acl_enabled = false;
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Access control disabled");
    } else {
        // 启用：重新应用所有规则
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Enabling access control, applying %d rules", g_acl_rules_count);
        for (int i = 0; i < g_acl_rules_count; i++) {
            if (strlen(g_acl_rules[i].raw_cmd) > 0) {
                ex_command(g_acl_rules[i].raw_cmd);
            }
        }
        g_acl_enabled = true;
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Access control enabled");
    }
    return 0;
}

/**
 * @brief 获取访问控制规则列表
 */
int acl_list_rules_json(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *rules_arr = cJSON_CreateArray();

    for (int i = 0; i < g_acl_rules_count; i++) {
        QosRuleEntry *rule = &g_acl_rules[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "rule_id", rule->rule_id);
        cJSON_AddStringToObject(item, "name", rule->name);
        cJSON_AddStringToObject(item, "action", rule->action);
        cJSON_AddBoolToObject(item, "active", rule->active);
        cJSON_AddNumberToObject(item, "created_at", (double)rule->created_at);

        if (strlen(rule->raw_cmd) > 0) {
            cJSON_AddStringToObject(item, "raw_command", rule->raw_cmd);
        }

        if (rule->params_json) {
            cJSON *params = cJSON_Parse(rule->params_json);
            if (params) cJSON_AddItemToObject(item, "params", params);
        }

        cJSON_AddItemToArray(rules_arr, item);
    }

    cJSON_AddNumberToObject(data, "total", cJSON_GetArraySize(rules_arr));
    cJSON_AddItemToObject(data, "rules", rules_arr);
    cJSON_AddItemToObject(root, "status", "success");
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_Print(root);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

/**
 * @brief 添加访问控制规则
 */
int acl_add_rule(cJSON *rule_json, char *out_rule_id, int id_buf_size) {
    if (g_acl_rules_count >= MAX_QOS_RULES) {
        WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Max rules (%d) reached", MAX_QOS_RULES);
        return -1;
    }

    QosRuleEntry *rule = &g_acl_rules[g_acl_rules_count];
    memset(rule, 0, sizeof(QosRuleEntry));

    // 生成 rule_id
    snprintf(rule->rule_id, sizeof(rule->rule_id), "acl_%03d", g_acl_next_rule_index++);

    // 获取 name
    cJSON *j_name = cJSON_GetObjectItemCaseSensitive(rule_json, "name");
    if (j_name && cJSON_IsString(j_name)) {
        strncpy(rule->name, j_name->valuestring, QOS_RULE_NAME_LEN - 1);
    }

    // 获取 action（默认 add）
    strncpy(rule->action, "add", sizeof(rule->action) - 1);

    // 构建 iptables 命令
    cJSON *j_params = cJSON_GetObjectItemCaseSensitive(rule_json, "params");
    if (!j_params || !cJSON_IsObject(j_params)) return -1;

    // 获取安全策略（放行或阻断）
    cJSON *j_policy = cJSON_GetObjectItemCaseSensitive(j_params, "policy");
    const char *policy = "DROP"; // 默认阻断
    if (j_policy && cJSON_IsString(j_policy)) {
        if (strcmp(j_policy->valuestring, "ACCEPT") == 0 || strcmp(j_policy->valuestring, "allow") == 0) {
            policy = "ACCEPT";
        } else {
            policy = "DROP";
        }
    }

    // 构建命令
    char cmd[QOS_MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "iptables -A FORWARD");

    // 源 IP
    cJSON *j_src = cJSON_GetObjectItemCaseSensitive(j_params, "src_ip");
    if (j_src && cJSON_IsString(j_src)) {
        strncat(cmd, " -s ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_src->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 目的 IP
    cJSON *j_dst = cJSON_GetObjectItemCaseSensitive(j_params, "dst_ip");
    if (j_dst && cJSON_IsString(j_dst)) {
        strncat(cmd, " -d ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_dst->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 协议
    cJSON *j_proto = cJSON_GetObjectItemCaseSensitive(j_params, "protocol");
    if (j_proto && cJSON_IsString(j_proto)) {
        strncat(cmd, " -p ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_proto->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 源端口
    cJSON *j_sport = cJSON_GetObjectItemCaseSensitive(j_params, "src_port");
    if (j_sport && cJSON_IsString(j_sport)) {
        strncat(cmd, " --sport ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_sport->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 目的端口
    cJSON *j_dport = cJSON_GetObjectItemCaseSensitive(j_params, "dst_port");
    if (j_dport && cJSON_IsString(j_dport)) {
        strncat(cmd, " --dport ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_dport->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 源 MAC
    cJSON *j_src_mac = cJSON_GetObjectItemCaseSensitive(j_params, "src_mac");
    if (j_src_mac && cJSON_IsString(j_src_mac)) {
        strncat(cmd, " -m mac --mac-source ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_src_mac->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 目的 MAC（iptables 无直接匹配目的 MAC 的标准模块）
    cJSON *j_dst_mac = cJSON_GetObjectItemCaseSensitive(j_params, "dst_mac");
    if (j_dst_mac && cJSON_IsString(j_dst_mac)) {
        // 注意：iptables 没有标准的目的 MAC 匹配模块，这里用 -m mac --mac-source 替代
        strncat(cmd, " -m mac --mac-source ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, j_dst_mac->valuestring, sizeof(cmd) - strlen(cmd) - 1);
    }

    // 安全策略
    strncat(cmd, " -j ", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, policy, sizeof(cmd) - strlen(cmd) - 1);

    // 执行命令并记录
    int ret = ex_command(cmd);
    if (ret == 0) {
        strncpy(rule->raw_cmd, cmd, sizeof(rule->raw_cmd) - 1);
        rule->params_json = cJSON_PrintUnformatted(rule_json);
        rule->created_at = time(NULL);
        rule->active = true;
        g_acl_rules_count++;

        if (out_rule_id) strncpy(out_rule_id, rule->rule_id, id_buf_size - 1);
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Added rule %s: %s", rule->rule_id, cmd);
    }

    return ret;
}

/**
 * @brief 删除访问控制规则
 */
int acl_delete_rule(const char *rule_id) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strcmp(g_acl_rules[i].rule_id, rule_id) == 0) {
            QosRuleEntry *rule = &g_acl_rules[i];
            // 从 iptables 中删除
            if (strlen(rule->raw_cmd) > 0) {
                char del_cmd[QOS_MAX_CMD_LEN];
                strncpy(del_cmd, rule->raw_cmd, sizeof(del_cmd) - 1);
                char *pos = strstr(del_cmd, "iptables -A");
                if (pos) memcpy(pos, "iptables -D", 13);
                ex_command(del_cmd);
            }
            if (rule->params_json) {
                free(rule->params_json);
                rule->params_json = NULL;
            }
            // 移动后面的规则
            for (int j = i; j < g_acl_rules_count - 1; j++) {
                g_acl_rules[j] = g_acl_rules[j + 1];
            }
            memset(&g_acl_rules[g_acl_rules_count - 1], 0, sizeof(QosRuleEntry));
            g_acl_rules_count--;
            WTSL_LOG_INFO(MODULE_NAME, "[ACL] Deleted rule %s", rule_id);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 获取单条访问控制规则
 */
int acl_get_rule_json(const char *rule_id, char *out_buf, int buf_size) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strcmp(g_acl_rules[i].rule_id, rule_id) == 0) {
            QosRuleEntry *rule = &g_acl_rules[i];
            cJSON *root = cJSON_CreateObject();
            cJSON *data = cJSON_CreateObject();

            cJSON_AddStringToObject(data, "rule_id", rule->rule_id);
            cJSON_AddStringToObject(data, "name", rule->name);
            cJSON_AddStringToObject(data, "action", rule->action);
            cJSON_AddBoolToObject(data, "active", rule->active);
            cJSON_AddNumberToObject(data, "created_at", (double)rule->created_at);
            if (strlen(rule->raw_cmd) > 0) {
                cJSON_AddStringToObject(data, "raw_command", rule->raw_cmd);
            }
            if (rule->params_json) {
                cJSON *params = cJSON_Parse(rule->params_json);
                if (params) cJSON_AddItemToObject(data, "params", params);
            }

            cJSON_AddItemToObject(root, "status", "success");
            cJSON_AddItemToObject(root, "data", data);

            char *json_str = cJSON_Print(root);
            if (!json_str) { cJSON_Delete(root); return -1; }
            strncpy(out_buf, json_str, buf_size - 1);
            out_buf[buf_size - 1] = '\0';
            free(json_str);
            cJSON_Delete(root);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 清除所有访问控制规则
 */
int acl_clear_all_rules(void) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strlen(g_acl_rules[i].raw_cmd) > 0) {
            char del_cmd[QOS_MAX_CMD_LEN];
            strncpy(del_cmd, g_acl_rules[i].raw_cmd, sizeof(del_cmd) - 1);
            char *pos = strstr(del_cmd, "iptables -A");
            if (pos) memcpy(pos, "iptables -D", 13);
            ex_command(del_cmd);
        }
        if (g_acl_rules[i].params_json) {
            free(g_acl_rules[i].params_json);
            g_acl_rules[i].params_json = NULL;
        }
    }
    g_acl_rules_count = 0;
    g_acl_next_rule_index = 1;
    memset(g_acl_rules, 0, sizeof(g_acl_rules));
    g_acl_enabled = false;
    WTSL_LOG_INFO(MODULE_NAME, "[ACL] All ACL rules cleared");
    return 0;
}
