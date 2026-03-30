
#include <sys/time.h>
#include <arpa/inet.h>
#include "wtsl_log_manager.h"
#include "wtsl_core_node_manager.h"

//全局的星闪节点list
static WTSLNodeList *gs_pListNodes = NULL;


#define MODULE_NAME "node_list"

WTSLNodeList* create_wtsl_node_list() {
    WTSLNodeList *list = (WTSLNodeList*)malloc(sizeof(WTSLNodeList));
    if (list) {
        list->head = NULL;
        list->tail = NULL;
        list->node_count = 0;
    }
    gs_pListNodes = list;
    return list;
}

//获取全局链表
WTSLNodeList *get_wtsl_core_node_list(){
    return gs_pListNodes;
}

// 创建节点接口表
static WTSLNodeInterface* create_node_interface(void) {
    WTSLNodeInterface *iface = (WTSLNodeInterface*)malloc(sizeof(WTSLNodeInterface));
    if (iface) {
        memcpy(iface, &default_interface, sizeof(WTSLNodeInterface));
    }
    return iface;
}


// 创建一个新的WTSLNode节点
WTSLNode* create_new_wtsl_node(){
    WTSLNode *node = (WTSLNode*)malloc(sizeof(WTSLNode));
    if (node == NULL) {
        return NULL;
    }
    // node->last_heartbeat = time(NULL);
    // 分配并初始化接口表
    node->interface = create_node_interface();
    if (!node->interface) {
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] create node interface error",__FUNCTION__,__LINE__);
        free(node);
        return NULL;
    }
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d] interface:%p",__FUNCTION__,__LINE__,node->interface);
    node->perm_map = global_perm_map;

    return node;
}
// 创建一个新的WTSLNode节点
WTSLNode* create_wtsl_node(int id, WTSLNodeType type, const char *ip, int port, const char *name) {
    WTSLNode *node = (WTSLNode*)malloc(sizeof(WTSLNode));
    if (node == NULL) {
        return NULL;
    }
    // 初始化基本属性
    node->id = id;
    node->last_heartbeat = time(NULL);
    node->type = type;
    // 初始化地址信息

    memset(&node->addr, 0, sizeof(struct sockaddr_in));
    node->addr.sin_family = AF_INET;
    node->addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &node->addr.sin_addr) <= 0) {
        free(node);
        return NULL;
    }
    // 初始化节点信息
    memset(&node->info, 0, sizeof(WTSLNodeInfo));
    strncpy(node->info.basic_info.name, name, sizeof(node->info.basic_info.name) - 1);
    node->info.basic_info.name[sizeof(node->info.basic_info.name) - 1] = '\0';
    // 分配并初始化接口表
    node->interface = create_node_interface();
    if (!node->interface) {
        free(node);
        return NULL;
    }
    return node;
}

// 设置节点接口函数
// int set_wtsl_node_interface_func(WTSLNode *node, FuncIndex idx, WTSLNodeCallBack *func) {
//     if (!node || !node->interface || idx < 0 || idx >= FUNC_COUNT) {
//         return -1;
//     }
    
//     // 使用偏移量设置函数指针
//     WTSLNodeCallBack **func_ptr = (WTSLNodeCallBack**)((char*)node->interface + func_offsets[idx]);
//     *func_ptr = func;
    
//     return 0;
// }



// 设置节点的特定接口实现
// int set_node_interface_func(WTSLNode *node, 
//                            void (**func_ptr)(void*, void*, unsigned int), 
//                            WTSLNodeCallBack *func) {
//     if (!node || !func_ptr || !node->interface) {
//         return -1;
//     }
//     *func_ptr = func;
//     return 0;
// }


int add_wtsl_node_to_list_tail(WTSLNodeList *list, WTSLNode *node_data) {
    if (!list || !node_data) return -1;
    
    WTSLListNode *new_node = (WTSLListNode*)malloc(sizeof(WTSLListNode));
    if (!new_node){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] no memory for malloc WTSLListNode",__FUNCTION__,__LINE__);
        return -1;
    } 
    
    memcpy(&new_node->node, node_data, sizeof(WTSLNode));
    new_node->next = NULL;
    
    if (!list->head) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        list->tail = new_node;
    }
    list->node_count++;
    return 0;
}


// 向链表头部添加节点
int add_wtsl_node_to_list_head(WTSLNodeList *list, WTSLNode *node_data) {
    if (!list || !node_data) return -1;
    
    // 创建新的链表节点
    WTSLListNode *new_node = (WTSLListNode*)malloc(sizeof(WTSLListNode));
    if (!new_node){
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] no memory for malloc WTSLListNode",__FUNCTION__,__LINE__);
        return -1;
    }
    // 复制节点数据
    memcpy(&new_node->node, node_data, sizeof(WTSLNode));
    new_node->next = list->head;  // 新节点指向原头节点
    
    // 更新链表头指针
    list->head = new_node;
    
    // 如果是第一个节点，同时更新尾指针
    if (!list->tail) {
        list->tail = new_node;
    }
    
    list->node_count++;
    return 0;
}

// 获取活跃节点数量
int get_active_wtsl_node_count(WTSLNodeList *list) {
    if (!list) return -1;
    
    int count = 0;
    WTSLListNode *current = list->head;
    
    while (current) {
        if (current->node.active) {
            count++;
        }
        current = current->next;
    }
    
    return count;
}

// 通过ID查找节点
WTSLNode* find_wtsl_node_by_id(WTSLNodeList *list, int id) {
    if (!list) return NULL;
    
    WTSLListNode *current = list->head;
    while (current) {
        if (current->node.id == id) {
            return &(current->node);
        }
        current =  current->next;
    }

    return NULL;
}

// 通过MAC查找节点
WTSLNode* find_wtsl_node_by_mac(WTSLNodeList *list, const char *mac){
    if (!list || !mac) return NULL;
    
    WTSLListNode *current = list->head;
    while (current) {
        // WTSL_LOG_DEBUG(MODULE_NAME,"in mac str:%s,node mac:%s",mac,current->node.info.basic_info.mac);
        if (strncasecmp(current->node.info.basic_info.mac, mac, strlen(mac)) == 0) {
            // WTSL_LOG_DEBUG(MODULE_NAME,"find the mac str:%s,node mac:%s",mac,current->node.info.basic_info.mac);
            return &(current->node);
        }
        current = current->next;
    }
    
    return NULL;
}

// 通过IP查找节点
WTSLNode* find_wtsl_node_by_ip(WTSLNodeList *list, const char *ip) {
    if (!list || !ip) return NULL;
    
    struct in_addr target_ip;
    if (inet_pton(AF_INET, ip, &target_ip) <= 0) {
        return NULL;
    }
    
    WTSLListNode *current = list->head;
    while (current) {
        if (memcmp(&current->node.addr.sin_addr, &target_ip, sizeof(struct in_addr)) == 0) {
            return &(current->node);
        }
        current = current->next;
    }
    
    return NULL;
}

// 通过IP和端口查找节点
WTSLNode* find_wtsl_node_by_ip_port(WTSLNodeList *list, const char *ip, int port) {
    if (!list || !ip) return NULL;
    
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &target_addr.sin_addr) <= 0) {
        return NULL;
    }
    
    WTSLListNode *current = list->head;
    while (current) {
        if (current->node.addr.sin_port == target_addr.sin_port &&
            memcmp(&current->node.addr.sin_addr, &target_addr.sin_addr, sizeof(struct in_addr)) == 0) {
            return &(current->node);
        }
        current = current->next;
    }
    
    return NULL;
}

// 删除指定ID的节点
int delete_wtsl_node_by_id(WTSLNodeList *list, int id) {
    if (!list || !list->head) return -1;
    
    WTSLListNode *current = list->head;
    WTSLListNode *prev = NULL;
    
    while (current && current->node.id != id) {
        prev = current;
        current = current->next;
    }
    
    if (!current) return -1;
    
    // 调整链表指针
    if (!prev) {
        list->head = current->next;
    } else {
        prev->next = current->next;
    }
    
    // 如果删除的是尾节点
    if (list->tail == current) {
        list->tail = prev;
    }
    
    // 释放节点资源
    if (current->node.interface) {
        free(current->node.interface);
    }
    free(current);
    list->node_count--;
    
    return 0;
}


// 遍历链表
void traverse_list(WTSLNodeList *list, void (*callback)(WTSLNode *node)) {
    if (!list || !callback) return;
    
    WTSLListNode *current = list->head;
    while (current) {
        callback(&current->node);
        current = current->next;
    }
}

// 打印节点信息的回调函数
void print_node_info(WTSLNode *node) {
    if (!node) return;
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &node->addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    
    WTSL_LOG_INFO(MODULE_NAME, "Node ID: %d", node->id);
    WTSL_LOG_INFO(MODULE_NAME, "Name: %s", node->info.basic_info.name);
    WTSL_LOG_INFO(MODULE_NAME, "IP Address: %s:%d", ip_str, ntohs(node->addr.sin_port));
    WTSL_LOG_INFO(MODULE_NAME, "MAC Address: %s", node->info.basic_info.mac);
    WTSL_LOG_INFO(MODULE_NAME, "Type: %d", node->info.basic_info.type);
    WTSL_LOG_INFO(MODULE_NAME, "Active: %s", node->active ? "Yes" : "No");
}



void print_list_info(WTSLNodeList * list){
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
    if(gs_pListNodes != NULL){
        WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
        traverse_list(list,print_node_info);
    }
    WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d]",__FUNCTION__,__LINE__);
}


// 销毁WTSLNode节点
void destroy_wtsl_node(WTSLNode *node) {
    if (!node) return;
    
    if (node->interface) {
        free(node->interface);
    }
    free(node);
}

// 销毁节点链表
void destroy_wtsl_node_list(WTSLNodeList *list) {
    if (!list) return;
    
    WTSLListNode *current = list->head;
    WTSLListNode *next;
    
    while (current) {
        next = current->next;
        // 释放节点的接口表
        if (current->node.interface) {
            free(current->node.interface);
        }
        free(current);
        current = next;
    }
    
    free(list);
}

int update_wtsl_node_basicinfo(WTSLNodeList *list, WTSLNodeBasicInfo *pnode_basic_info){
    WTSLNode *list_of_node = find_wtsl_node_by_mac(list,pnode_basic_info->mac);
    if(list_of_node != NULL){
        memcpy(&list_of_node->info.basic_info,pnode_basic_info,sizeof(WTSLNodeBasicInfo));
        // WTSL_LOG_DEBUG(MODULE_NAME,"[%s][%d] update node success",__FUNCTION__,__LINE__);
        return 0;
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]can't update the node type:%d:(name:%s,mac: %s)",__FUNCTION__,__LINE__,pnode_basic_info->type,pnode_basic_info->name,pnode_basic_info->mac);
    return -1;
}


int update_wtsl_node(WTSLNodeList *list, WTSLNode *pnode){
    WTSLNode *list_of_node = find_wtsl_node_by_mac(list,pnode->info.basic_info.mac);
    if(list_of_node != NULL){
        memcpy(list_of_node,pnode,sizeof(WTSLNode));
        WTSL_LOG_DEBUG(MODULE_NAME,"update node success");
        return 0;
    }
    WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d]can't update the node[%d]:(name:%s,mac: %s)",__FUNCTION__,__LINE__,pnode->id,pnode->info.basic_info.name,pnode->info.basic_info.mac);
    return -1;
}

// 更新节点心跳
int update_wtsl_node_heartbeat(WTSLNodeList *list, int id) {
    WTSLNode *node = find_wtsl_node_by_id(list, id);
    if (!node) {
        WTSL_LOG_ERROR(MODULE_NAME,"[%s][%d] can't find node[%d]",__FUNCTION__,__LINE__,id);
        return -1;
    }
    
    node->last_heartbeat = time(NULL);
    node->active = 1;
    return 0;
}


#ifdef UNIT_TEST

#endif