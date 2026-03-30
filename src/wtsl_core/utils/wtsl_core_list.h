#ifndef __WTSL_CORE_LIST_H_
#define __WTSL_CORE_LIST_H_

#include <stdlib.h>
#include <string.h>

// 链表节点结构
typedef struct ListNode {
    void *data;               // 存储任意类型数据（泛型）
    struct ListNode *next;    // 指向下一个节点
} ListNode;

// 链表结构（管理节点，记录元数据）
typedef struct {
    ListNode *head;           // 头节点
    size_t data_size;         // 每个数据元素的大小（字节）
    int size;                 // 链表当前节点数量
} List;

// 函数指针：用于释放数据（针对动态分配的数据）
typedef void (*FreeFunc)(void *data);

// 函数指针：用于比较数据（查找/删除时使用）
typedef int (*CompareFunc)(const void *a, const void *b);

// 函数指针：用于遍历数据时的回调（打印/处理数据）
typedef void (*TraverseFunc)(const void *data);


// 1. 创建链表（指定数据元素大小）
List* list_create(size_t data_size);

// 2. 头插法插入节点
void list_insert_head(List *list, const void *data);

// 3. 尾插法插入节点
void list_insert_tail(List *list, const void *data);

// 4. 按索引插入节点（index: 0~size）
int list_insert_at(List *list, int index, const void *data);

// 5. 查找节点（返回索引，未找到返回-1）
int list_find(const List *list, const void *target, CompareFunc compare);

const void* list_get(const List *list, int index);

// 6. 删除头节点
int list_remove_head(List *list, FreeFunc free_data);

// 7. 删除尾节点
int list_remove_tail(List *list, FreeFunc free_data);

// 8. 按索引删除节点
int list_remove_at(List *list, int index, FreeFunc free_data);

// 9. 按值删除节点（删除第一个匹配项）
int list_remove_by_value(List *list, const void *target, 
                         CompareFunc compare, FreeFunc free_data);

// 10. 遍历链表（对每个节点调用回调函数）
void list_traverse(const List *list, TraverseFunc traverse);

// 11. 获取链表长度
int list_size(const List *list);

// 12. 销毁链表（释放所有节点和数据）
void list_destroy(List *list, FreeFunc free_data);

#endif // __WTSL_CORE_LIST_H_