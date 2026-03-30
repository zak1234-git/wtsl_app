#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>

#include "wtsl_cfg_manager.h"

#include "wtsl_log_manager.h"

#include "wtsl_core_http_restful_api.h"
#include "wtsl_core_dataparser.h"

#define MODULE_NAME "http_main"
#define BUFFER_SIZE 1024
#define POSTBUFFERSIZE  512
#define MAXCLIENTS      32
#define MAX_RESPONSE_LEN 1024  // 限制响应大小，避免处理问题
#define STATIC_ERROR_RESPONSE "{\"error\":\"Internal error\"}"
#define STATIC_SUCCESS_RESPONSE "{\"status\":\"success\"}"

int wtsl_gw_port = 8080;

static struct MHD_Daemon *global_daemon = NULL;


// 每个请求的数据结构
struct request_data {
    char *data;
    size_t data_size;
    size_t buffer_capacity;
    size_t expected_size;
    int is_complete;
    int data_received;
    char response_str[MAX_RESPONSE_LEN];  // 改用固定大小缓冲区存储响应
    int response_valid;  // 响应是否有效
};



static struct request_data* init_request_data(const char *content_length);
int http_get_cmd(struct MHD_Connection *connection,const char *url, const char *upload_data,size_t *upload_data_size);
int wtsl_core_http_deal_post_data(struct MHD_Connection *connection,const char *url,char *data,size_t data_size);



// 添加允许任意源的CORS头
void add_any_origin_cors_headers(struct MHD_Response *response) {
    // 允许任意源（*表示通配符）
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    // 允许的请求方法
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    // 允许的请求头
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    // 预检结果缓存时间（24小时）
    MHD_add_response_header(response, "Access-Control-Max-Age", "86400");
}

// 处理预检请求（OPTIONS方法）
enum MHD_Result handle_options_request(struct MHD_Connection *conn) {
    struct MHD_Response *response = MHD_create_response_from_buffer(
        0, NULL, MHD_RESPMEM_PERSISTENT
    );
    
    add_any_origin_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

// 处理业务请求
enum MHD_Result handle_business_request(struct MHD_Connection *conn, const char *method, const char *url,const char *upload_data,
                               size_t *upload_data_size) {
    const char *response_data;
    int status_code;
    WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] business method:%s",__func__,__LINE__,method);
    if (strcmp(method, "POST") == 0 && strcmp(url, "/api/v1/setnode") == 0) {
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] post len:%u,data:%s",__func__,__LINE__,*upload_data_size,upload_data);
        response_data = "{\"status\":\"success\",\"message\":\"Node set successfully\"}";
        status_code = MHD_HTTP_OK;
    } else {
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] no",__func__,__LINE__);
        response_data = "{\"status\":\"error\",\"message\":\"Not found\"}";
        status_code = MHD_HTTP_NOT_FOUND;
    }
    
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(response_data),
        (void *)response_data,
        MHD_RESPMEM_MUST_COPY
    );
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    add_any_origin_cors_headers(response);
    
    enum MHD_Result ret = MHD_queue_response(conn, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

// 主请求处理回调
enum MHD_Result request_handler(void *cls, struct MHD_Connection *conn,
                               const char *url, const char *method,
                               const char *version, const char *upload_data,
                               size_t *upload_data_size, void **con_cls) {

	(void)cls;
	(void)version;
	(void)url;
	(void)upload_data;
	// WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] method:%s,url:%s,uplen:%d,updata:%s",__func__,__LINE__,method,url,*upload_data_size,upload_data);
    if (*con_cls == NULL) {
		const char *content_length = MHD_lookup_connection_value(conn,
	                                                                MHD_HEADER_KIND,
	                                                                "Content-Length");
		//WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] content_len:%s",__func__,__LINE__,content_length);
		struct request_data *rd = init_request_data(content_length);
        if (!rd) {
            // 使用最简化的错误响应
            struct MHD_Response *resp = MHD_create_response_from_buffer(
                strlen(STATIC_ERROR_RESPONSE), (void*)STATIC_ERROR_RESPONSE, MHD_RESPMEM_PERSISTENT);
            if (resp) {
                MHD_add_response_header(resp, "Content-Type", "application/json");
                MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
                MHD_destroy_response(resp);
            }
            return MHD_YES;  // 返回YES避免服务器立即关闭连接
        }
        *con_cls = rd;
        return MHD_YES;
    }
    if (strcmp(method, "OPTIONS") == 0) {
			WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] option",__func__,__LINE__);
            return handle_options_request(conn);
    } else if(strcmp(method, "GET") == 0){
		WTSL_LOG_INFO(MODULE_NAME, "handle_get ");
		return http_get_cmd(conn,url,upload_data,upload_data_size);
    }else if(strcmp(method, "POST") == 0){
    	struct request_data *rd = *con_cls;
		if (*upload_data_size > 0) {
	        rd->data_received = 1;
	        
	        if (rd->data_size + *upload_data_size + 1 > rd->buffer_capacity) {
	            size_t new_cap = rd->data_size + *upload_data_size + 1;
	            char *new_data = realloc(rd->data, new_cap);
	            if (!new_data) {
	                WTSL_LOG_ERROR(MODULE_NAME, "buffer expansion failed");
	                snprintf(rd->response_str, MAX_RESPONSE_LEN, 
	                         "{\"error\":\"Buffer full\"}");
	                rd->response_valid = 1;
	                rd->is_complete = 1;
	            }
	            rd->data = new_data;
	            rd->buffer_capacity = new_cap;
	            WTSL_LOG_INFO(MODULE_NAME, "post_data_handler: buffer expansion to %zu byte", new_cap);
	        }

	        memcpy(rd->data + rd->data_size, upload_data, *upload_data_size);
	        rd->data_size += *upload_data_size;
	        rd->data[rd->data_size] = '\0';
	        *upload_data_size = 0;
	        if (rd->expected_size > 0 && rd->data_size >= rd->expected_size) {
	            rd->is_complete = 1;
	        }
	        return MHD_YES;
	    }

		if (rd->is_complete) {
		   WTSL_LOG_INFO(MODULE_NAME, "post_data_handler: data Receive completed(actual: %zu, expect: %zu)",rd->data_size, rd->expected_size);
		   WTSL_LOG_INFO(MODULE_NAME, "data:%s,data_receied:%d,size:%u",rd->data,rd->data_received,rd->data_size);
		   wtsl_core_http_deal_post_data(conn,url,rd->data,rd->data_size);
		   *upload_data_size = 0;
		   return MHD_YES;
		}
    }else{
		struct MHD_Response *resp = MHD_create_response_from_buffer(
				strlen(STATIC_ERROR_RESPONSE), (void*)STATIC_ERROR_RESPONSE, MHD_RESPMEM_PERSISTENT);
		if (resp) {
			MHD_add_response_header(resp, "Content-Type", "application/json");
			MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
			MHD_destroy_response(resp);
		}
	}
    return MHD_YES;
}

// 连接清理回调
void connection_cleanup(void *cls, struct MHD_Connection *conn,
                       void **con_cls, enum MHD_RequestTerminationCode toe) {
	(void)cls;
	(void)conn;
	(void)toe;
    if (*con_cls != NULL) {
        free(*con_cls);
        *con_cls = NULL;
    }
}





// 初始化请求数据
static struct request_data* init_request_data(const char *content_length) {
    struct request_data *rd = malloc(sizeof(struct request_data));
    if (!rd) {
        WTSL_LOG_ERROR(MODULE_NAME, "memory malloc failed");
        return NULL;
    }

    // 初始化数据接收字段
    rd->expected_size = content_length ? (size_t)atoi(content_length) : 0;
    WTSL_LOG_INFO(MODULE_NAME, "initialization request data - expected receive size: %zu bytes", rd->expected_size);

    rd->buffer_capacity = rd->expected_size > 0 ? rd->expected_size + 1 : BUFFER_SIZE;
    rd->data = malloc(rd->buffer_capacity);
    if (!rd->data) {
        WTSL_LOG_ERROR(MODULE_NAME, "malloc failed");
        free(rd);
        return NULL;
    }

    rd->data_size = 0;
    rd->is_complete = 0;
    rd->data_received = 0;
    rd->data[0] = '\0';

    // 初始化响应（使用固定缓冲区，避免动态内存问题）
    rd->response_str[0] = '\0';
    rd->response_valid = 0;

    return rd;
}

static enum MHD_Result
print_out_key (void *cls, enum MHD_ValueKind kind, const char *key,
               const char *value)
{
  (void) cls;    /* Unused. Silent compiler warning. */
  (void) kind;   /* Unused. Silent compiler warning. */
  WTSL_LOG_INFO(MODULE_NAME, "key:%s: value: %s", key, value);
  return MHD_YES;
}


extern char *data_response_to_json(char status,const char *data);



inline static int http_response_json_str(struct MHD_Connection *connection,const char *json_str){
	int ret;
	// WTSL_LOG_INFO(MODULE_NAME, "http_response_json_str json_str:%s",json_str);
	struct MHD_Response *http_response = MHD_create_response_from_buffer(
			strlen(json_str), (void*)json_str, MHD_RESPMEM_MUST_FREE);
	add_any_origin_cors_headers(http_response);
	
	ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
	// WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],MHD_queue_response ret:%d",__func__,__LINE__,ret);
	usleep(1000);
	if(!http_response){
		MHD_destroy_response(http_response);
		WTSL_LOG_INFO(MODULE_NAME, "[%s][%d],MHD_queue_response ret:%d !!!!!!!!",__func__,__LINE__,ret);	
	}
	// WTSL_LOG_INFO(MODULE_NAME, "[%s][%d] out",__func__,__LINE__);
	return ret;
}
int http_get_cmd(struct MHD_Connection *connection,const char *url, const char *upload_data,size_t *upload_data_size) {

	int ret = 0;

	MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, print_out_key,NULL);
	APIRET apiret = wtcoreapi_parse_http_get_cmd(url,upload_data,*upload_data_size);

    char *json_str = data_response_to_json(apiret.status, apiret.data);
	// WTSL_LOG_INFO(MODULE_NAME, "json_str:%s",json_str);
	ret = http_response_json_str(connection,json_str);
    return ret;
}


int wtsl_core_http_deal_post_data(struct MHD_Connection *connection,const char *url,char *data,size_t data_size){
	int ret = 0;
	APIRET apiret = wtcoreapi_parse_http_post_cmd(url,data,data_size);
	char *json_str = data_response_to_json(apiret.status, apiret.data);
	// WTSL_LOG_INFO(MODULE_NAME, "json_str:%s",json_str);
	ret = http_response_json_str(connection,json_str);
	return ret;
}


int wtsl_http_main() {
    int http_port;
    char env_http_port[8]="8080";
	WTSL_LOG_INFO(MODULE_NAME, "[%s][%s][%d]",__FILE__,__FUNCTION__,__LINE__);
	WTSL_LOG_INFO(MODULE_NAME, "version: %s",MHD_get_version());

    
    int ret = config_get("HTTP_PORT",env_http_port,8);
    if(ret != 0){
        WTSL_LOG_ERROR(MODULE_NAME, "get key value error:%s",env_http_port);
        strcpy(env_http_port,"8080");
    }
    http_port = atoi(env_http_port);
    WTSL_LOG_INFO(MODULE_NAME, "env_http_port:%s,http_port:%d",env_http_port,http_port);
    if(http_port != 0)
        wtsl_gw_port = http_port;

    // 创建HTTP服务器
    global_daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD |
                           MHD_USE_DEBUG, wtsl_gw_port, NULL, NULL,
                             (MHD_AccessHandlerCallback)&request_handler, NULL, MHD_OPTION_END);
    
    if (global_daemon == NULL) {
        WTSL_LOG_ERROR(MODULE_NAME, "Failed to start daemon");
        return 1;
    }
    
    WTSL_LOG_INFO(MODULE_NAME, "REST API server running on port %d", wtsl_gw_port);
    //WTSL_LOG_INFO(MODULE_NAME, "Press Enter to stop...");
    //getchar();
    return 0;
}

int wtsl_http_main_stop(){
    if(global_daemon == NULL){
        return -1;
    }   
    MHD_stop_daemon(global_daemon);
    return 0;
}