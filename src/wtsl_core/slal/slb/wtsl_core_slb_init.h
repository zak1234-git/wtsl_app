#ifndef __WTSL_CORE_SLB_INIT_H__
#define __WTSL_CORE_SLB_INIT_H__

/**
 * @brief 初始化 iptables 环境
 * 
 * 解决两个关键问题：
 * 1. 创建 /tmp/run 目录（iptables 锁文件需要）
 * 2. 设置 XTABLES_LIBDIR 环境变量（iptables 扩展模块路径）
 * 
 * @return 0 成功，-1 失败
 */
int slb_init_iptables_env(void);

/**
 * @brief SLB 模块初始化入口
 * 
 * 在应用启动时调用，准备所有必要的环境
 * 
 * @return 0 成功，-1 失败
 */
int slb_module_init(void);

#endif /* __WTSL_CORE_SLB_INIT_H__ */
