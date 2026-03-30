#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
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
#include <pthread.h>

#include "wtsl_log_manager.h"
#include "wtsl_client_upload.h"

#define MODULE_NAME "client_upload"

extern log_system_t g_log_system;

static size_t get_file_size(const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

// 检查是否需要上传并清空日志文件
int need_upload_and_clear()
{
    if (!g_log_system.log_file) return 0;

    // 刷新缓冲区确保所有数据写入文件
    fflush(g_log_system.log_file);

    size_t current_size = get_file_size(g_log_system.log_file_path);
    // printf("FFLUSH BUFFER ALL data and write to file: %d\n", current_size);
    return (current_size >= g_log_system.max_file_size);
}

// 计算校验和（从帧头到校验之前的数据按字节异或）
static uint16_t calculate_checksum(const uint8_t* data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return (uint16_t)checksum;
}

// 将32位地址结构转换为网络字节序的32位整数
static uint32_t address_to_little_endian(address_32bit_t addr)
{
    uint32_t result = 0;

    // 按照协议格式组装： 2位保留 + 14位子网号 + 8位T节点 + 8位设备
    result = ((uint32_t)(addr.device_id & 0xFF)) |
            ((uint32_t)(addr.t_node_id & 0xFF) << 8) |
            ((uint32_t)(addr.subnet_id & 0x3FFF) << 16) |
            ((uint32_t)(addr.reserved & 0x03) << 30);
    
    return result;
}

// 构建日志上传报文帧
static int build_log_upload_frame(const uint8_t *data, size_t data_length, uint32_t data_type,
                                    uint8_t data_identifier, uint8_t **frame_data, size_t *frame_length)
{
    if (!data || data_length == 0) {
        return -1;
    }

    // 计算帧总长度
    *frame_length = 2 + 2 + 4 + 4 + 4 + 4 + data_length + 2 + 2 + 2;
    *frame_data = (uint8_t*)malloc(*frame_length);
    if (!*frame_data) {
        return -1;
    }

    uint8_t *ptr = *frame_data;

    // 帧头（2字节）
    uint16_t header = FRAME_HEADER;
    memcpy(ptr, &header, 2);
    ptr += 2;

    // 帧序号（2字节）
    static uint16_t frame_seq = 1;
    uint32_t seq = frame_seq++;
    memcpy(ptr, &seq, 2);
    ptr += 2;

    // 源地址（4字节）
    uint32_t src_addr = address_to_little_endian(g_log_system.upload_config.source_addr);
    memcpy(ptr, &src_addr, 4);
    ptr += 4;

    // 目标地址（4字节）
    uint32_t dest_addr = address_to_little_endian(g_log_system.upload_config.dest_addr);
    memcpy(ptr, &dest_addr, 4);
    ptr += 4;

    // 数据类型（4字节）
    memcpy(ptr, &data_type, 4);
    ptr += 4;

    // 数据长度（3字节） + 数据标识（1字节）
    uint32_t data_len_field = data_length;
    memcpy(ptr, &data_len_field, 3);
    ptr += 3;

    *ptr = data_identifier;
    ptr += 1;

    // 帧数据
    memcpy(ptr, data, data_length);
    ptr += data_length;

    // 扩展标识（2字节）
    uint16_t ext_id = EXTENDED_IDENTIFIER;
    memcpy(ptr, &ext_id, 2);
    ptr += 2;

    // 计算校验和（从帧头到扩展标识）
    size_t checksum_data_length = ptr - *frame_data;
    uint16_t check_sum = calculate_checksum(*frame_data, checksum_data_length);
    memcpy(ptr, &check_sum, 2);
    ptr += 2;

    // 帧尾 （2字节）
    uint16_t footer = FRAME_FOOTER;
    memcpy(ptr, &footer, 2);

    return 0; 
}

// 构建文件信息帧
static int build_file_info_frame(const file_info_t *file_info, uint8_t **frame_data, size_t *frame_length)
{
    return build_log_upload_frame((const uint8_t*)file_info, sizeof(file_info_t),
                                    DATA_INFO_LOG_UPLAOD, DATA_IDENTIFIER_STRUCT,
                                    frame_data, frame_length);
}

// 构建日志文件数据帧
static int build_log_data_frame(const uint8_t *file_data, size_t data_length, uint8_t **frame_data, size_t * frame_length)
{
    return build_log_upload_frame(file_data, data_length, DATA_TYPE_LOG_UPLOAD,
                                    DATA_IDENTIFIER_FILE, frame_data, frame_length);
}
                                

static int read_log_file_content(uint8_t **file_data, size_t *file_length)
{
    FILE *file = fopen(g_log_system.log_file_path, "rb");
    if (!file) {
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to open log file for reading");
        return -1;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    *file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (*file_length == 0) {
        fclose(file);
        return -1;
    }

    // 分配内存
    *file_data = (uint8_t*)malloc(*file_length);
    if (!*file_data) {
        fclose(file);
        return -1;
    }

    // 读取文件内容
    size_t bytes_read = fread(*file_data, 1, *file_length, file);
    fclose(file);

    if (bytes_read != *file_length) {
        free(*file_data);
        return -1;
    }

    return 0;
}

static int create_tcp_connection()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to create socket");
        return -1;
    }

    // set timeout
    struct timeval timeout;
    timeout.tv_sec = g_log_system.upload_config.timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // set server ip addr
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_log_system.upload_config.server_port);

    if (inet_pton(AF_INET, g_log_system.upload_config.server_ip, &server_addr.sin_addr) <= 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "Invalid server IP address: %s", g_log_system.upload_config.server_ip);
        close(sockfd);
        return -1;
    }

    // connect server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to connect to server %s:%d: %s",
                    g_log_system.upload_config.server_ip,
                    g_log_system.upload_config.server_port,
                    strerror(errno));
        close(sockfd);
        return -1;
    }

    WTSL_LOG_INFO(MODULE_NAME, "Connect to TCP server: %s:%d",
                g_log_system.upload_config.server_ip,
                g_log_system.upload_config.server_port);

    return sockfd;
}

// 发送报文帧数据
static int send_frame_data(int sockfd, const uint8_t *frame_data, size_t frame_length)
{
    size_t total_sent = 0;

    while (total_sent < frame_length) {
        ssize_t bytes_sent = send(sockfd, frame_data + total_sent,
                                    frame_length - total_sent, 0);
        if (bytes_sent <= 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "Failed to send frame data");
            return -1;
        }
        total_sent += bytes_sent;
    }

    WTSL_LOG_INFO(MODULE_NAME, "Sent frame data: %zu bytes", total_sent);
    return 0;
}

#if 0
// send file real data
static int send_file_data(int sockfd, const char *file_path)
{
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        fprintf(stderr, "Failed to open file for reading: %s\n", strerror(errno));
        return -1;
    }

    char buffer[TCP_BUFFER_SIZE] = {0};
    ssize_t bytes_read = 0;
    size_t total_sent = 0;
    
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = send(sockfd, buffer, bytes_read, 0);
        if (bytes_sent != bytes_read) {
            fprintf(stderr, "Failed to send file data: %s\n", strerror(errno));
            close(file_fd);
            return -1;
        }
        total_sent += bytes_sent;
    }

    close(file_fd);

    if (bytes_read < 0) {
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        return -1;
    }

    printf("Sent file data: %zu bytes\n", total_sent);
    return 0;
}

// send file end flag
static int send_file_end(int sockfd)
{
    const char* end_marker = "FILE_END\n";
    if (send(sockfd, end_marker, strlen(end_marker), 0) != (ssize_t)strlen(end_marker)) {
        fprintf(stderr, "Failed to send file end marker: %s\n", strerror(errno));
        return -1;
    }

    printf("Sent file end marker\n");
    return 0;
}

// waiting server confirm
static int wait_for_ack(int sockfd, int timeout_seconds)
{
    fd_set read_fds;
    struct timeval timeout;
    char ack_buffer[32] = {0};

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    // use select wait data can read
    int result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (result == -1) {
        fprintf(stderr, "select error: %s\n", strerror(errno));
        return -1;
    } else if (result == 0) {
        printf("ACK wait timeout after %d seconds\n", timeout_seconds);
        return -1;
    }

    ssize_t bytes_received = recv(sockfd, ack_buffer, sizeof(ack_buffer) - 1, 0);

    if (bytes_received <= 0) {
        fprintf(stderr, "Failed to receive ACK from server: %s\n", strerror(errno));
        return -1;
    }

    ack_buffer[bytes_received] = '\0';

    // check confirm message
    if (strstr(ack_buffer, "ACK") != NULL) {
        printf("Received ACK from server: %s\n", ack_buffer);
        return 0;
    } else {
        printf("Invalid ACK from server: %s\n", ack_buffer);
        return -1;
    }
}

// send file header info
static int send_file_header(int sockfd, const char *filename, size_t file_size)
{
    char header[256] = {0};

    // format: FILE_START:filename:file_size
    int header_len = snprintf(header, sizeof(header), "FILE_START:%s:%zu\n", filename, file_size);

    if (send(sockfd, header, header_len, 0) != header_len) {
        fprintf(stderr, "Failed to send file header: %s\n", strerror(errno));
        return -1;
    }

    printf("Send file header: %s\n", header);
    return 0;
}
#else
#endif

static void prepare_file_info(file_info_t *file_info, size_t file_size)
{
    memset(file_info, 0, sizeof(file_info_t));

    const char *log_filename = strrchr(g_log_system.log_file_path, '/');
    const char *source_name = log_filename ? (log_filename + 1) : g_log_system.log_file_path;

    int result = snprintf(file_info->filename,
                            sizeof(file_info->filename),
                            "%s", source_name);

    if (result < 0) {
        strcpy(file_info->filename, "unknown.log");
        WTSL_LOG_DEBUG(MODULE_NAME, "Error copying file name");
    } else if ((size_t)result >= sizeof(file_info->filename)) {
        WTSL_LOG_DEBUG(MODULE_NAME, "file name truncated from '%s' to '%s'",
                source_name, file_info->filename);
    }

    file_info->file_size = file_size;
    file_info->is_dir = false;

    WTSL_LOG_DEBUG(MODULE_NAME, "File info prepared: name=%s, size=%zu, is_dir=%d",
            file_info->filename, file_info->file_size, file_info->is_dir);
}

void print_hex(unsigned char *buffer, int length){
    for (int i = 0; i < length; i++) {
        printf("%02X ", (unsigned char)buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

// waiting server confirm
static int wait_for_ack(int sockfd, int timeout_seconds, const char *ACK_verify)
{
    fd_set read_fds;
    struct timeval timeout;
    char ack_buffer[1024] = {0};

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    // use select wait data can read
    int result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (result == -1) {
        WTSL_LOG_DEBUG(MODULE_NAME, "select error: %s", strerror(errno));
        return -1;
    } else if (result == 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "ACK wait timeout after %d seconds", timeout_seconds);
        return -1;
    }

    ssize_t bytes_received = recv(sockfd, ack_buffer, sizeof(ack_buffer) - 1, 0);

    if (bytes_received <= 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Failed to receive ACK from server: %s", strerror(errno));
        return -1;
    }

    ack_buffer[bytes_received] = '\0';

    // check confirm message
    if (strstr(ack_buffer, ACK_verify) != NULL) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Received ACK from server: %s", ack_buffer);
        return 0;
    } else {
        WTSL_LOG_DEBUG(MODULE_NAME, "Received %lld bytes in hex:", (long long)bytes_received);
        return -1;
    }
}


// TCP upload log file
static int tcp_upload_logs(const char *file_path)
{
    WTSL_LOG_DEBUG(MODULE_NAME, "Starting TCP upload of log file: %s", file_path);

    // read log file text
    uint8_t *file_data = NULL;
    size_t file_length = 0;

    if (read_log_file_content(&file_data, &file_length) != 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Failed to read log file content");
        return -1;
    }

    // 准备文件信息报文
    file_info_t file_info;
    prepare_file_info(&file_info, file_length);

    int retry_count = 0;
    int upload_result = -1;

    while (retry_count < UPLOAD_RETRY_COUNT && upload_result != 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Upload attempt %d/%d", retry_count + 1, UPLOAD_RETRY_COUNT);

        int sockfd = create_tcp_connection();
        if (sockfd < 0) {
            retry_count++;
            if (retry_count < UPLOAD_RETRY_COUNT) {
                WTSL_LOG_DEBUG(MODULE_NAME, "Waiting %d seconds before retry...", g_log_system.upload_config.reconnect_interval);
                sleep(g_log_system.upload_config.reconnect_interval);
            }
            continue;
        }

        // 首先发送文件信息帧
        uint8_t *info_frame_data = NULL;
        size_t info_frame_length = 0;

        if (build_file_info_frame(&file_info, &info_frame_data, &info_frame_length) != 0) {
            WTSL_LOG_DEBUG(MODULE_NAME, "Failed to build file info frame");
            close(sockfd);
            retry_count++;
            continue;
        }

        WTSL_LOG_DEBUG(MODULE_NAME, "Sending file info frame (%zu bytes)...", info_frame_length);
        // 发送报文帧
        if (send_frame_data(sockfd, info_frame_data, info_frame_length) != 0) {
            WTSL_LOG_DEBUG(MODULE_NAME, "Failed to send file info frame");
            free(info_frame_data);
            close(sockfd);
            retry_count++;
            continue;
        }

        // print_hex(info_frame_data, info_frame_length);
        free(info_frame_data);
        WTSL_LOG_DEBUG(MODULE_NAME, "File info frame sent successfully");

        int ack_timeout = 10;
        if (wait_for_ack(sockfd, ack_timeout, ACK_INFO_VERIFY) != 0) {
            retry_count++;
            continue;
        }

        usleep(100000);

        uint8_t *data_frame_data = NULL;
        size_t data_frame_length = 0;

        if (build_log_data_frame(file_data, file_length, &data_frame_data, &data_frame_length) != 0) {
            WTSL_LOG_DEBUG(MODULE_NAME, "Failed to build log data frame");
            close(sockfd);
            retry_count++;
            continue;
        }

        WTSL_LOG_DEBUG(MODULE_NAME, "Sending log data frame (%zu bytes, file size %zu bytes)...", data_frame_length, file_length);
        if (send_frame_data(sockfd, data_frame_data, data_frame_length)) {
            WTSL_LOG_DEBUG(MODULE_NAME, "Failed to send log data frame");
            free(data_frame_data);
            close(sockfd);
            retry_count++;
            continue;
        }

        usleep(100000);
        // print_hex(data_frame_data, data_frame_length);
        free(data_frame_data);
        WTSL_LOG_DEBUG(MODULE_NAME, "Log data frame sent successfully");

        // wait server ack
        if (wait_for_ack(sockfd, ack_timeout, ACK_FILEDATA_VERIFY) != 0) {
            retry_count++;
            continue;
        }
        // upload success
        upload_result = 0;
        close(sockfd);
    }

    free(file_data);

    if (upload_result == 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "TCP upload successful: file size %zu bytes", file_length);
    } else {
        WTSL_LOG_DEBUG(MODULE_NAME, "TCP upload failed after %d attempts", UPLOAD_RETRY_COUNT);
    }

    return upload_result;
}

// exec log upload and clearup
int upload_and_clear_logs()
{
    if (!g_log_system.log_file) return -1;

    // close current file
    fclose(g_log_system.log_file);
    g_log_system.log_file = NULL;

    // callback TCP upload function
    int upload_result = tcp_upload_logs(g_log_system.log_file_path);

    // update current filename
    char *filename = generate_file_name();
    if (filename) {
        strcpy(g_log_system.log_file_path, filename);
        free(filename);
    } else {
        WTSL_LOG_DEBUG(MODULE_NAME, "Failed to generate log file path. use default log file");
        strcpy(g_log_system.log_file_path, DEFAULT_LOG_FILE);
    }

    // Empty the log file and start over whether the upload is successful or not
    g_log_system.log_file = fopen(g_log_system.log_file_path, "w");
    if (!g_log_system.log_file) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Failed to recreate log file");
        return -1;
    }

    // log Upload event
    char time_buffer[64] = {0};
    format_time_with_ms(time_buffer, sizeof(time_buffer));

    fprintf(g_log_system.log_file, "[%s][%s][SYSTEM][UPLOAD] : log file uploaded via TCP. Upload result: %s\n",
                time_buffer, g_log_system.app_name,
                upload_result == 0 ? "SUCCESS" : "FAILED");
    fflush(g_log_system.log_file);

    WTSL_LOG_INFO(MODULE_NAME, "Log file upload via TCP. Upload result: %s",
                upload_result == 0 ? "SUCCESS" : "FAILED");
    
    return upload_result;
}

// backstage upload thread function
void* upload_thread_func(void *arg)
{
    (void)arg;

    WTSL_LOG_INFO(MODULE_NAME, "Upload thread started");

    while (g_log_system.upload_thread_running) {
        pthread_mutex_lock(&g_log_system.config_mutex);

        // wait trigger or timeout
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        _sec += UPLOAD_CHECK_INTERVAL;

        pthread_cond_timedwait(&g_log_system.upload_cond, &g_log_system.config_mutex, &ts);

        int should_upload = g_log_system.trigger_upload;
        g_log_system.trigger_upload = 0;

        pthread_mutex_unlock(&g_log_system.config_mutex);

        // check is not should upload
        if (should_upload || need_upload_and_clear()) {
            upload_and_clear_logs();
        }
    }

    WTSL_LOG_INFO(MODULE_NAME, "Upload thread stopped");
    return NULL;
}

// Manual triggering of log upload
int wtsl_logger_upload_logs()
{
    pthread_mutex_lock(&g_log_system.config_mutex);
    g_log_system.trigger_upload = 1;
    pthread_cond_signal(&g_log_system.upload_cond);
    pthread_mutex_unlock(&g_log_system.config_mutex);
    return 0;
}

// set max file size
void wtsl_logger_set_max_file_size(size_t max_size)
{
    pthread_mutex_lock(&g_log_system.config_mutex);
    g_log_system.max_file_size = max_size;
    pthread_mutex_unlock(&g_log_system.config_mutex);
}

// get current log file size 
size_t wtsl_logger_get_current_file_size()
{
    return get_file_size(g_log_system.log_file_path);
}

// set TCP UPLOAD config
void wtsl_logger_set_upload_config_ex(const char *server_ip, int server_port,
                                    uint16_t src_subnet, uint8_t src_tnode, uint8_t src_device,
                                    uint16_t dest_subnet, uint8_t dest_tnode, uint8_t dest_device,
                                    int timeout, int reconnect_interval)
{
    pthread_mutex_lock(&g_log_system.config_mutex);

    if (server_ip) {
        strncpy(g_log_system.upload_config.server_ip, server_ip, sizeof(g_log_system.upload_config.server_ip) - 1);
    }

    g_log_system.upload_config.server_port = server_port;

    // 设置源地址
    g_log_system.upload_config.source_addr.reserved = 0;
    g_log_system.upload_config.source_addr.subnet_id = src_subnet;
    g_log_system.upload_config.source_addr.t_node_id = src_tnode;
    g_log_system.upload_config.source_addr.device_id = src_device;

    // 设置目标地址
    g_log_system.upload_config.dest_addr.reserved = 0;
    g_log_system.upload_config.dest_addr.subnet_id = dest_subnet;
    g_log_system.upload_config.dest_addr.t_node_id = dest_tnode;
    g_log_system.upload_config.dest_addr.device_id = dest_device;

    g_log_system.upload_config.timeout_seconds = timeout;
    g_log_system.upload_config.reconnect_interval = reconnect_interval;

    pthread_mutex_unlock(&g_log_system.config_mutex);

    WTSL_LOG_DEBUG(MODULE_NAME, "TCP_upload_configuration updated:");
    WTSL_LOG_DEBUG(MODULE_NAME, "    Server: %s:%d",
            g_log_system.upload_config.server_ip, g_log_system.upload_config.server_port);
    WTSL_LOG_DEBUG(MODULE_NAME, "    Source Address: subnet=%d, t_node=%d, device=%d",
            src_subnet, src_tnode, src_device);
    WTSL_LOG_DEBUG(MODULE_NAME, "    Destination Address: subnet=%d, t_node=%d, device=%d",
            dest_subnet, dest_tnode, dest_device);
    WTSL_LOG_DEBUG(MODULE_NAME, "    Timeout: %d seconds", g_log_system.upload_config.timeout_seconds);
    WTSL_LOG_DEBUG(MODULE_NAME, "    Reconnect Interval: %d seconds", g_log_system.upload_config.reconnect_interval);
}

void wtsl_logger_set_upload_config(const char* server_ip, int server_port,
                                    int timeout, int reconnect_interval)
{
    // 使用默认地址配置
    wtsl_logger_set_upload_config_ex(server_ip, server_port,
                                    101, 16, 16,
                                    102, 16, 16,
                                    timeout, reconnect_interval);
}

// test TCP connect
int wtsl_logger_test_connection()
{
    WTSL_LOG_DEBUG(MODULE_NAME, "Testing TCP connection to %s:%d...",
            g_log_system.upload_config.server_ip, g_log_system.upload_config.server_port);

    int sockfd = create_tcp_connection();
    if (sockfd < 0) {
        WTSL_LOG_DEBUG(MODULE_NAME, "Connection test FAILED");
        return -1; 
    }

    close(sockfd);
    WTSL_LOG_DEBUG(MODULE_NAME, "Connection test SUCCESS");
    return 0;
}
