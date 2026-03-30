#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <cjson/cJSON.h>
#include <sys/utsname.h>
// #include <openssl/md5.h>

#include "wtsl_core_dataparser_ota.h"
#include "wtsl_log_manager.h"


#define MODULE_NAME "parse_ota"

static const char* get_md5_by_ko_name(const firmware_header_t *header, const char *ko_name)
{
    for (int i = 0; i < 4; i++) {
        if (header->ko_files[i].wt_koname[0] != '\0' &&
            strcmp(header->ko_files[i].wt_koname, ko_name) == 0) {
                return header->ko_files[i].md5;
        }
    }
    return NULL;
}

static int verify_single_ko_file(const char *ko_file_path, const char *expected_md5)
{
    char actual_md5[33] = {0};

    if (access(ko_file_path, F_OK) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "ko file not exist: %s",ko_file_path);
        return -1;
    }

    if (calculate_file_md5(ko_file_path, actual_md5) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "calculate file %s md5sum failed", ko_file_path);
        return -1;
    }

    WTSL_LOG_INFO(MODULE_NAME, "ko file: %s", ko_file_path);
    WTSL_LOG_INFO(MODULE_NAME, "expected_md5: %s", expected_md5);
    WTSL_LOG_INFO(MODULE_NAME, "actual_md5: %s", actual_md5);

    if (strcasecmp(actual_md5, expected_md5) == 0) {
        WTSL_LOG_INFO(MODULE_NAME, "%s file md5 sum verify success!", ko_file_path);
        return 0;
    } else {
        WTSL_LOG_ERROR(MODULE_NAME, "%s file md5 sum verify failed!", ko_file_path);
        return -1;
    }
}

static const char* get_build_version(const firmware_header_t *header)
{
    if (header == NULL) {
        return NULL;
    }

    char *last_b = strchr(header->version, 'B');
    if (last_b != NULL) {
        return last_b + 1;
    }
    return NULL;
}

static int upgrade_hi_firmware(const firmware_header_t *header, const char *extract_dir, const char *version_number)
{
    (void)header;
    int ret = -1;
    char firmware_dir[UPGRADE_PATH_SIZE] = {0};

    snprintf(firmware_dir, sizeof(firmware_dir), "%s/firmware_t%s", extract_dir, version_number);

    if (access(firmware_dir, F_OK) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "%s not exist", firmware_dir);
        return -1;
    } else {
        char exec_cmd[CMD_SIZE] = {0};
        snprintf(exec_cmd, sizeof(exec_cmd), "rm -rf /home/firmware_t%s", version_number);
        ret = system(exec_cmd);
    }

    char copy_cmd[CMD_SIZE] = {0};
    snprintf(copy_cmd, sizeof(copy_cmd), "cp -rf %s/firmware_t%s /home/wt", extract_dir, version_number);
    ret |= system(copy_cmd);

    char ln_cmd[CMD_SIZE] = {0};
    snprintf(ln_cmd, sizeof(ln_cmd), "rm -rf /home/firmware && ln -s /home/wt/firmware_t%s /home/wt/firmware", version_number);
    ret |= system(ln_cmd);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);
    }
    return 0;
}

static int upgrade_web_files_in_background(const firmware_header_t *header, const char *extract_dir)
{
    int ret = -1;
    (void)header;
    char www_dir[UPGRADE_PATH_SIZE] = {0};
    char current_www_dir[UPGRADE_PATH_SIZE] = {0};
    char upgrade_www_dir[UPGRADE_PATH_SIZE] = {0};
    char httpd_exec_dir[UPGRADE_PATH_SIZE] = {0};

    snprintf(www_dir, sizeof(www_dir), "%s/www", extract_dir);
    snprintf(current_www_dir, sizeof(current_www_dir), "/home/wt/upload_file");

    if (access(current_www_dir, F_OK) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "%s not exist", current_www_dir);
    } else {
        char exec_cmd[CMD_SIZE] = {0};
        snprintf(exec_cmd, sizeof(exec_cmd), "rm -rf %s", current_www_dir);
        WTSL_LOG_INFO(MODULE_NAME, "exec command :%s ",exec_cmd);
        ret = system(exec_cmd);
    }

    snprintf(upgrade_www_dir, sizeof(upgrade_www_dir), "/tmp/upload_file/www");
    if (access(upgrade_www_dir, F_OK) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "%s not exist, upload path not include www", upgrade_www_dir);
        return -1;
    } else {
        char copy_cmd[CMD_SIZE] = {0};
        snprintf(copy_cmd, sizeof(copy_cmd), "cp -rf %s /home/wt/", upgrade_www_dir);
        WTSL_LOG_INFO(MODULE_NAME, "exec command :%s ",copy_cmd);
        ret |= system(copy_cmd);
    }

    char www_service_cmd[CMD_SIZE] = {0};
    snprintf(httpd_exec_dir, sizeof(httpd_exec_dir), "/home/wt/www");
    if (access(httpd_exec_dir, F_OK) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "%s not exist, startup httpd www failed!", httpd_exec_dir);
        return -1;
    } else {
        snprintf(www_service_cmd, sizeof(www_service_cmd), "killall httpd && httpd -h %s", httpd_exec_dir);
        WTSL_LOG_INFO(MODULE_NAME, "exec command :%s ",www_service_cmd);
        ret |= system(www_service_cmd);
    }
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);
    }

    WTSL_LOG_INFO(MODULE_NAME, "upgrade web service page success");
    return 0;
}

static int perform_app_upgrade(const firmware_header_t *header, const char *extract_dir)
{
    (void)header;
    int ret = -1;
    char app_dir[UPGRADE_PATH_SIZE] = {0};
    char new_binary_path[UPGRADE_PATH_SIZE] = {0};
    char current_binary_path[UPGRADE_PATH_SIZE] = "/home/wt/app/wtsl_app";
    char backup_binary_path[UPGRADE_PATH_SIZE] = "/home/wt/app/wtsl_app.backup";

    char lib_dir[UPGRADE_PATH_SIZE] = "/home/wt/app";
    char backup_lib_dir[UPGRADE_PATH_SIZE] = "/home/wt/app/lib.backup";

    snprintf(app_dir, sizeof(app_dir), "%s/app", extract_dir);
    snprintf(new_binary_path, sizeof(new_binary_path), "%s/wtsl_app", app_dir);

    // check new version file
    if (access(new_binary_path, F_OK) != 0) {
        WTSL_LOG_WARNING(MODULE_NAME, "new version app file not exist!");
        return -1;
    }

    // check library file
    char lib_wtsl_core[UPGRADE_PATH_SIZE] = {0};
    char lib_cjson[UPGRADE_PATH_SIZE] = {0};
    char lib_microhttpd[UPGRADE_PATH_SIZE] = {0};
    snprintf(lib_wtsl_core, sizeof(lib_wtsl_core), "%s/libwtsl_core.so", app_dir);
    snprintf(lib_cjson, sizeof(lib_cjson), "%s/libcjson.so", app_dir);
    snprintf(lib_microhttpd, sizeof(lib_microhttpd), "%s/libmicrohttpd.so", app_dir);

    int has_lib_wtsl_core = (access(lib_wtsl_core, F_OK) == 0);
    int has_lib_cjson = (access(lib_wtsl_core, F_OK) == 0);
    int has_lib_microhttpd = (access(lib_wtsl_core, F_OK) == 0);

    chmod(new_binary_path, 0755);

    WTSL_LOG_INFO(MODULE_NAME, "start backup curent version...");

    // backup main process
    char backup_cmd[CMD_SIZE] = {0};
    snprintf(backup_cmd, sizeof(backup_cmd), "cp %s %s", current_binary_path, backup_binary_path);
    ret = system(backup_cmd);

    // backup library file path
    snprintf(backup_cmd, sizeof(backup_cmd), "mkdir -p %s && cp -r %s/*.so %s", backup_lib_dir, lib_dir, backup_lib_dir);
    ret |= system(backup_cmd);

    // wait parent process Finished response
    sleep(3);

    WTSL_LOG_INFO(MODULE_NAME, "stop current service...");

    char stop_cmd[CMD_SIZE] = {0};
    snprintf(stop_cmd, sizeof(stop_cmd),
        "ps | grep wtsl_app | grep -v %d | grep -v grep | awk '{print $1}' | xargs kill -TERM 2>/dev/null",
        getpid());
    ret |= system(stop_cmd);

    // wait process stop
    sleep(2);

    // if processs don't stop, Forced kill
    snprintf(stop_cmd, sizeof(stop_cmd),
        "ps | grep wtsl_app | grep -v %d | grep -v grep | awk '{print $1}' | xargs kill -KILL 2>/dev/null",
        getpid());
    ret |= system(stop_cmd);

    sleep(1);

    // replace main process
    char replace_cmd[CMD_SIZE] = {0};
    snprintf(replace_cmd, sizeof(replace_cmd), "cp %s %s", new_binary_path, current_binary_path);
    int replace_result = system(replace_cmd);

    if (replace_result != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "error: main process replace failed!");
        goto rollback;
    }

    chmod(current_binary_path, 0755);

    if (has_lib_cjson) {
        char lib_replace_cmd[CMD_SIZE] = {0};
        snprintf(lib_replace_cmd, sizeof(lib_replace_cmd), "cp %s %s/libcjson.so", lib_cjson, lib_dir);
        if (system(lib_replace_cmd) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "error: replace libcsjon.so failed!");
            goto rollback;
        }
        WTSL_LOG_INFO(MODULE_NAME, "replace libcjson.so");
    }

    if (has_lib_wtsl_core) {
        char lib_replace_cmd[CMD_SIZE] = {0};
        snprintf(lib_replace_cmd, sizeof(lib_replace_cmd), "cp %s %s/libwtsl_core.so", lib_wtsl_core, lib_dir);
        if (system(lib_replace_cmd) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "error: replace libwtsl_core.so failed!");
            goto rollback;
        }
        WTSL_LOG_INFO(MODULE_NAME, "replace libwtsl_core.so");
    }

    if (has_lib_microhttpd) {
        char lib_replace_cmd[CMD_SIZE] = {0};
        snprintf(lib_replace_cmd, sizeof(lib_replace_cmd), "cp %s %s/libmicrohttpd.so", lib_microhttpd, lib_dir);
        if (system(lib_replace_cmd) != 0) {
            WTSL_LOG_ERROR(MODULE_NAME, "error: replace libmicrohttpd.so failed!");
            goto rollback;
        }
        WTSL_LOG_INFO(MODULE_NAME, "replace libmicrohttpd.so");
    }

    ret |= system("sync");

    // start new service
    char start_cmd[CMD_SIZE] = {0};
    snprintf(start_cmd, sizeof(start_cmd), "LD_LIBRARY_PATH=%s %s &", lib_dir, current_binary_path);
    int start_result = system(start_cmd);
    if (start_result != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "error: startup new service failed!");
        goto rollback;
    }

    sleep(3);

    WTSL_LOG_INFO(MODULE_NAME, "appclition upgrade success...");
    
    char cleanup_cmd[CMD_SIZE] = {0};
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", extract_dir);
    ret |= system(cleanup_cmd);
    if(ret != 0)
        WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d]error: startup new service failed!",__FUNCTION__,__LINE__);

    WTSL_LOG_INFO(MODULE_NAME, "backup file and path:");
    WTSL_LOG_INFO(MODULE_NAME, "    main process: %s", backup_binary_path);
    WTSL_LOG_INFO(MODULE_NAME, "    library file: %s", backup_lib_dir);
    return 0;

rollback:

    // rollback
    WTSL_LOG_INFO(MODULE_NAME, "exec rollbacking..."); 
    snprintf(replace_cmd, sizeof(replace_cmd), "cp %s %s", backup_binary_path, current_binary_path);
    ret |= system(replace_cmd);

    // recovery library file
    char restore_lib_cmd[CMD_SIZE] = {0};
    snprintf(restore_lib_cmd, sizeof(restore_lib_cmd), "cp -r %s/* %s/", backup_lib_dir, lib_dir);
    ret |= system(restore_lib_cmd);

    // restart service
    ret |= system(start_cmd);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d],ret:%d",__FUNCTION__,__LINE__,ret);
    }
    return -1;
}

static void log_upgrade_status(const char *status, const firmware_header_t *header, int has_app, int has_www, int has_firmware) {
    FILE *log = fopen("/tmp/upgrade_status.log", "a");
    if (log) {
        time_t now = time(NULL);
        WTSL_LOG_INFO(MODULE_NAME, "[%s] %s - PID: %d - Process: %s - Version: %s - App: %s - www: %s - firmware_t%s: %s",
                ctime(&now), status, getpid(), header->process, header->version,
                has_app ? "YES" : "NO", has_www ? "YES" : "NO", get_build_version(header), has_firmware ? "YES" : "NO");
        fclose(log);
    }
}

static void check_app_directory(const char *app_dir)
{
    int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "========= check app path text =========");

    // check main process wtsl_app
    char main_binary[UPGRADE_PATH_SIZE] = {0};
    snprintf(main_binary, sizeof(main_binary), "%s/wtsl_app", app_dir);
    if (access(main_binary, F_OK) == 0) {
        WTSL_LOG_INFO(MODULE_NAME, "wtsl_app binary exist!");

        // get file info
        struct stat st;
        if (stat(main_binary, &st) == 0) {
            WTSL_LOG_INFO(MODULE_NAME, "size: %lld bytes, Permissions: %o", st.st_size, st.st_mode & 0777);
        }
    } else {
        WTSL_LOG_WARNING(MODULE_NAME, "wtsl_app binary not exist!");
    }

    // check library files
    char *lib_files[] = {
        "libwtsl_core.so",
        "libcjson.so",
        "libmicrohttpd.so",
        NULL
    };

    for (int i = 0; lib_files[i] != NULL; i++) {
        char lib_path[UPGRADE_PATH_SIZE] = {0};
        snprintf(lib_path, sizeof(lib_path), "%s/%s", app_dir, lib_files[i]);

        if (access(lib_path, F_OK) == 0) {
            WTSL_LOG_INFO(MODULE_NAME, " %s exist", lib_files[i]);

            struct stat st;
            if (stat(lib_path, &st) == 0) {
                WTSL_LOG_INFO(MODULE_NAME, " size: %lld bytes", st.st_size);
            }
        } else {
            WTSL_LOG_WARNING(MODULE_NAME, " %s not exist", lib_files[i]);
        }
    }

    // list all files
    char ls_cmd[CMD_SIZE] = {0};
    snprintf(ls_cmd, sizeof(ls_cmd), "ls -la %s", app_dir);
    ret = system(ls_cmd);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME,"");
    }
}

// 解析固件头信息 - 适配新的JSON格式
int parse_firmware_header(const char *json_data, firmware_header_t *header) {
    WTSL_LOG_INFO(MODULE_NAME, "parse JSON data: %.200s...", json_data);
    
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            WTSL_LOG_ERROR(MODULE_NAME, "JSON parse error pos: %s", error_ptr);
        }
        return -1;
    }
    
    // 解析各个字段
    cJSON *filename = cJSON_GetObjectItem(root, "filename");
    cJSON *md5sum = cJSON_GetObjectItem(root, "md5sum");
    cJSON *filesize = cJSON_GetObjectItem(root, "filesize");
    cJSON *version = cJSON_GetObjectItem(root, "version");
    cJSON *filetype = cJSON_GetObjectItem(root, "filetype");
    cJSON *type = cJSON_GetObjectItem(root, "firmwaretype");
    cJSON *up_method = cJSON_GetObjectItem(root, "up_method");
    cJSON *process = cJSON_GetObjectItem(root, "process");
    cJSON *targetarch = cJSON_GetObjectItem(root, "targetarch");

    cJSON *ko_md5 = cJSON_GetObjectItem(root, "ko_md5sum");
    
    // 设置默认值并复制数据
    memset(header, 0, sizeof(firmware_header_t));
    
    // 必需字段检查
    int missing_fields = 0;
    
    if (filename && cJSON_IsString(filename)) {
        strncpy(header->filename, filename->valuestring, sizeof(header->filename)-1);
    } else {
        WTSL_LOG_ERROR(MODULE_NAME, "missing or invalid filename field");
        missing_fields++;
    }
    
    if (md5sum && cJSON_IsString(md5sum)) {
        strncpy(header->md5sum, md5sum->valuestring, sizeof(header->md5sum)-1);
    } else {
        WTSL_LOG_ERROR(MODULE_NAME, "missing or invalid md5sum field");
        missing_fields++;
    }

    if (filesize && cJSON_IsString(filesize)) {
        strncpy(header->filesize, filesize->valuestring, sizeof(header->filesize)-1);
    } else {
        WTSL_LOG_ERROR(MODULE_NAME, "missing or invalid filesize field");
        missing_fields++;
    }
    
    // 可选字段
    if (version && cJSON_IsString(version)) {
        strncpy(header->version, version->valuestring, sizeof(header->version)-1);
    }
    
    if (filetype && cJSON_IsString(filetype)) {
        strncpy(header->filetype, filetype->valuestring, sizeof(header->filetype)-1);
    }
    
    if (type && cJSON_IsString(type)) {
        strncpy(header->firmwaretype, type->valuestring, sizeof(header->firmwaretype)-1);
    }
    
    if (up_method && cJSON_IsString(up_method)) {
        strncpy(header->up_method, up_method->valuestring, sizeof(header->up_method)-1);
    }
    
    if (process && cJSON_IsString(process)) {
        strncpy(header->process, process->valuestring, sizeof(header->process)-1);
    }

    if (targetarch && cJSON_IsString(targetarch)) {
        strncpy(header->targetarch, targetarch->valuestring, sizeof(header->targetarch)-1);
    }

    if (ko_md5 && cJSON_IsObject(ko_md5)) {
        int ko_index = 0;
        cJSON *ko_item = NULL;

        // 遍历ko_md5 对象的所有键值对
        cJSON_ArrayForEach(ko_item, ko_md5) {
            if (ko_index >= 4) {
                WTSL_LOG_WARNING(MODULE_NAME, "warning: ko_md5 Object contains more than four ko files and only the first 4 will be processed.");
                break;
            }

            if (cJSON_IsString(ko_item) && ko_item->string && ko_item->valuestring) {
                strncpy(header->ko_files[ko_index].wt_koname, ko_item->string,
                    sizeof(header->ko_files[ko_index].wt_koname)-1);
                
                strncpy(header->ko_files[ko_index].md5, ko_item->valuestring,
                    sizeof(header->ko_files[ko_index].md5)-1);

                ko_index++;
            }
        }

        if (ko_index == 0) {
            WTSL_LOG_WARNING(MODULE_NAME, "Warning: ko_md5 Object is empty or malformed.");
        } else {
            WTSL_LOG_INFO(MODULE_NAME, "success parser %d ko files md5sum info.", ko_index);
        }
    } else {
        WTSL_LOG_WARNING(MODULE_NAME, "Warning: Missing or invalid ko field.");
    }
    
    cJSON_Delete(root);
    
    if (missing_fields > 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "missing %d Required fields", missing_fields);
        return -1;
    }
    
    return 0;
}

// 计算文件的MD5值
int calculate_file_md5(const char *filename, char *md5_sum) {
    char command[512] = {0};
    FILE *fp;
    char result[64] = {0};
    // 使用MD5sum命令
    snprintf(command, sizeof(command), "md5sum '%s' | cut -d ' ' -f1", filename);
    
    fp = popen(command, "r");
    if (fp == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "unable to execute md5sum command");
        return -1;
    }

    if (fgets(result, sizeof(result), fp) == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "unable read md5sum command");
        pclose(fp);
        return -1;
    }

    pclose(fp);

    // 移除换行符
    result[strcspn(result, "\n")] = '\0';

    if (strlen(result) != 32) {
        WTSL_LOG_ERROR(MODULE_NAME, "invalid MD5 result: %s", result);
        return -1;
    }

    strcpy(md5_sum, result);
    WTSL_LOG_INFO(MODULE_NAME, " MD5: %s -> %s",filename, md5_sum);
    return 0;
}

// 执行系统命令并获取返回值
int execute_command(const char *command)
{
    WTSL_LOG_INFO(MODULE_NAME, "execute command : %s", command);
    int status = system(command);
    if (status == -1) {
        WTSL_LOG_ERROR(MODULE_NAME, "failed to execute command: %s", command);
        return -1;
    }

    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        WTSL_LOG_INFO(MODULE_NAME, "command exited with status: %d", exit_status);
        return exit_status;
    } else {
        WTSL_LOG_ERROR(MODULE_NAME, "Comand did not exit normally");
        return -1;
    }
}

// 使用系统tar命令解压文件
static int extract_tar_gz(const char *tar_gz_file, const char *extract_dir)
{
    char command[512] = {0};

    snprintf(command, sizeof(command), "mkdir -p %s", extract_dir);
    if (execute_command(command) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "failed to create extract directory: %s", extract_dir);
        return -1;
    }

    snprintf(command, sizeof(command), "tar -xzf %s --strip-components 2 -C %s", tar_gz_file, extract_dir);
    if (execute_command(command) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "failed to extract tar.gz file: %s", tar_gz_file);
        return -1;
    }

    WTSL_LOG_INFO(MODULE_NAME, "Successfully extracted %s to %s", tar_gz_file, extract_dir);
    return 0;
}

/**
 * Platform Architecture Verify
 */
int Platform_verify(firmware_header_t *header) {
    // 参数校验
    if (header->targetarch == NULL) {
        return -1;
    }

    // 接受不同平台架构：arm 、arm64、x86、x86_64
    int want_arm32 = (strcmp(header->targetarch, "arm") == 0);
    int want_arm64 = (strcmp(header->targetarch, "arm64") == 0);

    if (!want_arm32 && !want_arm64) {
        return -1;
    }

    // 获取系统架构信息
    struct utsname un;
    if (uname(&un) != 0) {
        return -1;
    }

    const char *machine = un.machine;

    // 判断实际架构
    int is_arm32 = (strncmp(machine, "armv", 4) == 0); // 匹配 armv6l, armv7l 等
    int is_arm64 = (strcmp(machine, "arm64") == 0);

    // 比对期望与实际
    if (want_arm32 && is_arm32) {
        return 0;
    }

    if (want_arm64 && is_arm64) {
        return 0;
    }

    return -1; // 不匹配
}


// 提取并验证固件
int extract_and_verify_firmware(const char *upload_file, const firmware_header_t *header) {

    char md5_calculated[33] = {0};
    
    // 计算MD5并比较
    if (calculate_file_md5(upload_file, md5_calculated) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "MD5 SUM FAILED : %s", upload_file);
        return -1;
    }
    
    WTSL_LOG_INFO(MODULE_NAME, "header MD5  : %s", header->md5sum);
    WTSL_LOG_INFO(MODULE_NAME, "MD5sum      : %s", md5_calculated);
    
    if (strcasecmp(header->md5sum, md5_calculated) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "MD5 verify failed!");
        // 清理临时文件
        // unlink(upload_file);
        return -1;
    }
    
    WTSL_LOG_INFO(MODULE_NAME, "MD5 verify successfully!");
    
    // 解压固件包
    char extract_dir[UPGRADE_PATH_SIZE] = {0};
    snprintf(extract_dir, sizeof(extract_dir), "/tmp/upload_file");
    
    WTSL_LOG_INFO(MODULE_NAME, "extract firmware to: %s", extract_dir);
    if (extract_tar_gz(upload_file, extract_dir) != 0) {
        WTSL_LOG_ERROR(MODULE_NAME, "firmware extract failed!");
        // 清理临时文件
        // unlink(upload_file);
        return -1;
    }

    char firmware_dir[UPGRADE_PATH_SIZE] = {0};
    char gt_gf61_ko_path[UPGRADE_PATH_SIZE] = {0};
    char hi_cipherserver_ko_path[UPGRADE_PATH_SIZE] = {0};
    char ksecurec_ko_path[UPGRADE_PATH_SIZE] = {0};
    char plat_gf61_ko_path[UPGRADE_PATH_SIZE] = {0};
    snprintf(firmware_dir, sizeof(firmware_dir), "%s/firmware_t%s", extract_dir, get_build_version(header));
    snprintf(gt_gf61_ko_path,sizeof(gt_gf61_ko_path), "%s/gt_gf61.ko", firmware_dir);
    snprintf(hi_cipherserver_ko_path,sizeof(hi_cipherserver_ko_path), "%s/hi_cipherserver.ko", firmware_dir);
    snprintf(ksecurec_ko_path,sizeof(ksecurec_ko_path), "%s/ksecurec.ko", firmware_dir);
    snprintf(plat_gf61_ko_path,sizeof(plat_gf61_ko_path), "%s/plat_gf61.ko", firmware_dir);

    int has_gt_gf61_ko_path = (access(gt_gf61_ko_path, F_OK) == 0);
    int has_hi_cipherserver_ko_path = (access(hi_cipherserver_ko_path, F_OK) == 0);
    int has_ksecurec_ko_path = (access(ksecurec_ko_path, F_OK) == 0);
    int has_plat_gf61_ko_path = (access(plat_gf61_ko_path, F_OK) == 0);

    if (has_gt_gf61_ko_path && 
        has_hi_cipherserver_ko_path && 
        has_ksecurec_ko_path && 
        has_plat_gf61_ko_path) {

        WTSL_LOG_INFO(MODULE_NAME, "start verify sdk build ko file md5 verify......");
        int md5_verified = 1;
        md5_verified &= (verify_single_ko_file(gt_gf61_ko_path, get_md5_by_ko_name(header, "gt_gf61.ko")) == 0);
        md5_verified &= (verify_single_ko_file(hi_cipherserver_ko_path, get_md5_by_ko_name(header, "hi_cipherserver.ko")) == 0);
        md5_verified &= (verify_single_ko_file(ksecurec_ko_path, get_md5_by_ko_name(header, "ksecurec.ko")) == 0);
        md5_verified &= (verify_single_ko_file(plat_gf61_ko_path, get_md5_by_ko_name(header, "plat_gf61.ko")) == 0);

        if (md5_verified) {
            WTSL_LOG_INFO(MODULE_NAME, "firmware extract successfully and all md5 verified!");
            // 清理临时压缩文件（可选）
            // unlink(firmware_path);
            return 0;
        } else {
            WTSL_LOG_ERROR(MODULE_NAME, "firmware extract but md5 verification failed!");
            return -1;
        }
    } else {
        WTSL_LOG_WARNING(MODULE_NAME, "firmware extract failed:some .ko files are missing!");
        if (!has_gt_gf61_ko_path) {
            WTSL_LOG_WARNING(MODULE_NAME, "---gt_gf61.ko missing");
        }
        if (!has_hi_cipherserver_ko_path) {
            WTSL_LOG_WARNING(MODULE_NAME, "---hi_cipherserver.ko missing");
        }
        if (!has_ksecurec_ko_path) {
            WTSL_LOG_WARNING(MODULE_NAME, "---ksecurec.ko missing");
        }
        if (!has_plat_gf61_ko_path) {
            WTSL_LOG_WARNING(MODULE_NAME, "---plat_gf61.ko missing");
        }
        return -1;
    }
}

// 使用孤儿进程的安全升级方案
static int orphan_process_upgrade(const firmware_header_t *header, const char *extract_dir)
{
    int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "start orphan process security");

    pid_t pid = fork();

    if (pid == -1) {
        WTSL_LOG_ERROR(MODULE_NAME, "fork failed");
        return -1;
    } else if (pid > 0) {
        // 父进程 - 立即返回, 让前端知道升级已开始
        WTSL_LOG_INFO(MODULE_NAME, "Parent processes: creating child processes PID = %d exec upgrade", pid);
        // if www path exist,also should upgrade web page
        char www_dir[UPGRADE_PATH_SIZE] = {0};
        snprintf(www_dir, sizeof(www_dir), "%s/www", extract_dir);
        if (access(www_dir, F_OK) == 0) {
            WTSL_LOG_INFO(MODULE_NAME, "upgrade web page...");
            upgrade_web_files_in_background(header, extract_dir);
        }
        return 0;
    } else {
        // 子进程 - 执行实际升级
        WTSL_LOG_INFO(MODULE_NAME, "child processes: start exec upgrade...");
        
        setsid();
        WTSL_LOG_INFO(MODULE_NAME, "create new conversation: SID=%d", getsid(0));
        ret = chdir("/");
        if(ret != 0){
            WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] ret:%d",__FUNCTION__,__LINE__,ret);
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        int log_fd = open("/tmp/app_upgrade.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        WTSL_LOG_INFO(MODULE_NAME, "process info: PID=%d, PPID=%d, SID=%d",
            getpid(), getppid(), getsid(0));
        int app_result = perform_app_upgrade(header, extract_dir);
        if (app_result == 0)
        {
            WTSL_LOG_DEBUG(MODULE_NAME, "sync && reboot");
            // record upgrade result
            FILE *result_file = fopen("/tmp/app_upgrade_result.txt", "w");
            if (result_file) {
                fprintf(result_file, "%d", app_result);
                fclose(result_file);
            }

            ret = system("sync && reboot");
            WTSL_LOG_INFO(MODULE_NAME, "========= appclition upgrade finish, result: %d ret:%d=========",app_result,ret);
        }
        exit(app_result);
    }
}


int handle_firmware_upgrade(const firmware_header_t *header, const char *version_number)
{
    int ret = -1;
    WTSL_LOG_INFO(MODULE_NAME, "========= start upgrade firmware handle =========");
    WTSL_LOG_INFO(MODULE_NAME, "target process: %s", header->process);
    WTSL_LOG_INFO(MODULE_NAME, "upgrade method: %s", header->up_method);

    char extract_dir[UPGRADE_PATH_SIZE] = {0};
    snprintf(extract_dir, sizeof(extract_dir), "/tmp/upload_file");

    // check upgrade package text
    int has_app = 0;
    int has_www = 0;
    int has_firmware = 0;

    char app_dir[UPGRADE_PATH_SIZE] = {0};
    char www_dir[UPGRADE_PATH_SIZE] = {0};
    char firmware_dir[UPGRADE_PATH_SIZE] = {0};
    version_number = get_build_version(header);

    snprintf(app_dir, sizeof(app_dir), "%s/app", extract_dir);
    snprintf(www_dir, sizeof(www_dir), "%s/www", extract_dir);
    snprintf(firmware_dir, sizeof(firmware_dir), "%s/firmware_t%s", extract_dir, version_number);

    // check path exist
    if (access(app_dir, F_OK) == 0) {
        has_app = 1;
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package include app path");

        // check app path is include wtsl_app or libwtsl_core.so
        check_app_directory(app_dir);

        // check app path is include wtsl_app
        char app_binary[UPGRADE_PATH_SIZE] = {0};
        snprintf(app_binary, sizeof(app_binary), "%s/wtsl_app", app_dir);
        if (access(app_binary, F_OK) == 0) {
            WTSL_LOG_INFO(MODULE_NAME, "found wtsl_app binary file");
        } else {
            WTSL_LOG_INFO(MODULE_NAME, "in app path not found wtsl_app binary file");
        }
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package not include app path");
    }

    if (access(www_dir, F_OK) == 0) {
        has_www = 1;
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package include www path");

        // check www path text
        char ls_cmd[CMD_SIZE] = {0};
        snprintf(ls_cmd, sizeof(ls_cmd), "ls -la %s", www_dir);
        ret = system(ls_cmd);
        if(ret != 0){
            WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
        }
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package not include www path");
    }

    if (access(firmware_dir, F_OK) == 0) {
        has_firmware = 1;
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package include firmware_t%s path", version_number);

        // check firmware path text
        char ls_cmd[CMD_SIZE] = {0};
        snprintf(ls_cmd, sizeof(ls_cmd), "ls -la %s", firmware_dir);
        ret |= system(ls_cmd);
    } else {
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package not include firmware_t%s path", version_number);
    }

    if (!has_app && !has_www && !has_firmware) {
        WTSL_LOG_INFO(MODULE_NAME, "upgrade package not found package path, exit upgrade!");
        return -1;
    }

    // record before upgrade status
    log_upgrade_status("START", header, has_app, has_www, has_firmware);

    if (has_firmware) {
        WTSL_LOG_INFO(MODULE_NAME, " ========= do Perform firmware_t%s File Upgrade", version_number);
        int reslut = upgrade_hi_firmware(header, extract_dir, version_number);
        if (reslut)
        {
            WTSL_LOG_ERROR(MODULE_NAME, "upgrade hi firmware failed!");
        }
    }

    // select the upgrade method according to the contents of upgrade package
    if (has_app) {
        WTSL_LOG_INFO(MODULE_NAME, " ========= do Perform Application Security Upgrade");
        return orphan_process_upgrade(header, extract_dir);
    } else {
        WTSL_LOG_INFO(MODULE_NAME, " ========= do Perform Web Page File Upgrade");
        return upgrade_web_files_in_background(header, extract_dir);
    }
}