#include "wtsl_core_slb_qos_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "wtsl_log_manager.h"

#define MODULE_NAME "qos_core"

// 全局状态
QosGlobalState g_state;

// 【修复 P1】线程安全：互斥锁
static pthread_mutex_t qos_mutex = PTHREAD_MUTEX_INITIALIZER;

// 【修复 P0】ACL 默认阻断
static bool g_acl_enabled = false;
static QosRuleEntry g_acl_rules[MAX_QOS_RULES];
static int g_acl_rules_count = 0;
static int g_acl_next_rule_index = 1;
static char g_acl_default_drop[QOS_MAX_CMD_LEN] = {0};

void qos_init_state(const char *default_dev) {
    pthread_mutex_lock(&qos_mutex);
    g_state.enabled = true;
    g_state.snapshot_count = 0;
    g_state.rules_count = 0;
    g_state.next_rule_index = 1;
    memset(g_state.snapshot_rules, 0, sizeof(g_state.snapshot_rules));
    memset(g_state.rules, 0, sizeof(g_state.rules));
    strncpy(g_state.default_device, default_dev, sizeof(g_state.default_device) - 1);
    g_state.default_device[sizeof(g_state.default_device) - 1] = '\0';
    g_acl_enabled = false;
    g_acl_rules_count = 0;
    g_acl_next_rule_index = 1;
    memset(g_acl_rules, 0, sizeof(g_acl_rules));
    memset(g_acl_default_drop, 0, sizeof(g_acl_default_drop));
    pthread_mutex_unlock(&qos_mutex);
    WTSL_LOG_INFO(MODULE_NAME, "[QoS] Engine Initialized. Default Device: %s", g_state.default_device);
}

static int ex_command(const char *cmd) {
    WTSL_LOG_DEBUG(MODULE_NAME, "[EXEC] %s", cmd);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) { WTSL_LOG_ERROR(MODULE_NAME, "popen failed"); return -1; }
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(cmd, "show") || strstr(cmd, "-L"))
            WTSL_LOG_DEBUG(MODULE_NAME, "[OUT] %s", buffer);
    }
    int status = pclose(fp);
    if (WEXITSTATUS(status) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[Error] exit code %d", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

static void record_rule(const char *cmd) {
    pthread_mutex_lock(&qos_mutex);
    if (g_state.snapshot_count >= MAX_SNAPSHOT_RULES) { pthread_mutex_unlock(&qos_mutex); return; }
    for (int i = 0; i < g_state.snapshot_count; i++) {
        if (strcmp(g_state.snapshot_rules[i], cmd) == 0) { pthread_mutex_unlock(&qos_mutex); return; }
    }
    strncpy(g_state.snapshot_rules[g_state.snapshot_count], cmd, QOS_MAX_CMD_LEN - 1);
    g_state.snapshot_count++;
    pthread_mutex_unlock(&qos_mutex);
    WTSL_LOG_DEBUG(MODULE_NAME, "[SNAPSHOT] Recorded #%d", g_state.snapshot_count);
}

bool qos_is_safe_param(const char *param) {
    if (!param) return false;
    const char *dangerous[] = {";", "|", "&&", "||", "$(", "`", ">", "<", "\n", "\r", "&", "(", ")", "{", "}", "!", "~"};
    for (int i = 0; i < (int)(sizeof(dangerous)/sizeof(dangerous[0])); i++) {
        if (strstr(param, dangerous[i]) != NULL) return false;
    }
    return true;
}

// 【修复 P2】输入校验
static bool qos_is_valid_ipv4(const char *ip) {
    if (!ip || strlen(ip) < 7) return false;
    int a, b, c, d;
    return sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 && a>=0&&a<=255 && b>=0&&b<=255 && c>=0&&c<=255 && d>=0&&d<=255;
}
static bool qos_is_valid_mac(const char *mac) {
    if (!mac || strlen(mac) != 17) return false;
    unsigned int a[6];
    return sscanf(mac, "%x:%x:%x:%x:%x:%x", &a[0],&a[1],&a[2],&a[3],&a[4],&a[5]) == 6;
}
static bool qos_is_valid_port_str(const char *port) {
    if (!port) return false;
    int p = atoi(port);
    return p > 0 && p <= 65535;
}

void qos_gen_rule_id(char *out_id, int len) {
    pthread_mutex_lock(&qos_mutex);
    snprintf(out_id, len, "rule_%03d", g_state.next_rule_index++);
    pthread_mutex_unlock(&qos_mutex);
}

int qos_add_rule_entry(QosRuleEntry *entry) {
    pthread_mutex_lock(&qos_mutex);
    if (g_state.rules_count >= MAX_QOS_RULES) { pthread_mutex_unlock(&qos_mutex); return -1; }
    QosRuleEntry *rule = &g_state.rules[g_state.rules_count];
    memcpy(rule, entry, sizeof(QosRuleEntry));
    rule->created_at = time(NULL);
    rule->active = true;
    int idx = g_state.rules_count++;
    WTSL_LOG_INFO(MODULE_NAME, "[RULE] Added %s: %s", rule->rule_id, rule->name);
    pthread_mutex_unlock(&qos_mutex);
    return idx;
}

QosRuleEntry *qos_find_rule(const char *rule_id) {
    pthread_mutex_lock(&qos_mutex);
    for (int i = 0; i < g_state.rules_count; i++) {
        if (strcmp(g_state.rules[i].rule_id, rule_id) == 0) { pthread_mutex_unlock(&qos_mutex); return &g_state.rules[i]; }
    }
    pthread_mutex_unlock(&qos_mutex);
    return NULL;
}

int qos_delete_rule(const char *rule_id) {
    pthread_mutex_lock(&qos_mutex);
    for (int i = 0; i < g_state.rules_count; i++) {
        if (strcmp(g_state.rules[i].rule_id, rule_id) == 0) {
            QosRuleEntry *rule = &g_state.rules[i];
            if (strlen(rule->raw_cmd) > 0) {
                char del_cmd[QOS_MAX_CMD_LEN];
                if (rule->type == QOS_TYPE_TC) {
                    snprintf(del_cmd, sizeof(del_cmd), "tc %s del dev %s", rule->obj_type, g_state.default_device);
                    if (rule->params_json) {
                        cJSON *p = cJSON_Parse(rule->params_json);
                        if (p) {
                            cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(p, "handle");
                            cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(p, "parent");
                            if (j_handle) { strncat(del_cmd, " handle ", sizeof(del_cmd)-strlen(del_cmd)-1); strncat(del_cmd, j_handle->valuestring, sizeof(del_cmd)-strlen(del_cmd)-1); }
                            if (j_parent) { strncat(del_cmd, " parent ", sizeof(del_cmd)-strlen(del_cmd)-1); strncat(del_cmd, j_parent->valuestring, sizeof(del_cmd)-strlen(del_cmd)-1); }
                            cJSON_Delete(p);
                        }
                    }
                    ex_command(del_cmd);
                } else if (rule->type == QOS_TYPE_IPTABLES) {
                    strncpy(del_cmd, rule->raw_cmd, sizeof(del_cmd) - 1);
                    char *pos;
                    if ((pos = strstr(del_cmd, "iptables -A"))) memcpy(pos, "iptables -D", 13);
                    else if ((pos = strstr(del_cmd, "iptables -I"))) memcpy(pos, "iptables -D", 13);
                    ex_command(del_cmd);
                }
            }
            if (rule->params_json) { free(rule->params_json); rule->params_json = NULL; }
            for (int j = i; j < g_state.rules_count - 1; j++) g_state.rules[j] = g_state.rules[j + 1];
            memset(&g_state.rules[g_state.rules_count - 1], 0, sizeof(QosRuleEntry));
            g_state.rules_count--;
            pthread_mutex_unlock(&qos_mutex);
            WTSL_LOG_INFO(MODULE_NAME, "[RULE] Deleted %s", rule_id);
            return 0;
        }
    }
    pthread_mutex_unlock(&qos_mutex);
    return -1;
}

int qos_clear_all_rules(void) {
    pthread_mutex_lock(&qos_mutex);
    for (int i = 0; i < g_state.rules_count; i++) {
        if (g_state.rules[i].params_json) { free(g_state.rules[i].params_json); }
    }
    g_state.rules_count = 0;
    g_state.next_rule_index = 1;
    memset(g_state.rules, 0, sizeof(g_state.rules));
    pthread_mutex_unlock(&qos_mutex);
    WTSL_LOG_INFO(MODULE_NAME, "[RULE] All rules cleared");
    return 0;
}

int qos_list_rules_json(char *out_buf, int buf_size, const char *filter_type) {
    pthread_mutex_lock(&qos_mutex);
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *rules_arr = cJSON_CreateArray();
    for (int i = 0; i < g_state.rules_count; i++) {
        QosRuleEntry *rule = &g_state.rules[i];
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
        if (strlen(rule->raw_cmd) > 0) cJSON_AddStringToObject(item, "raw_command", rule->raw_cmd);
        if (rule->params_json) { cJSON *params = cJSON_Parse(rule->params_json); if (params) cJSON_AddItemToObject(item, "params", params); }
        cJSON_AddItemToArray(rules_arr, item);
    }
    int total = cJSON_GetArraySize(rules_arr);
    cJSON_AddNumberToObject(data, "total", total);
    cJSON_AddItemToObject(data, "rules", rules_arr);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);
    char *json_str = cJSON_Print(root);
    pthread_mutex_unlock(&qos_mutex);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

int qos_get_rule_json(const char *rule_id, char *out_buf, int buf_size) {
    pthread_mutex_lock(&qos_mutex);
    QosRuleEntry *rule = NULL;
    for (int i = 0; i < g_state.rules_count; i++) {
        if (strcmp(g_state.rules[i].rule_id, rule_id) == 0) { rule = &g_state.rules[i]; break; }
    }
    if (!rule) { pthread_mutex_unlock(&qos_mutex); return -1; }
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
    if (strlen(rule->raw_cmd) > 0) cJSON_AddStringToObject(data, "raw_command", rule->raw_cmd);
    if (rule->params_json) { cJSON *params = cJSON_Parse(rule->params_json); if (params) cJSON_AddItemToObject(data, "params", params); }
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);
    char *json_str = cJSON_Print(root);
    pthread_mutex_unlock(&qos_mutex);
    if (!json_str) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(json_str);
    cJSON_Delete(root);
    return 0;
}

int tc_show_all(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *qdiscs = cJSON_CreateArray(), *classes = cJSON_CreateArray(), *filters = cJSON_CreateArray();
    const char *dev = g_state.default_device;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc show dev %s 2>/dev/null", dev);
    FILE *fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(qdiscs, cJSON_CreateString(buf)); } pclose(fp); }
    snprintf(cmd, sizeof(cmd), "tc class show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(classes, cJSON_CreateString(buf)); } pclose(fp); }
    snprintf(cmd, sizeof(cmd), "tc filter show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(filters, cJSON_CreateString(buf)); } pclose(fp); }
    cJSON_AddItemToObject(data, "qdiscs", qdiscs);
    cJSON_AddItemToObject(data, "classes", classes);
    cJSON_AddItemToObject(data, "filters", filters);
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

int tc_show_stats(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *qdiscs = cJSON_CreateArray(), *classes = cJSON_CreateArray(), *filters = cJSON_CreateArray();
    const char *dev = g_state.default_device;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc -s qdisc show dev %s 2>/dev/null", dev);
    FILE *fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(qdiscs, cJSON_CreateString(buf)); } pclose(fp); }
    snprintf(cmd, sizeof(cmd), "tc -s class show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(classes, cJSON_CreateString(buf)); } pclose(fp); }
    snprintf(cmd, sizeof(cmd), "tc -s filter show dev %s 2>/dev/null", dev);
    fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(filters, cJSON_CreateString(buf)); } pclose(fp); }
    cJSON *ipt = cJSON_CreateArray();
    snprintf(cmd, sizeof(cmd), "iptables -L FORWARD -v -n -x 2>/dev/null");
    fp = popen(cmd, "r");
    if (fp) { char buf[512]; while (fgets(buf, sizeof(buf), fp)) { buf[strcspn(buf, "\n")] = 0; if (strlen(buf) > 0) cJSON_AddItemToArray(ipt, cJSON_CreateString(buf)); } pclose(fp); }
    cJSON_AddItemToObject(data, "qdiscs", qdiscs);
    cJSON_AddItemToObject(data, "classes", classes);
    cJSON_AddItemToObject(data, "filters", filters);
    cJSON_AddItemToObject(data, "iptables_stats", ipt);
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

/**
 * 【修复 P0】tc filter 使用 fw 过滤器匹配 iptables MARK 值
 * 正确语法: tc filter add dev br0 parent 1:0 protocol ip prio 10 handle 0x10 fw flowid 1:10
 */
int tc_handle_request(const char *action, const char *obj_type, cJSON *params) {
    char cmd[QOS_MAX_CMD_LEN];
    cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(params, "device");
    const char *dev = (j_dev && cJSON_IsString(j_dev)) ? j_dev->valuestring : g_state.default_device;
    if (!qos_is_safe_param(dev)) { WTSL_LOG_ERROR(MODULE_NAME, "[TC] Unsafe device: %s", dev); return -1; }

    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) {
        snprintf(cmd, sizeof(cmd), "tc %s del dev %s", obj_type, dev);
        cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(params, "parent");
        cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(params, "handle");
        if (j_parent && cJSON_IsString(j_parent)) { strncat(cmd, " parent ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_parent->valuestring, sizeof(cmd)-strlen(cmd)-1); }
        if (j_handle && cJSON_IsString(j_handle)) { strncat(cmd, " handle ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_handle->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    } else if (strcmp(action, "show") == 0) {
        snprintf(cmd, sizeof(cmd), "tc %s show dev %s", obj_type, dev);
    } else {
        snprintf(cmd, sizeof(cmd), "tc %s %s dev %s", action, obj_type, dev);
        cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(params, "parent");
        cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(params, "handle");
        cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(params, "kind");
        cJSON *j_flowid = cJSON_GetObjectItemCaseSensitive(params, "flowid");
        cJSON *j_args = cJSON_GetObjectItemCaseSensitive(params, "args");

        if (j_parent && cJSON_IsString(j_parent) && qos_is_safe_param(j_parent->valuestring)) { strncat(cmd, " parent ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_parent->valuestring, sizeof(cmd)-strlen(cmd)-1); }
        if (j_handle && cJSON_IsString(j_handle) && qos_is_safe_param(j_handle->valuestring)) { strncat(cmd, " handle ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_handle->valuestring, sizeof(cmd)-strlen(cmd)-1); }
        if (j_kind && cJSON_IsString(j_kind) && qos_is_safe_param(j_kind->valuestring)) { strncat(cmd, " ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_kind->valuestring, sizeof(cmd)-strlen(cmd)-1); }

        if (j_args && cJSON_IsObject(j_args)) {
            cJSON *item;
            cJSON_ArrayForEach(item, j_args) {
                const char *val = NULL; static char num_buf[32];
                if (cJSON_IsString(item)) { if (!qos_is_safe_param(item->valuestring)) return -1; val = item->valuestring; }
                else if (cJSON_IsNumber(item)) { snprintf(num_buf, sizeof(num_buf), "%g", item->valuedouble); val = num_buf; }
                if (val) { strncat(cmd, " ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, item->string, sizeof(cmd)-strlen(cmd)-1); strncat(cmd, " ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, val, sizeof(cmd)-strlen(cmd)-1); }
            }
        }

        // 【修复 P0】Filter: handle→fw 过滤器匹配 MARK, dsfield→匹配 DSCP
        if (strcmp(obj_type, "filter") == 0) {
            cJSON *j_mh = cJSON_GetObjectItemCaseSensitive(params, "match_handle");
            cJSON *j_ds = cJSON_GetObjectItemCaseSensitive(params, "dsfield");
            if (j_mh && cJSON_IsString(j_mh) && qos_is_safe_param(j_mh->valuestring)) {
                char new_cmd[QOS_MAX_CMD_LEN];
                snprintf(new_cmd, sizeof(new_cmd), "tc filter add dev %s parent 1:0 protocol ip prio 10 handle %s fw", dev, j_mh->valuestring);
                if (j_flowid && cJSON_IsString(j_flowid) && qos_is_safe_param(j_flowid->valuestring)) {
                    strncat(new_cmd, " flowid ", sizeof(new_cmd)-strlen(new_cmd)-1);
                    strncat(new_cmd, j_flowid->valuestring, sizeof(new_cmd)-strlen(new_cmd)-1);
                }
                strncpy(cmd, new_cmd, sizeof(cmd) - 1);
                cmd[sizeof(cmd) - 1] = '\0';
            } else if (j_ds && cJSON_IsString(j_ds) && qos_is_safe_param(j_ds->valuestring)) {
                strncat(cmd, " dsfield ", sizeof(cmd)-strlen(cmd)-1);
                strncat(cmd, j_ds->valuestring, sizeof(cmd)-strlen(cmd)-1);
            }
            if (j_flowid && cJSON_IsString(j_flowid) && qos_is_safe_param(j_flowid->valuestring) && !j_mh) {
                strncat(cmd, " flowid ", sizeof(cmd)-strlen(cmd)-1);
                strncat(cmd, j_flowid->valuestring, sizeof(cmd)-strlen(cmd)-1);
            }
        }
    }

    int ret = ex_command(cmd);
    if (ret == 0 && (strcmp(action, "add") == 0 || strcmp(action, "replace") == 0)) record_rule(cmd);
    return ret;
}

/**
 * 【修复 P0】支持 insert_num 插入到指定行号，确保 ACCEPT 在 DROP 之前
 */
int iptables_handle_request(const char *action, const char *chain, cJSON *params) {
    char cmd[QOS_MAX_CMD_LEN];
    if (!qos_is_safe_param(chain)) { WTSL_LOG_ERROR(MODULE_NAME, "[IPT] Unsafe chain: %s", chain); return -1; }

    cJSON *j_insert_num = cJSON_GetObjectItemCaseSensitive(params, "insert_num");
    int insert_num = (j_insert_num && cJSON_IsNumber(j_insert_num)) ? (int)j_insert_num->valuedouble : 0;
    const char *flag = "-A";
    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) flag = "-D";
    else if (strcmp(action, "insert") == 0) flag = "-I";
    else if (strcmp(action, "replace") == 0) flag = "-R";

    if (insert_num > 0) snprintf(cmd, sizeof(cmd), "iptables %s %s %d", flag, chain, insert_num);
    else snprintf(cmd, sizeof(cmd), "iptables %s %s", flag, chain);

    cJSON *j_proto = cJSON_GetObjectItemCaseSensitive(params, "protocol");
    if (j_proto && cJSON_IsString(j_proto) && qos_is_safe_param(j_proto->valuestring)) { strncat(cmd, " -p ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_proto->valuestring, sizeof(cmd)-strlen(cmd)-1); }

    cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(params, "device");
    const char *dev = (j_dev && cJSON_IsString(j_dev)) ? j_dev->valuestring : g_state.default_device;
    cJSON *j_in = cJSON_GetObjectItemCaseSensitive(params, "in_interface");
    cJSON *j_out = cJSON_GetObjectItemCaseSensitive(params, "out_interface");

    if (!j_in && !j_out) {
        if (strcmp(chain, "INPUT") == 0 || strcmp(chain, "FORWARD") == 0) { strncat(cmd, " -i ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, dev, sizeof(cmd)-strlen(cmd)-1); }
        else if (strcmp(chain, "OUTPUT") == 0) { strncat(cmd, " -o ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, dev, sizeof(cmd)-strlen(cmd)-1); }
    } else {
        if (j_in && cJSON_IsString(j_in) && qos_is_safe_param(j_in->valuestring)) { strncat(cmd, " -i ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_in->valuestring, sizeof(cmd)-strlen(cmd)-1); }
        if (j_out && cJSON_IsString(j_out) && qos_is_safe_param(j_out->valuestring)) { strncat(cmd, " -o ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_out->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    }

    cJSON *j_args = cJSON_GetObjectItemCaseSensitive(params, "args");
    if (j_args && cJSON_IsObject(j_args)) {
        cJSON *item;
        cJSON_ArrayForEach(item, j_args) {
            const char *key = item->string, *val = NULL; static char num_buf[32];
            if (cJSON_IsString(item)) { if (!qos_is_safe_param(item->valuestring)) return -1; val = item->valuestring; }
            else if (cJSON_IsNumber(item)) { snprintf(num_buf, sizeof(num_buf), "%d", (int)item->valuedouble); val = num_buf; }
            if (!val) continue;
            strncat(cmd, " ", sizeof(cmd)-strlen(cmd)-1);
            if (strcmp(key,"source")==0||strcmp(key,"src")==0) strncat(cmd, "-s ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"destination")==0||strcmp(key,"dst")==0) strncat(cmd, "-d ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"sport")==0) strncat(cmd, "--sport ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"dport")==0) strncat(cmd, "--dport ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"jump")==0||strcmp(key,"target")==0) strncat(cmd, "-j ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"set_dscp")==0||strcmp(key,"dscp_class")==0) strncat(cmd, "-j DSCP --set-dscp-class ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"set_mark")==0||strcmp(key,"mark")==0) strncat(cmd, "-j MARK --set-mark ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"match_dscp")==0) strncat(cmd, "-m dscp --dscp ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"match_mark")==0) strncat(cmd, "-m mark --mark ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"src_mac")==0||strcmp(key,"source_mac")==0) strncat(cmd, "-m mac --mac-source ", sizeof(cmd)-strlen(cmd)-1);
            else if (strcmp(key,"dst_mac")==0||strcmp(key,"dest_mac")==0) strncat(cmd, "-m mac --mac-source ", sizeof(cmd)-strlen(cmd)-1);
            else { strncat(cmd, "--", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, key, sizeof(cmd)-strlen(cmd)-1); strncat(cmd, " ", sizeof(cmd)-strlen(cmd)-1); }
            strncat(cmd, val, sizeof(cmd)-strlen(cmd)-1);
        }
    }

    int ret = ex_command(cmd);
    if (ret == 0 && (strcmp(action,"add")==0||strcmp(action,"insert")==0||strcmp(action,"replace")==0)) record_rule(cmd);
    return ret;
}

int qos_toggle_switch(bool enable) {
    pthread_mutex_lock(&qos_mutex);
    if (g_state.enabled == enable) { pthread_mutex_unlock(&qos_mutex); return 0; }
    pthread_mutex_unlock(&qos_mutex);

    if (!enable) {
        WTSL_LOG_INFO(MODULE_NAME, "[SWITCH] Disabling QoS...");
        char cmd[QOS_MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root 2>/dev/null", g_state.default_device); system(cmd);
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s ingress 2>/dev/null", g_state.default_device); system(cmd);
        for (int i = 0; i < g_state.snapshot_count; i++) {
            char *rule = g_state.snapshot_rules[i];
            if (strstr(rule, "iptables") != NULL) {
                char del_cmd[QOS_MAX_CMD_LEN];
                strcpy(del_cmd, rule);
                if (strstr(del_cmd, "iptables -A")) memcpy(strstr(del_cmd, "iptables -A"), "iptables -D", 13);
                else if (strstr(del_cmd, "iptables -I")) memcpy(strstr(del_cmd, "iptables -I"), "iptables -D", 13);
                else if (strstr(del_cmd, "iptables -R")) continue;
                system(del_cmd);
            }
        }
        pthread_mutex_lock(&qos_mutex);
        g_state.enabled = false;
        pthread_mutex_unlock(&qos_mutex);
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "[SWITCH] Enabling QoS...");
        for (int i = 0; i < g_state.snapshot_count; i++) system(g_state.snapshot_rules[i]);
        pthread_mutex_lock(&qos_mutex);
        g_state.enabled = true;
        pthread_mutex_unlock(&qos_mutex);
    }
    return 0;
}

// ==========================================
// 场景 API
// ==========================================

int qos_scene_list_json(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject(), *data = cJSON_CreateObject(), *scenes = cJSON_CreateArray();
    const char *scenes_data[][5] = {
        {"bandwidth_limit","Bandwidth Limit","Limit bandwidth for a specific port","",""},
        {"traffic_block","Traffic Block","Block traffic on a specific port","",""},
        {"traffic_allow","Traffic Allow","Allow traffic on a port with high priority","",""},
        {"device_limit","Device Rate Limit","Limit bandwidth for a device by IP","",""},
        {"gaming_boost","Gaming Boost","Set highest priority for gaming ports","default_ports","8080, 27015-27030, 4380"},
        {"video_smooth","Video Smooth","Guaranteed bandwidth for video streaming ports","default_ports","80, 443, 1935, 5000-5100"},
        {"iot_qos","IoT Device QoS","Guaranteed minimum bandwidth for IoT devices","",""},
        {"web_browse","Web Browse Guarantee","Guaranteed bandwidth for HTTP and HTTPS","",""},
    };
    for (int i = 0; i < 8; i++) {
        cJSON *s = cJSON_CreateObject();
        cJSON_AddStringToObject(s, "scene", scenes_data[i][0]);
        cJSON_AddStringToObject(s, "title", scenes_data[i][1]);
        cJSON_AddStringToObject(s, "description", scenes_data[i][2]);
        cJSON *req = cJSON_CreateObject();
        if (scenes_data[i][3][0]) cJSON_AddStringToObject(s, scenes_data[i][3], scenes_data[i][4]);
        cJSON_AddItemToObject(s, "request_example", req);
        cJSON_AddItemToArray(scenes, s);
    }
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

/**
 * 【修复 P2】使用 g_state.default_device 而非硬编码 br0
 */
static int ensure_htb_root(void) {
    const char *dev = g_state.default_device;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc show dev %s 2>/dev/null | grep -q 'root.*htb'", dev);
    if (system(cmd) != 0) {
        snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root handle 1: htb default 99", dev);
        return system(cmd);
    }
    return 0;
}

int qos_scene_bandwidth_limit(int class_id, int port, const char *protocol, const char *rate, const char *ceil) {
    if (!qos_is_safe_param(rate) || !qos_is_safe_param(ceil)) return -1;
    ensure_htb_root();
    char cmd[QOS_MAX_CMD_LEN];
    const char *dev = g_state.default_device;
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s", dev, class_id, rate, ceil);
    int ret = ex_command(cmd);
    if (ret != 0) return ret;
    record_rule(cmd);
    const char *proto_list[] = {"tcp", "udp", NULL};
    if (!protocol || strcmp(protocol, "both") == 0) {
        for (int i = 0; proto_list[i]; i++) {
            snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d", dev, proto_list[i], port, class_id);
            ex_command(cmd); record_rule(cmd);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d", dev, protocol, port, class_id);
        ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    }
    WTSL_LOG_INFO(MODULE_NAME, "[SCENE] bandwidth_limit class 1:%d", class_id);
    return 0;
}

int qos_scene_traffic_block(int port, const char *protocol) {
    char cmd[QOS_MAX_CMD_LEN];
    const char *pl[] = {"tcp", "udp", NULL};
    int ret = 0;
    if (!protocol || strcmp(protocol, "both") == 0) {
        for (int i = 0; pl[i]; i++) {
            snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s --dport %d -j DROP", pl[i], port);
            ret = ex_command(cmd); if (ret == 0) record_rule(cmd);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s --dport %d -j DROP", protocol, port);
        ret = ex_command(cmd); if (ret == 0) record_rule(cmd);
    }
    return ret;
}

int qos_scene_traffic_allow(int port, const char *protocol) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    const char *pl[] = {"tcp", "udp", NULL};
    if (!protocol || strcmp(protocol, "both") == 0) {
        for (int i = 0; pl[i]; i++) { snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s -i %s --dport %d -j ACCEPT", pl[i], dev, port); ex_command(cmd); record_rule(cmd); }
    } else {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p %s -i %s --dport %d -j ACCEPT", protocol, dev, port);
        ex_command(cmd); record_rule(cmd);
    }
    ensure_htb_root();
    int cid = port % 100 + 200;
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:%d htb rate 100mbit ceil 1000mbit prio 1", dev, cid);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    const char *p = protocol ? protocol : "tcp";
    snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip %s dport %d 0xffff flowid 1:%d", dev, p, port, cid);
    ex_command(cmd); record_rule(cmd);
    return 0;
}

int qos_scene_device_limit(const char *ip, const char *rate, const char *ceil) {
    if (!qos_is_safe_param(ip) || !qos_is_safe_param(rate) || !qos_is_safe_param(ceil)) return -1;
    if (!qos_is_valid_ipv4(ip)) { WTSL_LOG_ERROR(MODULE_NAME, "[SCENE] Invalid IP: %s", ip); return -1; }
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    ensure_htb_root();
    int last_octet = 0;
    sscanf(ip, "%*d.%*d.%*d.%d", &last_octet);
    int cid = last_octet + 50;
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s", dev, cid, rate, ceil);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip src %s flowid 1:%d", dev, ip, cid);
    ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    return 0;
}

int qos_scene_gaming_boost(int port) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    ensure_htb_root();
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:201 htb rate 50mbit ceil 1000mbit prio 1", dev);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    int default_ports[] = {8080,27015,27016,27017,27018,27019,27020,4380,0};
    int *ports = default_ports;
    int cp[2] = {port, 0};
    if (port > 0) ports = cp;
    for (int i = 0; ports[i]; i++) {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p udp -i %s --dport %d -j ACCEPT", dev, ports[i]);
        ex_command(cmd); record_rule(cmd);
        snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip udp dport %d 0xffff flowid 1:201", dev, ports[i]);
        ex_command(cmd); record_rule(cmd);
    }
    return 0;
}

int qos_scene_video_smooth(void) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    ensure_htb_root();
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:202 htb rate 30mbit ceil 500mbit prio 2", dev);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    int vp[] = {80,443,1935,0};
    for (int i = 0; vp[i]; i++) {
        snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 2 u32 match ip tcp dport %d 0xffff flowid 1:202", dev, vp[i]);
        ex_command(cmd); record_rule(cmd);
    }
    for (int i = 0; vp[i]; i++) {
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -p tcp -i %s --dport %d -j ACCEPT", dev, vp[i]);
        ex_command(cmd); record_rule(cmd);
    }
    return 0;
}

int qos_scene_iot_qos(const char *ip, const char *rate) {
    if (!qos_is_safe_param(ip) || !qos_is_safe_param(rate)) return -1;
    if (!qos_is_valid_ipv4(ip)) return -1;
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    ensure_htb_root();
    int last_octet = 0;
    sscanf(ip, "%*d.%*d.%*d.%d", &last_octet);
    int cid = last_octet + 150;
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil 100mbit prio 3", dev, cid, rate);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 3 u32 match ip src %s flowid 1:%d", dev, ip, cid);
    ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    return 0;
}

int qos_scene_web_browse(void) {
    const char *dev = g_state.default_device;
    char cmd[QOS_MAX_CMD_LEN];
    ensure_htb_root();
    snprintf(cmd, sizeof(cmd), "tc class add dev %s parent 1:1 classid 1:203 htb rate 20mbit ceil 200mbit prio 2", dev);
    int ret = ex_command(cmd); if (ret != 0) return ret; record_rule(cmd);
    int wp[] = {80,443,0};
    for (int i = 0; wp[i]; i++) {
        snprintf(cmd, sizeof(cmd), "tc filter add dev %s parent 1:0 protocol ip prio 2 u32 match ip tcp dport %d 0xffff flowid 1:203", dev, wp[i]);
        ex_command(cmd); record_rule(cmd);
    }
    return 0;
}

// ==========================================
// 【修复 P0 + P1】ACL 访问控制（防火墙）
// ==========================================

static int acl_insert_default_drop(const char *dev) {
    snprintf(g_acl_default_drop, sizeof(g_acl_default_drop), "iptables -A FORWARD -i %s -j DROP", dev);
    return ex_command(g_acl_default_drop);
}
static int acl_remove_default_drop(void) {
    if (strlen(g_acl_default_drop) > 0) return ex_command(g_acl_default_drop);
    return 0;
}

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
 * 【修复 P0】ACL 开关：开启时插入默认 DROP，关闭时删除
 */
int acl_set_enabled(bool enable) {
    if (g_acl_enabled == enable) return 0;
    if (!enable) {
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Disabling, removing %d rules", g_acl_rules_count);
        acl_remove_default_drop();
        for (int i = 0; i < g_acl_rules_count; i++) {
            if (strlen(g_acl_rules[i].raw_cmd) > 0) {
                char del_cmd[QOS_MAX_CMD_LEN];
                strncpy(del_cmd, g_acl_rules[i].raw_cmd, sizeof(del_cmd) - 1);
                char *pos = strstr(del_cmd, "iptables -");
                if (pos) {
                    memcpy(pos, "iptables -D", 13);
                    char *num = strstr(del_cmd, " 1 ");
                    if (num) memmove(num, num + 2, strlen(num + 2) + 1);
                }
                ex_command(del_cmd);
            }
            if (g_acl_rules[i].params_json) { free(g_acl_rules[i].params_json); g_acl_rules[i].params_json = NULL; }
        }
        g_acl_rules_count = 0; g_acl_next_rule_index = 1;
        memset(g_acl_rules, 0, sizeof(g_acl_rules));
        memset(g_acl_default_drop, 0, sizeof(g_acl_default_drop));
        g_acl_enabled = false;
    } else {
        const char *dev = g_state.default_device;
        acl_insert_default_drop(dev);
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Enabling, applying %d rules", g_acl_rules_count);
        for (int i = 0; i < g_acl_rules_count; i++) {
            if (strlen(g_acl_rules[i].raw_cmd) > 0) ex_command(g_acl_rules[i].raw_cmd);
        }
        g_acl_enabled = true;
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Access control enabled with default DROP");
    }
    return 0;
}

int acl_list_rules_json(char *out_buf, int buf_size) {
    cJSON *root = cJSON_CreateObject(), *data = cJSON_CreateObject(), *arr = cJSON_CreateArray();
    for (int i = 0; i < g_acl_rules_count; i++) {
        QosRuleEntry *r = &g_acl_rules[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "rule_id", r->rule_id);
        cJSON_AddStringToObject(item, "name", r->name);
        cJSON_AddStringToObject(item, "action", r->action);
        cJSON_AddBoolToObject(item, "active", r->active);
        cJSON_AddNumberToObject(item, "created_at", (double)r->created_at);
        if (strlen(r->raw_cmd) > 0) cJSON_AddStringToObject(item, "raw_command", r->raw_cmd);
        if (r->params_json) { cJSON *p = cJSON_Parse(r->params_json); if (p) cJSON_AddItemToObject(item, "params", p); }
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddNumberToObject(data, "total", cJSON_GetArraySize(arr));
    cJSON_AddItemToObject(data, "rules", arr);
    cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
    cJSON_AddItemToObject(root, "data", data);
    char *js = cJSON_Print(root);
    if (!js) { cJSON_Delete(root); return -1; }
    strncpy(out_buf, js, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(js); cJSON_Delete(root);
    return 0;
}

/**
 * 【修复 P0】ACCEPT 规则用 -I 1 插入到最前面，DROP 用 -A 追加
 * 【修复 P2】输入校验 IP/MAC/Port
 */
int acl_add_rule(cJSON *rule_json, char *out_rule_id, int id_buf_size) {
    if (g_acl_rules_count >= MAX_QOS_RULES) return -1;
    QosRuleEntry *rule = &g_acl_rules[g_acl_rules_count];
    memset(rule, 0, sizeof(QosRuleEntry));
    snprintf(rule->rule_id, sizeof(rule->rule_id), "acl_%03d", g_acl_next_rule_index++);
    cJSON *j_name = cJSON_GetObjectItemCaseSensitive(rule_json, "name");
    if (j_name && cJSON_IsString(j_name)) strncpy(rule->name, j_name->valuestring, QOS_RULE_NAME_LEN - 1);
    strncpy(rule->action, "add", sizeof(rule->action) - 1);

    cJSON *j_params = cJSON_GetObjectItemCaseSensitive(rule_json, "params");
    if (!j_params || !cJSON_IsObject(j_params)) return -1;

    // 【修复 P2】输入校验
    cJSON *j_sip = cJSON_GetObjectItemCaseSensitive(j_params, "src_ip");
    if (j_sip && cJSON_IsString(j_sip) && !qos_is_valid_ipv4(j_sip->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid src_ip"); return -1; }
    cJSON *j_dip = cJSON_GetObjectItemCaseSensitive(j_params, "dst_ip");
    if (j_dip && cJSON_IsString(j_dip) && !qos_is_valid_ipv4(j_dip->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid dst_ip"); return -1; }
    cJSON *j_smac = cJSON_GetObjectItemCaseSensitive(j_params, "src_mac");
    if (j_smac && cJSON_IsString(j_smac) && !qos_is_valid_mac(j_smac->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid src_mac"); return -1; }
    cJSON *j_dmac = cJSON_GetObjectItemCaseSensitive(j_params, "dst_mac");
    if (j_dmac && cJSON_IsString(j_dmac) && !qos_is_valid_mac(j_dmac->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid dst_mac"); return -1; }
    cJSON *j_sport = cJSON_GetObjectItemCaseSensitive(j_params, "src_port");
    if (j_sport && cJSON_IsString(j_sport) && !qos_is_valid_port_str(j_sport->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid src_port"); return -1; }
    cJSON *j_dport = cJSON_GetObjectItemCaseSensitive(j_params, "dst_port");
    if (j_dport && cJSON_IsString(j_dport) && !qos_is_valid_port_str(j_dport->valuestring)) { WTSL_LOG_ERROR(MODULE_NAME, "[ACL] Invalid dst_port"); return -1; }

    cJSON *j_policy = cJSON_GetObjectItemCaseSensitive(j_params, "policy");
    const char *policy = "DROP";
    if (j_policy && cJSON_IsString(j_policy)) {
        if (strcmp(j_policy->valuestring, "ACCEPT") == 0 || strcmp(j_policy->valuestring, "allow") == 0) policy = "ACCEPT";
    }

    char cmd[QOS_MAX_CMD_LEN];
    // 【修复 P0】ACCEPT 用 -I 1 插入到链首（在 DROP 之前匹配）
    if (strcmp(policy, "ACCEPT") == 0) snprintf(cmd, sizeof(cmd), "iptables -I FORWARD 1");
    else snprintf(cmd, sizeof(cmd), "iptables -A FORWARD");

    if (j_sip && cJSON_IsString(j_sip)) { strncat(cmd, " -s ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_sip->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    if (j_dip && cJSON_IsString(j_dip)) { strncat(cmd, " -d ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_dip->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    cJSON *j_proto = cJSON_GetObjectItemCaseSensitive(j_params, "protocol");
    if (j_proto && cJSON_IsString(j_proto)) { strncat(cmd, " -p ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_proto->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    if (j_sport && cJSON_IsString(j_sport)) { strncat(cmd, " --sport ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_sport->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    if (j_dport && cJSON_IsString(j_dport)) { strncat(cmd, " --dport ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_dport->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    if (j_smac && cJSON_IsString(j_smac)) { strncat(cmd, " -m mac --mac-source ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_smac->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    if (j_dmac && cJSON_IsString(j_dmac)) { strncat(cmd, " -m mac --mac-source ", sizeof(cmd)-strlen(cmd)-1); strncat(cmd, j_dmac->valuestring, sizeof(cmd)-strlen(cmd)-1); }
    strncat(cmd, " -j ", sizeof(cmd)-strlen(cmd)-1);
    strncat(cmd, policy, sizeof(cmd)-strlen(cmd)-1);

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

int acl_delete_rule(const char *rule_id) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strcmp(g_acl_rules[i].rule_id, rule_id) == 0) {
            QosRuleEntry *r = &g_acl_rules[i];
            if (strlen(r->raw_cmd) > 0) {
                char del_cmd[QOS_MAX_CMD_LEN];
                strncpy(del_cmd, r->raw_cmd, sizeof(del_cmd) - 1);
                char *pos = strstr(del_cmd, "iptables -");
                if (pos) {
                    memcpy(pos, "iptables -D", 13);
                    char *num = strstr(del_cmd, " 1 ");
                    if (num) memmove(num, num + 2, strlen(num + 2) + 1);
                }
                ex_command(del_cmd);
            }
            if (r->params_json) { free(r->params_json); r->params_json = NULL; }
            for (int j = i; j < g_acl_rules_count - 1; j++) g_acl_rules[j] = g_acl_rules[j + 1];
            memset(&g_acl_rules[g_acl_rules_count - 1], 0, sizeof(QosRuleEntry));
            g_acl_rules_count--;
            return 0;
        }
    }
    return -1;
}

int acl_get_rule_json(const char *rule_id, char *out_buf, int buf_size) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strcmp(g_acl_rules[i].rule_id, rule_id) == 0) {
            QosRuleEntry *r = &g_acl_rules[i];
            cJSON *root = cJSON_CreateObject(), *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "rule_id", r->rule_id);
            cJSON_AddStringToObject(data, "name", r->name);
            cJSON_AddStringToObject(data, "action", r->action);
            cJSON_AddBoolToObject(data, "active", r->active);
            cJSON_AddNumberToObject(data, "created_at", (double)r->created_at);
            if (strlen(r->raw_cmd) > 0) cJSON_AddStringToObject(data, "raw_command", r->raw_cmd);
            if (r->params_json) { cJSON *p = cJSON_Parse(r->params_json); if (p) cJSON_AddItemToObject(data, "params", p); }
            cJSON_AddItemToObject(root, "status", cJSON_CreateString("success"));
            cJSON_AddItemToObject(root, "data", data);
            char *js = cJSON_Print(root);
            if (!js) { cJSON_Delete(root); return -1; }
            strncpy(out_buf, js, buf_size - 1);
            out_buf[buf_size - 1] = '\0';
            free(js); cJSON_Delete(root);
            return 0;
        }
    }
    return -1;
}

int acl_clear_all_rules(void) {
    for (int i = 0; i < g_acl_rules_count; i++) {
        if (strlen(g_acl_rules[i].raw_cmd) > 0) {
            char del_cmd[QOS_MAX_CMD_LEN];
            strncpy(del_cmd, g_acl_rules[i].raw_cmd, sizeof(del_cmd) - 1);
            char *pos = strstr(del_cmd, "iptables -");
            if (pos) { memcpy(pos, "iptables -D", 13); char *num = strstr(del_cmd, " 1 "); if (num) memmove(num, num + 2, strlen(num + 2) + 1); }
            ex_command(del_cmd);
        }
        if (g_acl_rules[i].params_json) { free(g_acl_rules[i].params_json); g_acl_rules[i].params_json = NULL; }
    }
    acl_remove_default_drop();
    g_acl_rules_count = 0; g_acl_next_rule_index = 1;
    memset(g_acl_rules, 0, sizeof(g_acl_rules));
    memset(g_acl_default_drop, 0, sizeof(g_acl_default_drop));
    g_acl_enabled = false;
    WTSL_LOG_INFO(MODULE_NAME, "[ACL] All ACL rules cleared");
    return 0;
}
