#ifndef __WTSL_CORE_NODE_LIST_H_
#define __WTSL_CORE_NODE_LIST_H_
#include "wtsl_core_node_manager.h"




/** 
 * 创建微泰星闪链表，为链表申请内存，返回空链表
 */
WTSLNodeList* create_wtsl_node_list();

/**
 * 获取当前的微泰星闪全局链表
*/
WTSLNodeList *get_wtsl_core_node_list();

/**
 * 创建微泰星闪节点 
 */
// 创建一个新的WTSLNode节点
WTSLNode* create_new_wtsl_node();

WTSLNode* create_wtsl_node(int id, WTSLNodeType type, const char *ip, int port, const char *name);

/**
 * 添加微泰星闪节点到链表尾部
 */
int add_wtsl_node_to_list_tail(WTSLNodeList *list, WTSLNode *node_data);


// 向链表头部添加节点
int add_wtsl_node_to_list_head(WTSLNodeList *list, WTSLNode *node_data);

// 获取活跃节点数量
int get_active_wtsl_node_count(WTSLNodeList *list);


// 遍历链表
void traverse_list(WTSLNodeList *list, void (*callback)(WTSLNode *node));

// 打印节点信息的回调函数
void print_node_info(WTSLNode *node);

// 通过ID查找节点
WTSLNode* find_wtsl_node_by_id(WTSLNodeList *list, int id);

// 通过Mac查找节点
WTSLNode* find_wtsl_node_by_mac(WTSLNodeList *list, const char *mac);

// 通过IP查找节点
WTSLNode* find_wtsl_node_by_ip(WTSLNodeList *list, const char *ip);

// 通过IP和端口查找节点
WTSLNode* find_wtsl_node_by_ip_port(WTSLNodeList *list, const char *ip, int port);

// 删除指定ID的节点
int delete_wtsl_node_by_id(WTSLNodeList *list, int id);

// 销毁WTSLNode节点
void destroy_wtsl_node(WTSLNode *node);

// 销毁节点链表
void destroy_wtsl_node_list(WTSLNodeList *list);

// 更新节点
int update_wtsl_node(WTSLNodeList *list, WTSLNode *pnode);

// 更新节点基础信息
int update_wtsl_node_basicinfo(WTSLNodeList *list, WTSLNodeBasicInfo *pbasic_info);

// 更新节点心跳
int update_wtsl_node_heartbeat(WTSLNodeList *list, int id);

// 调试使用，打印列表所有节点信息
void print_list_info(WTSLNodeList * list);


// 设置节点接口函数
int set_node_interface_func(WTSLNode *node, FuncIndex idx, WTSLNodeCallBack *func);



#endif