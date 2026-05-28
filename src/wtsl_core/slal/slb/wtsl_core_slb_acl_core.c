#include "wtsl_core_slb_acl_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "wtsl_log_manager.h"

#define MODULE_NAME "acl_core"

// 定义全局状态
AclGlobalState g_acl_state;

// 执行命令
static int ex_command(const char *cmd) {
    WTSL_LOG_DEBUG(MODULE_NAME, "[EXEC] %s", cmd);
    
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "popen failed");
        return -1;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(cmd, "-L") || strstr(cmd, "-S")) {
            WTSL_LOG_DEBUG(MODULE_NAME, "[IPT-OUT] %s", buffer);
        }
    }
    
    int status = pclose(fp);
    if (WEXITSTATUS(status) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "[IPT Error] Command failed with exit code %d", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

// 记录规则
static void record_rule(const char *cmd) {
    if (g_acl_state.rule_count >= ACL_MAX_RULES) return;
    
    // 简单去重
    for (int i = 0; i < g_acl_state.rule_count; i++) {
        if (strcmp(g_acl_state.rules[i], cmd) == 0) return;
    }
    
    strncpy(g_acl_state.rules[g_acl_state.rule_count], cmd, ACL_MAX_CMD_LEN - 1);
    g_acl_state.rule_count++;
    WTSL_LOG_DEBUG(MODULE_NAME, "[ACL] Recorded rule #%d", g_acl_state.rule_count);
}

/**
 * @brief 初始化 ACL 状态
 */
void acl_init_state(const char *default_dev) {
    g_acl_state.enabled = false;
    g_acl_state.rule_count = 0;
    memset(g_acl_state.rules, 0, sizeof(g_acl_state.rules));
    strncpy(g_acl_state.default_device, default_dev, sizeof(g_acl_state.default_device) - 1);
    g_acl_state.default_device[sizeof(g_acl_state.default_device) - 1] = '\0';
    
    WTSL_LOG_INFO(MODULE_NAME, "[ACL] Engine Initialized. Default Device: %s", g_acl_state.default_device);
}

/**
 * @brief 构建 iptables 命令
 */
static int build_iptables_cmd(char *cmd, size_t cmd_size, const char *action, const char *chain, cJSON *params) {
    const char *flag = "-A";
    if (strcmp(action, "delete") == 0 || strcmp(action, "del") == 0) {
        flag = "-D";
    } else if (strcmp(action, "insert") == 0) {
        flag = "-I";
    } else if (strcmp(action, "replace") == 0) {
        flag = "-R";
    }
    
    // 使用 -w 等待锁，解决 lock file 问题
    snprintf(cmd, cmd_size, "iptables -w %s %s", flag, chain);
    
    // 协议
    cJSON *j_proto = cJSON_GetObjectItemCaseSensitive(params, "protocol");
    if (j_proto && cJSON_IsString(j_proto)) {
        strcat(cmd, " -p ");
        strcat(cmd, j_proto->valuestring);
    }
    
    // 源地址
    cJSON *j_src = cJSON_GetObjectItemCaseSensitive(params, "source");
    if (j_src && cJSON_IsString(j_src)) {
        strcat(cmd, " -s ");
        strcat(cmd, j_src->valuestring);
    }
    
    // 目标地址
    cJSON *j_dst = cJSON_GetObjectItemCaseSensitive(params, "destination");
    if (j_dst && cJSON_IsString(j_dst)) {
        strcat(cmd, " -d ");
        strcat(cmd, j_dst->valuestring);
    }
    
    // 入站接口
    cJSON *j_in = cJSON_GetObjectItemCaseSensitive(params, "in_interface");
    if (j_in && cJSON_IsString(j_in)) {
        strcat(cmd, " -i ");
        strcat(cmd, j_in->valuestring);
    }
    
    // 出站接口
    cJSON *j_out = cJSON_GetObjectItemCaseSensitive(params, "out_interface");
    if (j_out && cJSON_IsString(j_out)) {
        strcat(cmd, " -o ");
        strcat(cmd, j_out->valuestring);
    }
    
    // 端口处理（需要先添加 -m tcp 模块）
    cJSON *j_dport = cJSON_GetObjectItemCaseSensitive(params, "dest_port");
    cJSON *j_sport = cJSON_GetObjectItemCaseSensitive(params, "source_port");
    
    if (j_dport || j_sport) {
        // 添加 tcp 模块支持
        strcat(cmd, " -m tcp");
        
        if (j_sport && cJSON_IsString(j_sport)) {
            strcat(cmd, " --sport ");
            strcat(cmd, j_sport->valuestring);
        }
        
        if (j_dport && cJSON_IsString(j_dport)) {
            strcat(cmd, " --dport ");
            strcat(cmd, j_dport->valuestring);
        }
    }
    
    // 动作目标
    cJSON *j_target = cJSON_GetObjectItemCaseSensitive(params, "target");
    if (j_target && cJSON_IsString(j_target)) {
        strcat(cmd, " -j ");
        strcat(cmd, j_target->valuestring);
    }
    
    // 行号 (用于 replace/delete)
    cJSON *j_rule_num = cJSON_GetObjectItemCaseSensitive(params, "rule_num");
    if (j_rule_num && cJSON_IsNumber(j_rule_num)) {
        char num_buf[16];
        snprintf(num_buf, sizeof(num_buf), "%d", j_rule_num->valueint);
        // 插入到 chain 后面
        char temp[ACL_MAX_CMD_LEN];
        snprintf(temp, sizeof(temp), "iptables -w %s %s %s", flag, chain, num_buf);
        if (strcmp(action, "replace") == 0) {
            // replace 需要特殊处理
            snprintf(cmd, cmd_size, "iptables -w -R %s %s", chain, num_buf);
            // 重新添加其他参数
            if (j_proto && cJSON_IsString(j_proto)) {
                strcat(cmd, " -p "); strcat(cmd, j_proto->valuestring);
            }
            if (j_src && cJSON_IsString(j_src)) {
                strcat(cmd, " -s "); strcat(cmd, j_src->valuestring);
            }
            if (j_dst && cJSON_IsString(j_dst)) {
                strcat(cmd, " -d "); strcat(cmd, j_dst->valuestring);
            }
            if (j_in && cJSON_IsString(j_in)) {
                strcat(cmd, " -i "); strcat(cmd, j_in->valuestring);
            }
            if (j_out && cJSON_IsString(j_out)) {
                strcat(cmd, " -o "); strcat(cmd, j_out->valuestring);
            }
            if (j_sport && cJSON_IsString(j_sport)) {
                strcat(cmd, " --sport "); strcat(cmd, j_sport->valuestring);
            }
            if (j_dport && cJSON_IsString(j_dport)) {
                strcat(cmd, " --dport "); strcat(cmd, j_dport->valuestring);
            }
            if (j_target && cJSON_IsString(j_target)) {
                strcat(cmd, " -j "); strcat(cmd, j_target->valuestring);
            }
        }
    }
    
    return 0;
}

/**
 * @brief 获取防火墙状态
 */
int acl_get_status(cJSON *resp) {
    cJSON_AddItemToObject(resp, "enabled", cJSON_CreateBool(g_acl_state.enabled));
    cJSON_AddItemToObject(resp, "rule_count", cJSON_CreateNumber(g_acl_state.rule_count));
    cJSON_AddItemToObject(resp, "device", cJSON_CreateString(g_acl_state.default_device));
    cJSON_AddItemToObject(resp, "type", cJSON_CreateString("iptables"));
    return 0;
}

/**
 * @brief ACL 总开关逻辑
 */
int acl_toggle_switch(bool enable) {
    if (g_acl_state.enabled == enable) return 0;
    
    char cmd[ACL_MAX_CMD_LEN];
    
    if (!enable) {
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Disabling FORWARD firewall...");
        
        // 只清空 FORWARD 链的规则
        snprintf(cmd, sizeof(cmd), "iptables -w -F FORWARD");
        ex_command(cmd);
        
        // 设置 FORWARD 默认策略为 ACCEPT
        snprintf(cmd, sizeof(cmd), "iptables -w -P FORWARD ACCEPT");
        ex_command(cmd);
        
        g_acl_state.enabled = false;
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "[ACL] Enabling FORWARD firewall...");
        
        // 设置 FORWARD 默认策略为 DROP
        snprintf(cmd, sizeof(cmd), "iptables -w -P FORWARD DROP");
        ex_command(cmd);
        
        // 允许已建立的连接（FORWARD 链）
        snprintf(cmd, sizeof(cmd), "iptables -w -A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null || true");
        ex_command(cmd);
        
        g_acl_state.enabled = true;
    }
    
    return 0;
}

/**
 * @brief 处理 ACL 规则请求
 */
int acl_handle_request(const char *action, const char *chain, cJSON *params, cJSON *resp) {
    char cmd[ACL_MAX_CMD_LEN];
    int ret = 0;
    
    WTSL_LOG_INFO(MODULE_NAME, "[ACL] action=%s, chain=%s", action, chain);
    
    // 限制只能操作 FORWARD 链
    if (strcmp(chain, "FORWARD") != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "[ACL] Only FORWARD chain is supported, ignoring chain: %s", chain);
        cJSON_AddItemToObject(resp, "success", cJSON_CreateBool(false));
        cJSON_AddItemToObject(resp, "error", cJSON_CreateString("Only FORWARD chain is supported"));
        return -1;
    }
    
    // 特殊处理：list 动作
    if (strcmp(action, "list") == 0 || strcmp(action, "get") == 0) {
        char output[4096];
        FILE *fp;
        
        // 使用 iptables -S 获取规则
        snprintf(cmd, sizeof(cmd), "iptables -w -S %s", chain);
        fp = popen(cmd, "r");
        if (fp == NULL) {
            cJSON_AddItemToObject(resp, "error", cJSON_CreateString("Failed to execute iptables"));
            return -1;
        }
        
        cJSON *rules_array = cJSON_CreateArray();
        char line[1024];
        while (fgets(line, sizeof(line), fp) != NULL) {
            // 跳过 -P (policy) 行
            if (strncmp(line, "-P", 2) == 0) continue;
            
            // 去掉换行符
            line[strcspn(line, "\n")] = 0;
            cJSON_AddItemToArray(rules_array, cJSON_CreateString(line));
        }
        pclose(fp);
        
        cJSON_AddItemToObject(resp, "rules", rules_array);
        return 0;
    }
    
    // 构建命令
    build_iptables_cmd(cmd, sizeof(cmd), action, chain, params);
    
    WTSL_LOG_DEBUG(MODULE_NAME, "[ACL] CMD: %s", cmd);
    
    ret = ex_command(cmd);
    
    if (ret == 0) {
        // 记录规则 (仅 add/insert)
        if (strcmp(action, "add") == 0 || strcmp(action, "insert") == 0) {
            record_rule(cmd);
        }
        
        cJSON_AddItemToObject(resp, "success", cJSON_CreateBool(true));
        cJSON_AddItemToObject(resp, "action", cJSON_CreateString(action));
        cJSON_AddItemToObject(resp, "chain", cJSON_CreateString(chain));
        cJSON_AddItemToObject(resp, "message", cJSON_CreateString("Operation successful"));
    } else {
        cJSON_AddItemToObject(resp, "success", cJSON_CreateBool(false));
        cJSON_AddItemToObject(resp, "error", cJSON_CreateString("Command execution failed"));
    }
    
    return ret;
}
