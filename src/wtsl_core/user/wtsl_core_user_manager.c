#include "wtsl_user_manager.h"
#include "wtsl_log_manager.h"
#include "wtsl_core_node_manager.h"
#include "openssl/md5.h"
#include "openssl/sha.h"


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


char* generate_salt(int length){
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "0123456789./";
    char *salt = (char *)malloc(length + 1);

	if(salt){
		srand(time(NULL) ^ (unsigned long)salt);
		for(int i=0;i<length;i++){
			salt[i] = charset[rand() % (sizeof(charset) -1 )];
		}
		salt[length]='\0';
	}
	return salt;
}


void str2md5(const char *str, char *out_md5)
{
    MD5_CTX ctx;
    unsigned char md[16];  // MD5 固定输出 16 字节
    int i;


    MD5_Init(&ctx);
    MD5_Update(&ctx, str, strlen(str));
    MD5_Final(md, &ctx);


    // 转成 32 位十六进制
    for (i = 0; i < 16; i++)
        sprintf(out_md5 + i*2, "%02x", md[i]);
}

// 使用SHA-256加盐哈希加密密码
char* encrypt_password(const char* plain_password, const char* salt) {
    if (!plain_password || !salt) return NULL;
    
    // 组合密码和盐值
    size_t pass_len = strlen(plain_password);
    size_t salt_len = strlen(salt);
    char* combined = (char*)malloc(pass_len + salt_len + 1);
    
    if (!combined) return NULL;
    
    strcpy(combined, plain_password);
    strcat(combined, salt);
    
    // 计算SHA-256哈希
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)combined, strlen(combined), hash);
    
    // 转换为十六进制字符串
    char* encrypted = (char*)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!encrypted) {
        free(combined);
        return NULL;
    }
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(encrypted + (i * 2), "%02x", hash[i]);
    }
    encrypted[SHA256_DIGEST_LENGTH * 2] = '\0';
    
    free(combined);
    return encrypted;
}


// 验证密码
int verify_password(const char* plain_password, const char* encrypted_password, const char* salt) {
    if (!plain_password || !encrypted_password || !salt) return 0;
    
    char* encrypted = encrypt_password(plain_password, salt);
    if (!encrypted) return 0;
    
    int result = (strcmp(encrypted, encrypted_password) == 0);
    free(encrypted);
    
    return result;
}

void generate_token(const char *user, char *out) {
	char tokenstr[128]={0};
	char md5[33]={0};
    unsigned long long ts = (unsigned long long)time(NULL);
	sprintf(tokenstr,"%s_%llu_%s", user, ts, generate_salt(16));
	str2md5(tokenstr,md5);
	printf("tokenstr:%s,md5:%s\n",tokenstr,md5);
	snprintf(out,MAX_TOKEN,md5);
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
	fprintf(stderr,"##########groupid:%d user:%s################\n",g_user_ctx.group_id,user);
	switch(g_user_ctx.group_id){
		case 0:
			g_user_ctx.permission = PERMISSION_ADMIN;//PERMISSION_READ | PERMISSION_WRITE |  PERMISSION_EXECUTE;
			break;
		case 1000:			
		default:
			g_user_ctx.permission = PERMISSION_READ;
			break;
	}
	//if(strcmp(user,"admin") == 0)g_user_ctx.permission = PERMISSION_READ | PERMISSION_WRITE |  PERMISSION_EXECUTE;
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
        db_register(user, 1000, pwd);
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