/*
 * LogManager.h
 *
 *  Create and modify on:  2025年10月27日
 *      Author: vitoyang
 */

#ifndef INTERFACE_LOGMANAGER_LOGMANAGER_H_
#define INTERFACE_LOGMANAGER_LOGMANAGER_H_

#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "wtsl_client_upload.h"
#include "wtsl_core_api.h"
#include "wtsl_core_slb_interface.h"

#define APP_NAME "wtsl_app"

// 默认配置
#define DEFAULT_CONFIG_FILE 			"/home/system.conf"
#define DEFAULT_LOG_FILE				"/home/wt/log/wtsl_app.log"
#define DEFAULT_MAX_FILE_SIZE           100 * 1024// 5MB
#define DEFAULT_CONFIG_CHECK_INTERVAL 	5		// 5秒检查一次配置
#define LINE_LENGTH					    256		// 配置文件每行最大长度

// 颜色定义 （控制台输出用）
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// loging level
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
} log_level_t;

// config mode
typedef enum {
    CONFIG_MODE_UNKNOWN = 0,
    CONFIG_MODE_ALL,
    CONFIG_MODE_PRODUCT
} config_mode_t;

// log system global configuration
typedef struct {
    char app_name[64];              // 应用名称
    char config_file[256];          // 配置文件路径
    char log_file_path[256];        // 日志文件路径
    config_mode_t current_mode;     // 当前配置模式
    size_t max_file_size;           // 最大文件大小 (字节)
    tcp_upload_config_t upload_config;// TCP 上传配置

    // 线程控制
    pthread_t upload_thread;        // 上传线程
    pthread_mutex_t config_mutex;   // 配置访问互斥锁
    pthread_mutex_t log_mutex;      // 日志文件访问互斥锁
    pthread_cond_t upload_cond;     // 上传条件变量
    int upload_thread_running;      // 上传线程运行标志
    int trigger_upload;             // 触发上传标志

    time_t last_config_check;       // 上次检查配置时间
    int config_check_interval;      // 配置检查间隔
    FILE* log_file;                 // 日志文件
} log_system_t;

// 初始化日志系统
int wtsl_logger_init(const char* app_name, SPLINK_INFO global_node_info);

// 销毁日志系统
void wtsl_logger_destroy(void);

// 记录日志
void wtsl_logger_log(log_level_t level, const char* module, const char* format, ...);

// 强制重新加载配置
int wtsl_logger_reload_config(void);

// 获取当前配置状态
void wtsl_logger_print_status(void);

// 获取当前配置模式
config_mode_t wtsl_logger_get_current_mode(void);

// 检查是否应该记录指定级别的日志
int wtsl_logger_should_log(log_level_t level);

// 格式化时间（带毫秒）
void format_time_with_ms(char* buffer, size_t size);

// 重定向 stdout 和 stderr 到 /dev/null
void silence_stdout_stderr(void);

/**
 * 生成格式为 “tnode_YYYY-MM-DD” 的文件名
 * 返回值：指向动态分配的字符串的指针， 调用者需用free() 释放
 *      若失败则返回NULL
 */
char* generate_file_name(void);

// 宏简化日志调用
#define WTSL_LOG_DEBUG(module, ...) wtsl_logger_log(LOG_LEVEL_DEBUG, module, __VA_ARGS__)
#define WTSL_LOG_INFO(module, ...) wtsl_logger_log(LOG_LEVEL_INFO, module, __VA_ARGS__)
#define WTSL_LOG_WARNING(module, ...) wtsl_logger_log(LOG_LEVEL_WARNING, module, __VA_ARGS__)
#define WTSL_LOG_ERROR(module, ...) wtsl_logger_log(LOG_LEVEL_ERROR, module, __VA_ARGS__)
#define WTSL_LOG_FATAL(module, ...) wtsl_logger_log(LOG_LEVEL_FATAL, module, __VA_ARGS__)

#endif /* INTERFACE_LOGMANAGER_LOGMANAGER_H_ */