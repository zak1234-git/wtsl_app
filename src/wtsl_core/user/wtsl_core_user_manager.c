#include "wtsl_user_manager.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_node_manager.h"


#define MODULE_NAME "user_manager"

#define MAX_TOKEN 128

static UserContext g_user_ctx = {0};

//static char user_api_ret_msg[2048] = {0};

static char api_err_msg[][64]={
    "success",
    "param error",
    "no memory",
    "user is not existed",
    "user is existed",
    "unknown error"
};

// 所有接口统一在这里配置！！
static RouteItem g_route_table[] = {
    { "/user/login",     wtsl_core_user_login },
    { "/user/register",  wtsl_core_user_register },
    { "/user/update_userinfo", wtsl_core_user_user_set_info },		//post json
    { "/user/get_userinfo",      wtsl_core_user_get_info },      //查询用户
	{ "/user/delete_user",       wtsl_core_user_delete },
    { "/user/change_pwd",        wtsl_core_user_change_pwd },
    { NULL, NULL },  // 结束符，必须保留
};

void generate_token(const char *user, char *out) {
	char key[33] = {0};
    unsigned long long ts = (unsigned long long)time(NULL);
	// if (md5_file(g_cfg.dbname, key) != 0) {
    //     printf("[WARN] MD5计算失败，使用默认密钥\n");
    //     strcpy(key, "00000000000000000000000000000000");
    // }
    //md5_string("user.db", key);
    strcpy(key,"thisisatestkey");
    //snprintf(out, MAX_TOKEN, "%s_%llu_%s", user, ts, g_cfg.token_key);
    snprintf(out, MAX_TOKEN, "%s_%llu_%s", user, ts, key);
}


int check_token_is_valid(char *user,char *token){
    char db_token[128]={0};
    db_get_token(user,db_token);
    return strcmp(db_token,token);
}

int check_permission(WTSLNode *pNode,void *data,unsigned int size){

    WTSL_LOG_DEBUG(MODULE_NAME,"check_permission,mac:%s,data:%s,size:%d",pNode->info.basic_info.mac,data,size);
    return 0;
}

// 权限检查函数
int check_permission_user(UserContext *ctx, PermissionLevel required) {
    if (!ctx) {
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] ctx is null",__FUNCTION__,__LINE__);
        return 0;
    }
    // 检查用户是否有足够的权限（包含关系）
    return (ctx->permission & required) == required;
}

int check_user_has_permission(char *username,char *tokenstr,char *timestr){
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d],user:%s,token:%s,time:%s",__FUNCTION__,__LINE__,username,tokenstr,timestr);
    return -1;
}

UserContext wtsl_core_user_get_context(){
    return g_user_ctx;
}

void wtsl_core_user_set_context(UserContext ctx){
    g_user_ctx = ctx;
}

void wtsl_core_user_get_routeitem(RouteItem **item,int *count){
    *count = sizeof(g_route_table) / sizeof(g_route_table[0]) - 1;
    *item = g_route_table;
}

char *wtsl_core_get_err_resp(RetErrorCode errcode){
     cJSON *resp = cJSON_CreateObject();
     cJSON_AddItemToObject(resp, "errcode", cJSON_CreateNumber(errcode));
     cJSON_AddItemToObject(resp, "errmsg", cJSON_CreateString(api_err_msg[errcode]));
     return cJSON_Print(resp);
}

void* wtsl_core_user_login(char *url,void *args,int size){
    char *user;
    char *pwd;
    cJSON *resp;
    cJSON *root =cJSON_Parse(args);
    if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
	}
    cJSON* cjuser = cJSON_GetObjectItem(root,"username");
    if(cjuser != NULL){
        user = cjuser->valuestring;
    }
	
	cJSON* cjpwd = cJSON_GetObjectItem(root,"password");
	if(cjpwd != NULL){
        pwd = cjpwd->valuestring;
    }
    if(user == NULL || pwd == NULL){
        WTSL_LOG_ERROR(MODULE_NAME,"user or password must set");
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
    }
    printf("[DEBUG] login user=%s, pwd=%s\n", user, pwd);
    if (!db_user_exists(user)) {
        printf("[DEBUG] user not exists\n");
        return wtsl_core_get_err_resp(RET_ERR_USER_IS_NOT_EXISTED);
    }
    if (!db_check_password(user, pwd)) {
        printf("[DEBUG] password error\n");
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
    }
    generate_token(user,g_user_ctx.token);
    fprintf(stderr,"##########token:%s ################\n",g_user_ctx.token);
    db_get_user_ctx(user,&g_user_ctx);
    db_update_user_token(user,g_user_ctx.token);
    resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "token", cJSON_CreateString(g_user_ctx.token));
    cJSON_AddItemToObject(resp, "expires", cJSON_CreateNumber(g_user_ctx.expire));
    cJSON_AddItemToObject(resp, "uid", cJSON_CreateNumber(g_user_ctx.user_id));
    cJSON_AddItemToObject(resp, "gid", cJSON_CreateNumber(g_user_ctx.group_id));
	return cJSON_Print(resp);
}


void* wtsl_core_user_register(char *url,void *args,int size){
    char *user;
    char *pwd;
    int db_uid,db_gid;
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d],user:%s,args:%s,size:%d",__FUNCTION__,__LINE__,url,args,size);

    cJSON *resp = cJSON_CreateObject();
    cJSON *root =cJSON_Parse(args);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
	}
    cJSON* cjuser = cJSON_GetObjectItem(root,"username");
    if(cjuser != NULL){
        user = cjuser->valuestring;
    }
	
	cJSON* cjpwd = cJSON_GetObjectItem(root,"password");
	if(cjpwd != NULL){
        pwd = cjpwd->valuestring;
    }
    if(user == NULL || pwd == NULL){
        WTSL_LOG_ERROR(MODULE_NAME,"user or password must set");
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
    }
    printf("[DEBUG] login user=%s, pwd=%s\n", user, pwd);
    if (!db_user_exists(user)) {
        printf("[DEBUG] user not exists,to register\n");
        db_register(user, pwd);
        printf("[DEBUG] register over\n");
    }else{
        WTSL_LOG_ERROR(MODULE_NAME,"<%s> is existed ################",user);
        return wtsl_core_get_err_resp(RET_ERR_USER_IS_EXISTED);
    }

    if(db_get_register_resp(user,&db_uid,&db_gid) == 0) {
        cJSON_AddItemToObject(resp, "code", cJSON_CreateNumber(RET_OK));
        cJSON_AddItemToObject(resp, "username", cJSON_CreateString(user));
        cJSON_AddItemToObject(resp, "uid", cJSON_CreateNumber(db_uid));
        cJSON_AddItemToObject(resp, "gid", cJSON_CreateNumber(db_gid));
        return cJSON_Print(resp);
        printf("[DEBUG] register ok\n");
    }else{
        printf("[DEBUG] register sth error\n");
    }
    return wtsl_core_get_err_resp(RET_ERR_OTHER);
}


void* wtsl_core_user_user_set_info(){
    return NULL;
}
void* wtsl_core_user_get_info(){
    return NULL;
}
void* wtsl_core_user_delete(){
    return NULL;
}
void* wtsl_core_user_change_pwd(char *url,void *args,int size){
    char *user;
    char *pwd;
    int db_uid,db_gid;
    WTSL_LOG_INFO(MODULE_NAME,"[%s][%d],user:%s,args:%s,size:%d",__FUNCTION__,__LINE__,url,args,size);

    cJSON *resp = cJSON_CreateObject();
    cJSON *root =cJSON_Parse(args);
	if(root == NULL){
		WTSL_LOG_ERROR(MODULE_NAME, "[%s][%d] data is not a vaild json data",__func__,__LINE__);
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
	}
    cJSON* cjuser = cJSON_GetObjectItem(root,"username");
    if(cjuser != NULL){
        user = cjuser->valuestring;
    }
	
	cJSON* cjoldpwd = cJSON_GetObjectItem(root,"oldpassword");
	if(cjoldpwd != NULL){
        pwd = cjoldpwd->valuestring;
    }
    if(user == NULL || pwd == NULL){
        WTSL_LOG_ERROR(MODULE_NAME,"user or password must set");
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
    }

    if (!db_check_password(user, pwd)) {
        printf("[DEBUG] password error\n");
        return wtsl_core_get_err_resp(RET_ERR_PARAM);
    }
    cJSON* cjnewpwd = cJSON_GetObjectItem(root,"newpassword");
	if(cjoldpwd != NULL){
        pwd = cjnewpwd->valuestring;
    }
    printf("[DEBUG] change passwd user=%s, pwd=%s\n", user, pwd);
    db_change_password(user,pwd);
    cJSON_AddItemToObject(resp, "username", cJSON_CreateString(user));
    cJSON_AddItemToObject(resp, "msg", cJSON_CreateString("change password success"));
    return cJSON_Print(resp);
}