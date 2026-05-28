#ifndef DB_H
#define DB_H
#include "wtsl_user_manager.h"

int init_db(void);
int db_create_all_tables(void); // 批量创建表

int db_user_exists(const char *user);
int db_register(const char *user, const char *pwd);
int db_get_register_resp(const char *user,int *id,int *gid);
int db_check_password(const char *user, const char *pwd);
int db_get_token(const char *user, char *out);
int db_update_user_token(const char *username, const char *token);
int db_update_userinfo(const char *username, const char *name, int age, const char *gender, int level, const char *address, const char *phone);
int db_save_or_update_task(const char *task_name, int used_time, int plan_time, int left_time, int reward_score);
int db_get_user_ctx(const char *username,UserContext *ctx);
int db_get_userinfo(const char *username, char *name, int *age, char *gender, int *level, char *address, char *phone);
int db_get_taskinfo(const char *task_name, int *used_time, int *plan_time, int *left_time, int *reward_score);
int db_delete_user(const char *username);
int db_delete_task(const char *task_name);
int db_change_password(const char *username, const char *new_pwd);


#endif

