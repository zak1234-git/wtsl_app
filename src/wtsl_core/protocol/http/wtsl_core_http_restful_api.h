#ifndef _HTTP_RESTFUL_API_H
#define _HTTP_RESTFUL_API_H

#define RESTFUL_API_VERSION 1
#define RESTFUL_API_BASE "api"

typedef struct _APIRET{
	char status;
	void *data;
}APIRET,*pAPIRET;
APIRET wtcoreapi_parse_http_get_cmd(const char *url, const char *upload_data,int upload_data_size);


APIRET wtcoreapi_parse_http_post_cmd(const char *url, const char *data,int size);



#endif