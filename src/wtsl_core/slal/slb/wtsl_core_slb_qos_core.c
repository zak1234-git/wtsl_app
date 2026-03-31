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
    memset(g_state.snapshot_rules, 0, sizeof(g_state.snapshot_rules));
    // 安全拷贝默认设备名
    strncpy(g_state.default_device, default_dev, sizeof(g_state.default_device) - 1);
    g_state.default_device[sizeof(g_state.default_device) - 1] = '\0';
    
    WTSL_LOG_INFO(MODULE_NAME, "[QoS] Engine Initialized. Default Device: %s", g_state.default_device);
}

/**
 * @brief 执行 Shell 命令
 * @param cmd 要执行的完整 tc 命令
 * @return 0 表示成功，-1 表示失败
 */
int ex_command(const char *cmd) {
    WTSL_LOG_DEBUG(MODULE_NAME, "[EXEC] %s", cmd);
    
    // 使用 popen 执行命令，"r" 表示读取输出 (主要用于 show 命令)
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "popen failed");
        return -1;
    }
    
    // 如果是 show 命令，读取并打印输出 (实际项目中可存入缓冲区返回给前端)
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // 这里简单打印到 stdout，生产环境可收集到字符串
        if (strstr(cmd, "show") || strstr(cmd, "-L")) {
            WTSL_LOG_DEBUG(MODULE_NAME, "[TC-OUT] %s", buffer);
        }
    }
    
    int status = pclose(fp);
    if (WEXITSTATUS(status) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[TC Error] Command failed with exit code %d", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

// 记录快照
void record_rule(const char *cmd) {
    if (g_state.snapshot_count >= MAX_SNAPSHOT_RULES) return;
    // 简单去重
    for (int i = 0; i < g_state.snapshot_count; i++) {
        if (strcmp(g_state.snapshot_rules[i], cmd) == 0) return;
    }
    strncpy(g_state.snapshot_rules[g_state.snapshot_count], cmd, QOS_MAX_CMD_LEN - 1);
    g_state.snapshot_count++;
    WTSL_LOG_DEBUG(MODULE_NAME, "[SNAPSHOT] Recorded #%d\n", g_state.snapshot_count);
}

int tc_handle_request(const char *action, const char *obj_type, cJSON *params) {
    char cmd[QOS_MAX_CMD_LEN];
    cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(params, "device");
    const char *dev = (j_dev && cJSON_IsString(j_dev)) ? j_dev->valuestring : g_state.default_device;

    // 基础命令： tc <action> <type> dev <dev>
    snprintf(cmd, sizeof(cmd), "tc %s %s dev %s", action, obj_type, dev);

    // 公共参数
    cJSON *j_parent = cJSON_GetObjectItemCaseSensitive(params, "parent");
    cJSON *j_handle = cJSON_GetObjectItemCaseSensitive(params, "handle");
    cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(params, "kind");
    cJSON *j_flowid = cJSON_GetObjectItemCaseSensitive(params, "flowid");
    cJSON *j_args = cJSON_GetObjectItemCaseSensitive(params, "args");

    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) {
        if (j_parent) { strcat(cmd, " parent "); strcat(cmd, j_parent->valuestring); }
        if (j_handle) { strcat(cmd, " handle "); strcat(cmd, j_handle->valuestring); }
    } else {
        if (j_parent) { strcat(cmd, " parent "); strcat(cmd, j_parent->valuestring); }
        if (j_handle) { strcat(cmd, " handle "); strcat(cmd, j_handle->valuestring); }

        if (j_kind && (strcmp(action, "add") == 0 || strcmp(action, "replace") == 0)) {
            strcat(cmd, " ");
            strcat(cmd, j_kind->valuestring);
        }

        // 处理动态 Args
        if (j_args && cJSON_IsObject(j_args)) {
            cJSON *item;
            cJSON_ArrayForEach(item, j_args) {
                const char *val = NULL;
                static char num_buf[32];
                if (cJSON_IsString(item)) {
                    val = item->valuestring;
                } else if (cJSON_IsNumber(item)) {
                    snprintf(num_buf, sizeof(num_buf), "%g", item->valuedouble);
                    val = num_buf;
                }

                if (val) {
                    strcat(cmd, " ");
                    strcat(cmd, item->string);
                    strcat(cmd, " ");
                    strcat(cmd, val);
                }
            }
        }

        // Filter 特有参数
        if (strcmp(obj_type, "filter") == 0 && j_flowid) {
            strcat(cmd, " flowid ");
            strcat(cmd, j_flowid->valuestring);
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
        strcat(cmd, " -p ");
        strcat(cmd, j_proto->valuestring);
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
            strcat(cmd, " -i ");
            strcat(cmd, dev);
        } else if (strcmp(chain, "OUTPUT") == 0) {
            strcat(cmd, " -o ");
            strcat(cmd, dev);
        }
    } else {
        if (j_in) {
            strcat(cmd, " -i ");
            strcat(cmd, j_in->valuestring);
        }
        if (j_out) {
            strcat(cmd, " -o ");
            strcat(cmd, j_out->valuestring);
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
                val = item->valuestring;
            } else if (cJSON_IsNumber(item)) {
                snprintf(num_buf, sizeof(num_buf), "%d", (int)item->valuedouble);
                val = num_buf;
            }
            if (!val) {
                continue;
            }
            strcat(cmd, " ");
            if (strcmp(key, "source") == 0 || strcmp(key, "src") == 0) {
                strcat(cmd, "-s ");
            } else if (strcmp(key, "destination") == 0 || strcmp(key, "dst") == 0) {
                strcat(cmd, "-d ");
            } else if (strcmp(key, "sport") == 0) {
                strcat(cmd, "--sport ");
            } else if (strcmp(key, "dport") == 0) {
                strcat(cmd, "--dport ");
            } else if (strcmp(key, "jump") == 0 || strcmp(key, "target") == 0) {
                strcat(cmd, "-j ");
            } else {
                strcat(cmd, "--");
                strcat(cmd, key);
                strcat(cmd, " ");
            }

            strcat(cmd, val);
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
        WTSL_LOG_INFO(MODULE_NAME, "[SWITCH] Disabling QoS/FW...");
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
                else if (strstr(del_cmd, "iptables -R")) continue; // Replace 难直接删，跳过或特殊处理
                
                WTSL_LOG_DEBUG(MODULE_NAME, "[CLEAN] %s", del_cmd);
                system(del_cmd);
            }
        }
        g_state.enabled = false;
    } else {
        WTSL_LOG_DEBUG(MODULE_NAME, "[SWITCH] Enabling QoS/FW...");
        for (int i = 0; i < g_state.snapshot_count; i++) {
            WTSL_LOG_DEBUG("[RESTORE] %s", g_state.snapshot_rules[i]);
            system(g_state.snapshot_rules[i]);
        }
        g_state.enabled = true;
    }
    return 0;
}