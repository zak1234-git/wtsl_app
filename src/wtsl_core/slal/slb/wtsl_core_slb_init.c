/**
 * @file wtsl_core_slb_init.c
 * @brief SLB 模块初始化（环境变量和目录准备）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "wtsl_log_manager.h"

#define MODULE_NAME "slb_init"

/**
 * @brief 初始化 iptables 环境
 * 
 * 解决两个关键问题：
 * 1. 创建 /tmp/run 目录（iptables 锁文件需要）
 * 2. 设置 XTABLES_LIBDIR 环境变量（iptables 扩展模块路径）
 * 
 * @return 0 成功，-1 失败
 */
int slb_init_iptables_env(void) {
    int ret = 0;
    
    // 1. 创建 /tmp/run 目录
    const char *lock_dir = "/tmp/run";
    struct stat st;
    
    if (stat(lock_dir, &st) != 0) {
        // 目录不存在，创建
        if (mkdir(lock_dir, 0755) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "Failed to create lock directory: %s", lock_dir);
            ret = -1;
        } else {
            WTSL_LOG_INFO(MODULE_NAME, "Created lock directory: %s", lock_dir);
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            WTSL_LOG_ERROR(MODULE_NAME, "%s exists but is not a directory", lock_dir);
            ret = -1;
        } else {
            WTSL_LOG_DEBUG(MODULE_NAME, "Lock directory already exists: %s", lock_dir);
        }
    }
    
    // 2. 设置 XTABLES_LIBDIR 环境变量
    const char *xtables_libdir = "/usr/lib/xtables";
    
    // 检查目录是否存在
    if (stat(xtables_libdir, &st) == 0 && S_ISDIR(st.st_mode)) {
        // 设置环境变量
        if (setenv("XTABLES_LIBDIR", xtables_libdir, 1) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "Failed to set XTABLES_LIBDIR");
            ret = -1;
        } else {
            WTSL_LOG_INFO(MODULE_NAME, "Set XTABLES_LIBDIR=%s", xtables_libdir);
        }
    } else {
        // 尝试其他可能的路径
        const char *alt_paths[] = {
            "/usr/lib/xtables",
            "/usr/local/lib/xtables",
            "/lib/xtables",
            NULL
        };
        
        int found = 0;
        for (int i = 0; alt_paths[i] != NULL; i++) {
            if (stat(alt_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
                if (setenv("XTABLES_LIBDIR", alt_paths[i], 1) == 0) {
                    WTSL_LOG_INFO(MODULE_NAME, "Set XTABLES_LIBDIR=%s (alternative path)", alt_paths[i]);
                    found = 1;
                    break;
                }
            }
        }
        
        if (!found) {
            WTSL_LOG_WARNING(MODULE_NAME, "Could not find xtables library directory");
            WTSL_LOG_WARNING(MODULE_NAME, "iptables extension modules may not load properly");
        }
    }
    
    return ret;
}

/**
 * @brief SLB 模块初始化入口
 * 
 * 在应用启动时调用，准备所有必要的环境
 * 
 * @return 0 成功，-1 失败
 */
int slb_module_init(void) {
    WTSL_LOG_INFO(MODULE_NAME, "Initializing SLB module...");
    
    // 初始化 iptables 环境
    if (slb_init_iptables_env() != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to initialize iptables environment");
        // 不返回错误，继续执行（iptables 可能仍然可用）
    }
    
    // 初始化 QoS 状态
    // （在 qos_core.c 中调用 qos_init_state）
    
    // 初始化 ACL 状态
    // （在 acl_core.c 中调用 acl_init_state）
    
    WTSL_LOG_INFO(MODULE_NAME, "SLB module initialized");
    return 0;
}
