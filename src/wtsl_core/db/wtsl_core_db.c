#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sqlite3.h"
#include "wtsl_core_db.h"
#include "wtsl_log_manager.h"
#include "wtsl_cfg_manager.h"

#define MODULE_NAME "DB"
static sqlite3 *db = NULL;

#define DBG_DB printf


static const char *table_create_sql[] = {
    // 用户表：关键字段强制非空
    "CREATE TABLE IF NOT EXISTS user ("
        "id	INTEGER NOT NULL UNIQUE,"
        "username	TEXT NOT NULL,"
        "password	TEXT NOT NULL,"
        "gid	INTEGER,"
        "token	TEXT,"
        "expire	INTEGER,"
        "name	TEXT,"
        "age	INTEGER,"
        "gender	TEXT,"
        "level	INTEGER,"
        "address	TEXT,"
        "phone	TEXT,"
        "PRIMARY KEY(\"id\" AUTOINCREMENT)"
    ");",

    // 任务表：任务名非空,记录修改配置操作记录
    "CREATE TABLE IF NOT EXISTS task("
    "task_name TEXT PRIMARY KEY NOT NULL,"  // 任务名 主键+非空
    "used_time INTEGER,"
    "plan_time INTEGER,"
    "left_time INTEGER,"
    "reward_score INTEGER"
    ");",

    // 日志表：任务名非空,记录登录,在线时间等常规日志
    "CREATE TABLE IF NOT EXISTS log("
    "username TEXT PRIMARY KEY NOT NULL,"  // 任务名 主键+非空
    "login_token TEXT,"
    "login_time INTEGER,"
    "left_time INTEGER"
    ");",

    NULL // 结束符
};

int db_create_all_tables(void) {
    char *err = NULL;
    int i = 0;

    while (table_create_sql[i] != NULL) {
        int rc = sqlite3_exec(db, table_create_sql[i], NULL, NULL, &err);
        if (rc != SQLITE_OK) {
            WTSL_LOG_ERROR(MODULE_NAME,"create table:[%s] error: %s\n",table_create_sql[i], err);
            sqlite3_free(err);
            return -1;
        }
        DBG_DB("Create table Success or is Existd\n");
        i++;
    }

    return 0;
}

static int db_set_default_admin_info(){
    if(!db_user_exists("admin")){
        db_register("admin",0,"123456");
        DBG_DB("db_set_default_admin_info ok\n");
    }
   return 0;
}

int init_db(void) {
    int ret;
    char *err_msg = NULL;
    char dbname[64]="wtsl.db";
    char dbkey[16]="12345678";
    char dbgmakey[64]={0};
    ret = config_get("DBNAME", dbname, 64);
    if(ret != 0){
        WTSL_LOG_WARNING(MODULE_NAME,"get DBNAME value error,use defalut dbname");
    }
    printf("init_db dbname:%s\n",dbname);
    //config_set("SLE_NAME", global_node_info.node_info.basic_info.sle_name);
    int rc = sqlite3_open(dbname, &db);
    if (rc != SQLITE_OK) {
        WTSL_LOG_ERROR(MODULE_NAME,"open database:%s error",dbname);
        return -1;
    }
    ret = config_get("DBKEY", dbkey, 16);
    if(ret != 0){
        WTSL_LOG_WARNING(MODULE_NAME,"get dbkey value error,use defalut dbkey");
        sqlite3_exec(db, "PRAGMA key = '12345678';", NULL, NULL, &err_msg);
    }else{
        sprintf(dbgmakey,"PRAGMA key = \'%s\';",dbkey);
        fprintf(stderr,"dbgmakey:%s",dbgmakey);
        sqlite3_exec(db, dbgmakey, NULL, NULL, &err_msg);
    }
    if (err_msg != NULL)
    {
        WTSL_LOG_ERROR(MODULE_NAME,"set database key error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    DBG_DB("database init ok\n");

    // 自动创建数组中所有表
    db_create_all_tables();

    ret = db_set_default_admin_info();
    if(ret != 0){
        WTSL_LOG_WARNING(MODULE_NAME,"db_set_default_admin_info error");
    }else{
        DBG_DB("db set default admin info ok\n");
    }
     
    return 0;
}

void db_close(void) {
    if (db) sqlite3_close(db);
}

int db_user_exists(const char *user) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT username FROM user WHERE username=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    int r = sqlite3_step(stmt) == SQLITE_ROW;
    printf("\n######  db_user_exists r:%d ,user:%s ##########\n",r,user);
    sqlite3_finalize(stmt);
    return r;
}

#if 0
int db_register(const char *user, const char *pwd) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO user VALUES(?,?,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pwd, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}
#endif

int db_register(const char *user, int gid ,const char *pwd) {

//    int gid = 1000;
    sqlite3_stmt *stmt;
    int rc;
    const char *sql = "INSERT INTO user ("
        "username, password, gid, token, expire, name, age, gender, level, address, phone"
        ") VALUES (?, ?, ?, NULL, 1699999999, '', 0, '', 0, '', '')";

    // 1. 校验非空
    if (!user || !pwd || strlen(user) == 0 || strlen(pwd) == 0) {
        printf("[DB] user password can not null\n");
        return -1;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("[DB] SQL failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pwd, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, gid); 

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("[DB] register failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);

    printf("[DB] register ok, user=%s\n", user);
    return 0;
}


int db_get_register_resp(const char *user,int *id,int *gid){
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id,gid FROM user WHERE username=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    *id = sqlite3_column_int(stmt,0);
    *gid = sqlite3_column_int(stmt,1);
    printf("db debug id:%d,gid:%d\n",*id,*gid);
    sqlite3_finalize(stmt);
    return 0;
}

int db_check_password(const char *user, const char *pwd) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT password FROM user WHERE username=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    const char *db_pwd = (const char *)sqlite3_column_text(stmt, 0);
    int ok = strcmp(pwd, db_pwd) == 0;
    sqlite3_finalize(stmt);
    return ok;
}

int db_get_token(const char *user, char *out) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT token FROM user WHERE username=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }
    const char *t = (const char *)sqlite3_column_text(stmt, 0);
    strncpy(out, t, 127);
    sqlite3_finalize(stmt);
    return 0;
}


int db_update_user_token(const char *username, const char *token) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE user SET token=? WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    int ret = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ret;
}

int db_update_userinfo(const char *username, const char *name, int age, const char *gender, int level, const char *address, const char *phone) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE user SET name=?, age=?, gender=?, level=?, address=?, phone=? WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, age);
    sqlite3_bind_text(stmt, 3, gender, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, level);
    sqlite3_bind_text(stmt, 5, address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, username, -1, SQLITE_STATIC);
    int ret = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ret;
}

int db_save_or_update_task(const char *task_name, int used_time, int plan_time, int left_time, int reward_score) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO task (task_name, used_time, plan_time, left_time, reward_score) VALUES (?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, task_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, used_time);
    sqlite3_bind_int(stmt, 3, plan_time);
    sqlite3_bind_int(stmt, 4, left_time);
    sqlite3_bind_int(stmt, 5, reward_score);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}


int db_get_user_ctx(const char *username,UserContext *ctx){
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, gid, expire FROM user WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return 0; }
    ctx->user_id = sqlite3_column_int(stmt, 0);
    ctx->group_id = sqlite3_column_int(stmt, 1);
    ctx->expire = sqlite3_column_int(stmt,2);
    sqlite3_finalize(stmt);
    return 0;
}

int db_get_userinfo(const char *username, char *name, int *age, char *gender, int *level, char *address, char *phone) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name, age, gender, level, address, phone FROM user WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return 0; }
    const char *n = (const char *)sqlite3_column_text(stmt, 0);
    *age = sqlite3_column_int(stmt, 1);
    const char *g = (const char *)sqlite3_column_text(stmt, 2);
    *level = sqlite3_column_int(stmt, 3);
    const char *addr = (const char *)sqlite3_column_text(stmt, 4);
    const char *ph = (const char *)sqlite3_column_text(stmt, 5);
    if (n) strcpy(name, n);
    if (g) strcpy(gender, g);
    if (addr) strcpy(address, addr);
    if (ph) strcpy(phone, ph);
    sqlite3_finalize(stmt);
    return 1;
}

int db_get_taskinfo(const char *task_name, int *used_time, int *plan_time, int *left_time, int *reward_score) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT used_time, plan_time, left_time, reward_score FROM task WHERE task_name=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, task_name, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return 0; }
    *used_time = sqlite3_column_int(stmt, 0);
    *plan_time = sqlite3_column_int(stmt, 1);
    *left_time = sqlite3_column_int(stmt, 2);
    *reward_score = sqlite3_column_int(stmt, 3);
    sqlite3_finalize(stmt);
    return 1;
}

int db_delete_user(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM user WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int db_delete_task(const char *task_name) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM task WHERE task_name=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, task_name, -1, SQLITE_STATIC);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int db_change_password(const char *username, const char *new_pwd) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE user SET password=? WHERE username=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, new_pwd, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}
