#include <stdio.h>
#include <string.h>
#include "wtsl_core_list.h"
#include "wtsl_log_manager.h"

#define MODULE_NAME "list_test"

// 测试1：存储整数
void test_int_list() {
    WTSL_LOG_INFO(MODULE_NAME, "=== test integer linked list ===");

    // 创建存储int的链表（int占4字节）
    List *int_list = list_create(sizeof(int));
    if (!int_list) {
        WTSL_LOG_ERROR(MODULE_NAME, "link create failed");
        return;
    }

    // 插入数据
    int a = 10, b = 20, c = 30, d = 15;
    list_insert_tail(int_list, &a);  // 尾插10
    list_insert_tail(int_list, &b);  // 尾插20
    list_insert_head(int_list, &c);  // 头插30
    list_insert_at(int_list, 2, &d); // 索引2插入15

    // 打印链表（定义遍历回调）
    WTSL_LOG_INFO(MODULE_NAME, "find tests by index: ");
    for (int i = 0; i < list_size(int_list); i++) {
        const int *val = (const int*)list_get(int_list, i);
        if (val) {
            WTSL_LOG_INFO(MODULE_NAME, "index %d value: %d", i, *val);
        } else {
            WTSL_LOG_ERROR(MODULE_NAME, "index %d find failed", i);
        }
    }
    // 销毁链表
    list_destroy(int_list, NULL);
    WTSL_LOG_INFO(MODULE_NAME, "end of integer linked list test");
}

// 测试2：存储字符串（动态分配）
typedef struct {
    char *str;
} StringData;

// 字符串比较函数
int str_compare(const void *a, const void *b) {
    const StringData *sa = (const StringData*)a;
    const StringData *sb = (const StringData*)b;
    return strcmp(sa->str, sb->str);
}

// 字符串遍历函数
void str_traverse(const void *data) {
    const StringData *sd = (const StringData*)data;
    WTSL_LOG_INFO(MODULE_NAME, "%s ", sd->str);
}

// 字符串释放函数（释放动态分配的str）
void str_free(void *data) {
    StringData *sd = (StringData*)data;
    free(sd->str);
}

void test_string_list() {
    WTSL_LOG_INFO(MODULE_NAME, "=== test string linked list ===");

    // 创建存储StringData的链表
    List *str_list = list_create(sizeof(StringData));
    if (!str_list) {
        WTSL_LOG_ERROR(MODULE_NAME, "linked create failed");
        return;
    }

    // 插入数据（动态分配字符串）
    StringData s1 = {.str = strdup("apple")};
    StringData s2 = {.str = strdup("banana")};
    StringData s3 = {.str = strdup("orange")};
    list_insert_tail(str_list, &s1);
    list_insert_tail(str_list, &s2);
    list_insert_head(str_list, &s3);

    // 打印链表
    WTSL_LOG_INFO(MODULE_NAME, "insertingt a linked list(length: %d):", list_size(str_list));
    list_traverse(str_list, str_traverse);
    printf("\n");  // 预期：orange apple banana

    // 查找元素
    StringData target = {.str = "apple"};
    int index = list_find(str_list, &target, str_compare);
    WTSL_LOG_INFO(MODULE_NAME, "find %s index: %d(expectations of:1)", target.str, index);

    // 删除元素（需释放动态分配的字符串）
    list_remove_by_value(str_list, &target, str_compare, str_free);

    WTSL_LOG_INFO(MODULE_NAME, "deleted %s after:",target.str);
    list_traverse(str_list, str_traverse);
    printf("\n");  // 预期：orange banana

    // 销毁链表（释放所有动态字符串）
    list_destroy(str_list, str_free);
    WTSL_LOG_INFO(MODULE_NAME, "end of string linked list test");
}

int list_test_main() {
    test_int_list();    // 测试整数类型
    test_string_list(); // 测试字符串类型（动态数据）
    return 0;
}