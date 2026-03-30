#include "wtsl_core_list.h"

// 1. 创建链表
List* list_create(size_t data_size) {
    List *list = (List*)malloc(sizeof(List));
    if (!list) return NULL;

    list->head = NULL;
    list->data_size = data_size;
    list->size = 0;
    return list;
}

// 辅助函数：创建新节点
static ListNode* create_node(const void *data, size_t data_size) {
    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return NULL;

    // 为数据分配内存并复制内容
    node->data = malloc(data_size);
    if (!node->data) {
        free(node);
        return NULL;
    }
    memcpy(node->data, data, data_size);

    node->next = NULL;
    return node;
}

// 2. 头插法
void list_insert_head(List *list, const void *data) {
    if (!list || !data) return;

    ListNode *node = create_node(data, list->data_size);
    if (!node) return;

    node->next = list->head;
    list->head = node;
    list->size++;
}

// 3. 尾插法
void list_insert_tail(List *list, const void *data) {
    if (!list || !data) return;

    ListNode *node = create_node(data, list->data_size);
    if (!node) return;

    if (!list->head) {  // 空链表直接插入头部
        list->head = node;
    } else {  // 遍历到尾部
        ListNode *cur = list->head;
        while (cur->next) {
            cur = cur->next;
        }
        cur->next = node;
    }
    list->size++;
}

// 4. 按索引插入
int list_insert_at(List *list, int index, const void *data) {
    if (!list || !data || index < 0 || index > list->size) {
        return -1;  // 索引无效
    }

    if (index == 0) {  // 头插
        list_insert_head(list, data);
        return 0;
    }

    // 找到索引前一个节点
    ListNode *cur = list->head;
    for (int i = 0; i < index - 1; i++) {
        cur = cur->next;
    }

    ListNode *node = create_node(data, list->data_size);
    if (!node) return -1;

    node->next = cur->next;
    cur->next = node;
    list->size++;
    return 0;
}

// 5. 查找节点
int list_find(const List *list, const void *target, CompareFunc compare) {
    if (!list || !target || !compare || list->size == 0) {
        return -1;
    }

    ListNode *cur = list->head;
    for (int i = 0; i < list->size; i++) {
        if (compare(cur->data, target) == 0) {
            return i;  // 找到返回索引
        }
        cur = cur->next;
    }
    return -1;  // 未找到
}

const void* list_get(const List *list, int index) {
    // 检查参数合法性：链表为空、索引越界
    if (!list || list->size == 0 || index < 0 || index >= list->size) {
        return NULL;
    }

    // 遍历到目标索引
    ListNode *cur = list->head;
    for (int i = 0; i < index; i++) {
        cur = cur->next;
    }

    return cur->data;  // 返回数据指针（const确保不被修改）
}

// 6. 删除头节点
int list_remove_head(List *list, FreeFunc free_data) {
    if (!list || list->size == 0) return -1;

    ListNode *temp = list->head;
    list->head = list->head->next;

    // 释放数据（如果需要）
    if (free_data) {
        free_data(temp->data);
    }
    free(temp->data);  // 释放数据内存
    free(temp);        // 释放节点
    list->size--;
    return 0;
}

// 7. 删除尾节点
int list_remove_tail(List *list, FreeFunc free_data) {
    if (!list || list->size == 0) return -1;

    if (list->size == 1) {  // 只有头节点
        return list_remove_head(list, free_data);
    }

    // 找到倒数第二个节点
    ListNode *cur = list->head;
    while (cur->next->next) {
        cur = cur->next;
    }

    ListNode *temp = cur->next;
    cur->next = NULL;

    if (free_data) free_data(temp->data);
    free(temp->data);
    free(temp);
    list->size--;
    return 0;
}

// 8. 按索引删除
int list_remove_at(List *list, int index, FreeFunc free_data) {
    if (!list || index < 0 || index >= list->size) {
        return -1;
    }

    if (index == 0) {  // 删除头节点
        return list_remove_head(list, free_data);
    }

    // 找到索引前一个节点
    ListNode *cur = list->head;
    for (int i = 0; i < index - 1; i++) {
        cur = cur->next;
    }

    ListNode *temp = cur->next;
    cur->next = temp->next;

    if (free_data) free_data(temp->data);
    free(temp->data);
    free(temp);
    list->size--;
    return 0;
}

// 9. 按值删除
int list_remove_by_value(List *list, const void *target, 
                         CompareFunc compare, FreeFunc free_data) {
    int index = list_find(list, target, compare);
    if (index == -1) return -1;

    return list_remove_at(list, index, free_data);
}

// 10. 遍历链表
void list_traverse(const List *list, TraverseFunc traverse) {
    if (!list || !traverse || list->size == 0) return;

    ListNode *cur = list->head;
    while (cur) {
        traverse(cur->data);
        cur = cur->next;
    }
}

// 11. 获取链表长度
int list_size(const List *list) {
    return list ? list->size : -1;
}

// 12. 销毁链表
void list_destroy(List *list, FreeFunc free_data) {
    if (!list) return;

    ListNode *cur = list->head;
    while (cur) {
        ListNode *next = cur->next;

        // 释放数据（如果需要）
        if (free_data) {
            free_data(cur->data);
        }
        free(cur->data);  // 释放数据内存
        free(cur);        // 释放节点
        cur = next;
    }

    free(list);  // 释放链表本身
}