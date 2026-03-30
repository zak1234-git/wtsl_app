/*
 * LogManager.c
 *
 *  Created on: 2015年10月10日
 *      Author: jieen
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "wtsl_log_manager.h"
#include "wtsl_client_upload.h"
#include "wtsl_core_slb_interface.h"
#include "wtsl_core_api.h"

#if 0
unsigned char logTFlag = 0;
unsigned char logTLevel = 0;


#define KERNELDEBUG 1


/*******************************************************************************
*函数名称：void LogI(const char *__restrict __format, ...)
*功能描述：提示信息日志显示,只在debug版显示
*入口参数：
*出口参数：
*******************************************************************************/
void logI(const char *__restrict __format, ...){
#ifdef KERNELDEBUG
		va_list   arg;
		char pbString[4096];
		va_start(arg,__format);
		vsprintf(pbString,__format,arg);
		va_end(arg);
		fprintf(stdout,"[Info]%s",pbString);
#endif
}
/*******************************************************************************
*函数名称：void LogW(const char *__restrict __format, ...)
*功能描述：提示信息日志显示，debug和release版本都要显示
*入口参数：
*出口参数：
*******************************************************************************/
void logW(const char *__restrict __format, ...){
		va_list   arg;
		char pbString[4096];
		va_start(arg,__format);
		vsprintf(pbString,__format,arg);
		va_end(arg);
		fprintf(stderr,"[Warning]%s",pbString);
}
/*******************************************************************************
*函数名称：void LogE(const char *__restrict __format, ...)
*功能描述：提示信息日志显示，debug和release版本都要显示，并且需要保存到错误日志文件中
*入口参数：
*出口参数：
*备注：			临时设计，请修改为mmap方式
*******************************************************************************/
void logE(const char *__restrict __format, ...){
#if 0
			if(fpErrorFile == NULL){
				fpErrorFile = fopen(ERRORLOGFILE,"a+");
				if(fpErrorFile == NULL){
					fprintf(stderr,"[Error]open file %s error\n",ERRORLOGFILE);
				}
			}
			va_list   arg;
			char pbString[4096];
			va_start(arg,__format);
			vsprintf(pbString,__format,arg);
			va_end(arg);
			fprintf(stdout,"[Error]%s",pbString);

			if(fpErrorFile != NULL){
				fprintf(fpErrorFile,"%s",pbString);
				fflush(fpErrorFile);
				fclose(fpErrorFile);
			}
#else
			va_list   arg;
			char pbString[4096];
			va_start(arg,__format);
			vsprintf(pbString,__format,arg);
			va_end(arg);
			fprintf(stderr,"[Error]%s",pbString);
#endif
}
#else

// 全局日志系统
log_system_t g_log_system;

/**
 * 生成格式为 “tnode_YYYY-MM-DD” 的文件名
 * 返回值：指向动态分配的字符串的指针， 调用者需用free() 释放
 *      若失败则返回NULL
 */
char* generate_file_name(void)
{
    time_t now;
    struct tm *local_time;
    char date_str[11] = {0};

    // 获取当前本地时间
    time(&now);
    local_time = localtime(&now);
    if (!local_time) {
        return NULL;
    }

    // 格式化日期为 YYYY-MM-DD
    if (strftime(date_str, sizeof(date_str), "%Y-%m-%d", local_time) == 0) {
        return NULL; //格式化失败
    }

    const char *prefix_filename = "/home/wt/log/";
    const char *extension = ".log";

    // 计算所需总长度：name + '_' + date + '\0'
    size_t name_len = strlen(global_node_info.node_info.basic_info.name);
    size_t total_len = strlen(extension) + 1 + strlen(prefix_filename) + 1 + name_len + 2 + strlen(date_str) + 1;

    // 分配内存
    char *filename = malloc(total_len);
    if (!filename) {
        return NULL;
    }

    // 拼接字符串
    strcpy(filename, prefix_filename);
    strcat(filename, global_node_info.node_info.basic_info.name);
    strcat(filename, "__");
    strcat(filename, date_str);
    strcat(filename, extension);

    return filename;
}


// 获取日志级别字符串
static const char* get_level_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_WARNING: return "WARN";
        case LOG_LEVEL_ERROR:   return "ERROR";
        case LOG_LEVEL_FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

// 获取日志级别颜色
static const char* get_level_color(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return COLOR_BLUE;
        case LOG_LEVEL_INFO:    return COLOR_GREEN;
        case LOG_LEVEL_WARNING: return COLOR_YELLOW;
        case LOG_LEVEL_ERROR:   return COLOR_RED;
        case LOG_LEVEL_FATAL:   return COLOR_MAGENTA;
        default:                return COLOR_RESET;
    }
}

// 去除字符串两端的空白字符
static char* trim_whitespace(char* str) {
    if (str == NULL) return NULL;
    
    char* end;
    
    // 去除前导空白
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // 去除尾部空白
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    // 写入新的终止符
    *(end + 1) = 0;
    
    return str;
}

// 解析配置模式
static config_mode_t parse_config_mode(const char* mode_str) {
    if (mode_str == NULL) return CONFIG_MODE_UNKNOWN;
    
    char upper_str[32];
    int i;
    for (i = 0; mode_str[i] && i < 31; i++) {
        upper_str[i] = toupper(mode_str[i]);
    }
    upper_str[i] = '\0';
    
    if (strcmp(upper_str, "ALL") == 0) return CONFIG_MODE_ALL;
    if (strcmp(upper_str, "PRODUCT") == 0) return CONFIG_MODE_PRODUCT;
    
    return CONFIG_MODE_UNKNOWN;
}

// 检查是否应该记录日志
int wtsl_logger_should_log(log_level_t level) {
    switch (g_log_system.current_mode) {
        case CONFIG_MODE_ALL:
            return 1; // ALL模式记录所有级别
            
        case CONFIG_MODE_PRODUCT:
            // PRODUCT模式只记录ERROR和WARN
            return (level == LOG_LEVEL_WARNING ||
					level == LOG_LEVEL_ERROR ||
					level == LOG_LEVEL_FATAL);
            
        default:
            return 0; // 未知模式不记录
    }
}

// 确保目录存在
static int ensure_directory_exists(const char* filepath) {
    char* path_copy = strdup(filepath);
    if (!path_copy) return -1;
    
    char* dir = dirname(path_copy);
    if (strcmp(dir, ".") != 0 && strcmp(dir, "/") != 0) {
        // 递归创建目录
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" 2>/dev/null", dir);
        int ret = system(cmd);
        if (ret != 0) {
            free(path_copy);
            return -1;
        }
    }
    
    free(path_copy);
    return 0;
}

// 打开日志文件
static int open_log_file() {
    if (g_log_system.log_file) {
        fclose(g_log_system.log_file);
        g_log_system.log_file = NULL;
    }
    
    if (ensure_directory_exists(g_log_system.log_file_path) != 0) {
        return -1;
    }
    
    g_log_system.log_file = fopen(g_log_system.log_file_path, "a");
    if (!g_log_system.log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}

// 解析配置文件
static int parse_config_file() {
    FILE* file = fopen(g_log_system.config_file, "r");
    if (!file) {
        fprintf(stderr, "Cannot open config file: %s\n", g_log_system.config_file);
        return -1;
    }
    
    char line[LINE_LENGTH];
    int in_chip_log_system = 0;
    int line_num = 0;
    config_mode_t new_mode = CONFIG_MODE_UNKNOWN;
	int found_mode = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char* trimmed_line = trim_whitespace(line);
        
        // 跳过空行
        if (strlen(trimmed_line) == 0) {
            continue;
        }
        
        // 检查节头
        if (trimmed_line[0] == '[') {
            char* end_bracket = strchr(trimmed_line, ']');
            if (end_bracket) {
                *end_bracket = '\0';
                char* section = trim_whitespace(trimmed_line + 1);
                
                if (strcasecmp(section, "chip log_system") == 0) {
                    in_chip_log_system = 1;
                } else {
                    in_chip_log_system = 0;
                }
            }
            continue;
        }
        
        // 如果在 [chip log_system] 节内，解析配置
        if (in_chip_log_system) {
            // 跳过注释
            if (trimmed_line[0] == '#' || trimmed_line[0] == ';') {
                continue;
            }
            
			
            // 检查是否是模式行
			if (!found_mode) {
				config_mode_t line_mode = parse_config_mode(trimmed_line);
            	if (line_mode != CONFIG_MODE_UNKNOWN) {
                	new_mode = line_mode;
                    found_mode = 1;
                	continue;
            	}
			}
        }
    }
    
    fclose(file);
    
    // 如果没有找到有效配置，使用默认值
	if (found_mode) {
		g_log_system.current_mode = new_mode;
	} else {
        g_log_system.current_mode = CONFIG_MODE_ALL;
        fprintf(stderr, "No valid configuration found, using default mode: ALL\n");
    }
    
    return 0;
}

// 检查配置文件是否需要重新加载
static int need_config_reload() {
    struct stat st;
    if (stat(g_log_system.config_file, &st) != 0) {
        return 0; // 文件不存在或无法访问
    }
    
    time_t now = time(NULL);
    if (now - g_log_system.last_config_check >= g_log_system.config_check_interval) {
        g_log_system.last_config_check = now;
        
        // 检查文件修改时间
        static time_t last_mtime = 0;
        if (st.st_mtime > last_mtime) {
            last_mtime = st.st_mtime;
            return 1;
        }
    }
    
    return 0;
}

// 格式化时间（带毫秒）
void format_time_with_ms(char* buffer, size_t size) {
    struct timeval tv;
    struct tm* tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 添加毫秒
    char ms_buffer[16];
    snprintf(ms_buffer, sizeof(ms_buffer), ".%03d", (int)(tv.tv_usec / 1000));
    strncat(buffer, ms_buffer, size - strlen(buffer) - 1);
}

// 获取当前配置模式
config_mode_t wtsl_logger_get_current_mode() {
    return g_log_system.current_mode;
}

// 重定向stdout和stderr 到 /dev/null
void silence_stdout_stderr(void) {
    int fd = open("/dev/null", O_WRONLY);
    if (fd != -1) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

// 初始化日志系统
int wtsl_logger_init(const char* app_name, SPLINK_INFO global_node_info) {
    // 初始化全局结构
    memset(&g_log_system, 0, sizeof(g_log_system));
    
    if (app_name) {
        strncpy(g_log_system.app_name, app_name, sizeof(g_log_system.app_name) - 1);
    } else {
        strcpy(g_log_system.app_name, "wtsl_app");
    }
    
    strcpy(g_log_system.config_file, DEFAULT_CONFIG_FILE);
    char *filename = generate_file_name();
    if (filename) {
        printf("filename = %s\n", filename);
        strcpy(g_log_system.log_file_path, filename);
        printf("g_log_system.log_file_path = %s\n", g_log_system.log_file_path);
        free(filename);
    } else {
        fprintf(stderr, "Failed to generate log file path. use default log file\n");
        strcpy(g_log_system.log_file_path, DEFAULT_LOG_FILE);
    }
    g_log_system.config_check_interval = DEFAULT_CONFIG_CHECK_INTERVAL;
    g_log_system.max_file_size = DEFAULT_MAX_FILE_SIZE;
    g_log_system.last_config_check = time(NULL);
    g_log_system.current_mode = CONFIG_MODE_UNKNOWN;
    g_log_system.log_file = NULL;

    // set default TCP upload config
    strcpy(g_log_system.upload_config.server_ip, global_node_info.node_info.basic_info.net_manage_ip);
    g_log_system.upload_config.server_port = global_node_info.node_info.basic_info.log_port;

    // set default address
    g_log_system.upload_config.source_addr.reserved = 0;
    g_log_system.upload_config.source_addr.subnet_id = 101;
    g_log_system.upload_config.source_addr.t_node_id = 16;
    g_log_system.upload_config.source_addr.device_id = 16;

    g_log_system.upload_config.dest_addr.reserved = 0;
    g_log_system.upload_config.dest_addr.subnet_id = 102;
    g_log_system.upload_config.dest_addr.t_node_id = 16;
    g_log_system.upload_config.dest_addr.device_id = 16;

    g_log_system.upload_config.timeout_seconds = 10;
    g_log_system.upload_config.reconnect_interval = 3;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_log_system.config_mutex, NULL) != 0) {
        return -1;
    }
    
    if (pthread_mutex_init(&g_log_system.log_mutex, NULL) != 0) {
        pthread_mutex_destroy(&g_log_system.config_mutex);
        return -1;
    }

    if (pthread_cond_init(&g_log_system.upload_cond, NULL) != 0) {
        pthread_mutex_destroy(&g_log_system.config_mutex);
        pthread_mutex_destroy(&g_log_system.log_mutex);
        return -1;
    }
    
    // 打开日志文件
    if (open_log_file() != 0) {
        pthread_mutex_destroy(&g_log_system.config_mutex);
        pthread_mutex_destroy(&g_log_system.log_mutex);
        pthread_cond_destroy(&g_log_system.upload_cond);
        return -1;
    }
    
    // 加载初始配置
    if (parse_config_file() != 0) {
        fprintf(stderr, "Warning: Failed to parse initial config file\n");
        // 继续使用默认配置
        g_log_system.current_mode = CONFIG_MODE_ALL;
    }

    // 启动上传线程
    g_log_system.upload_thread_running = 1;
    g_log_system.trigger_upload = 0;

    if (pthread_create(&g_log_system.upload_thread, NULL, upload_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create upload thread\n");
        pthread_mutex_destroy(&g_log_system.config_mutex);
        pthread_mutex_destroy(&g_log_system.log_mutex);
        pthread_cond_destroy(&g_log_system.upload_cond);
        fclose(g_log_system.log_file);
        return -1;
    }
    
    // 写入启动日志
    char time_buffer[64];
    format_time_with_ms(time_buffer, sizeof(time_buffer));
    pthread_mutex_lock(&g_log_system.log_mutex);
    fprintf(g_log_system.log_file, "[%s] [%s] [SYSTEM] [STARTUP] : Logger system initialized with mode: %s\n", 
            time_buffer, g_log_system.app_name,
            g_log_system.current_mode == CONFIG_MODE_ALL ? "ALL" : "PRODUCT");
    fprintf(g_log_system.log_file, "[%s] [%s] [SYSTEM] [UPLOAD] : TCP upload configured to %s:%d\n", 
            time_buffer, g_log_system.app_name,
            g_log_system.upload_config.server_ip,
            g_log_system.upload_config.server_port);
    fflush(g_log_system.log_file);
    pthread_mutex_unlock(&g_log_system.log_mutex);
    
    printf("WTSL Logger initialized for app: %s\n", g_log_system.app_name);
    printf("Current mode: %s\n", g_log_system.current_mode == CONFIG_MODE_ALL ? "ALL" : "PRODUCT");
    printf("Max file size: %zu bytes (%.2f MB)\n",
            g_log_system.max_file_size,
            (double)g_log_system.max_file_size / (1024 * 1024));
    printf("TCP upload configured to %s:%d",
            g_log_system.upload_config.server_ip,
            g_log_system.upload_config.server_port);
    
    printf("Source Address: subnet=%d, t_node=%d, device=%d\n",
            g_log_system.upload_config.source_addr.subnet_id,
            g_log_system.upload_config.source_addr.t_node_id,
            g_log_system.upload_config.source_addr.device_id);

    printf("Destination Address: subnet=%d, t_node=%d, device=%d\n",
            g_log_system.upload_config.dest_addr.subnet_id,
            g_log_system.upload_config.dest_addr.t_node_id,
            g_log_system.upload_config.dest_addr.device_id);

    return 0;
}

// 销毁日志系统
void wtsl_logger_destroy() {

    // 停止上传线程
    pthread_mutex_lock(&g_log_system.config_mutex);
    g_log_system.upload_thread_running = 0;
    pthread_cond_signal(&g_log_system.upload_cond);
    pthread_mutex_unlock(&g_log_system.config_mutex);

    // wait upload thread end
    pthread_join(g_log_system.upload_thread, NULL);

    // 写入关闭日志
    char time_buffer[64];
    format_time_with_ms(time_buffer, sizeof(time_buffer));
    
    pthread_mutex_lock(&g_log_system.log_mutex);
    if (g_log_system.log_file) {
        fprintf(g_log_system.log_file, "[%s] [%s] [SYSTEM] [SHUTDOWN] : Logger system shutdown\n", 
                time_buffer, g_log_system.app_name);
        fclose(g_log_system.log_file);
        g_log_system.log_file = NULL;
    }
    pthread_mutex_unlock(&g_log_system.log_mutex);
    
    pthread_mutex_destroy(&g_log_system.config_mutex);
    pthread_mutex_destroy(&g_log_system.log_mutex);
    pthread_cond_destroy(&g_log_system.upload_cond);
}


// 强制重新加载配置
int wtsl_logger_reload_config() {
    int result = parse_config_file();
    
    // 记录配置重载事件
    char time_buffer[64];
    format_time_with_ms(time_buffer, sizeof(time_buffer));
    
    pthread_mutex_lock(&g_log_system.log_mutex);
    if (g_log_system.log_file) {
        fprintf(g_log_system.log_file, "[%s] [%s] [SYSTEM] [CONFIG] : Configuration reloaded, mode: %s\n", 
                time_buffer, g_log_system.app_name,
                g_log_system.current_mode == CONFIG_MODE_ALL ? "ALL" : "PRODUCT");
        fflush(g_log_system.log_file);
    }
    pthread_mutex_unlock(&g_log_system.log_mutex);
    
    return result;
}

// 获取当前配置状态
void wtsl_logger_print_status() {
    size_t current_size = wtsl_logger_get_current_file_size();

    printf("=== WTSL Logger Status ===\n");
    printf("App Name: %s\n", g_log_system.app_name);
    printf("Config File: %s\n", g_log_system.config_file);
    printf("Log File: %s\n", DEFAULT_LOG_FILE);
    printf("Current Mode: %s\n", 
            g_log_system.current_mode == CONFIG_MODE_ALL ? "ALL" : "PRODUCT");
    printf("Max file size: %zu bytes (%.2f MB)\n",
            g_log_system.max_file_size,
            (double)g_log_system.max_file_size / (1024 * 1024));
    printf("Current file size: %zu bytes (%.2f MB)\n",
            current_size,
            (double)current_size / (1024 * 1024));
    printf("TCP upload: %s:%d\n",
            g_log_system.upload_config.server_ip,
            g_log_system.upload_config.server_port);
    printf("Source Address: subnet=%d, t_node=%d, device=%d\n",
            g_log_system.upload_config.source_addr.subnet_id,
            g_log_system.upload_config.source_addr.t_node_id,
            g_log_system.upload_config.source_addr.device_id);
    printf("Destination Address: subnet=%d, t_node=%d, device=%d\n",
            g_log_system.upload_config.dest_addr.subnet_id,
            g_log_system.upload_config.dest_addr.t_node_id,
            g_log_system.upload_config.dest_addr.device_id);
    printf("Upload Thread: %s\n",
            g_log_system.upload_thread_running ? "RUNNING" : "STOPPED");
    printf("==========================\n");
}

// 记录日志
void wtsl_logger_log(log_level_t level, const char* module, const char* format, ...) {
    // 检查配置是否需要重新加载
    if (need_config_reload()) {
        wtsl_logger_reload_config();
    }
    
    // 根据当前模式判断是否记录该级别日志
    if (!wtsl_logger_should_log(level)) {
        return;
    }
    
    pthread_mutex_lock(&g_log_system.log_mutex);
    
    // 准备变量参数列表
    va_list args;
    char time_buffer[64];
    char log_buffer[4096];
    
    // 格式化时间（带毫秒）
    format_time_with_ms(time_buffer, sizeof(time_buffer));
    
    // 构建日志前缀 - 格式: [时间] [app名称] [日志等级] [模块名] : 日志内容
    int prefix_len = snprintf(log_buffer, sizeof(log_buffer) - 1,
                             "[%s][%s][%s][%s]: ",
                             time_buffer, 
                             g_log_system.app_name,
                             get_level_string(level),
                             module);
    
    // 处理用户格式
    va_start(args, format);
    int content_len = vsnprintf(log_buffer + prefix_len, 
                               sizeof(log_buffer) - prefix_len - 1, 
                               format, args);
    va_end(args);
    
    // 添加换行符
    int total_len = prefix_len + content_len;
    if (total_len < (int)sizeof(log_buffer) - 2) {
        log_buffer[total_len] = '\n';
        log_buffer[total_len + 1] = '\0';
    } else {
        // 防止缓冲区溢出
        log_buffer[sizeof(log_buffer) - 2] = '\n';
        log_buffer[sizeof(log_buffer) - 1] = '\0';
    }
    
    // 输出到控制台（带颜色）
    const char* color = get_level_color(level);
    fprintf(stderr, "%s%s%s", color, log_buffer, COLOR_RESET);
    fflush(stderr);
    
    // 输出到文件
    if (g_log_system.log_file) {
        fprintf(g_log_system.log_file, "%s", log_buffer);
        fflush(g_log_system.log_file);
    }
    
    pthread_mutex_unlock(&g_log_system.log_mutex);
}

#endif 