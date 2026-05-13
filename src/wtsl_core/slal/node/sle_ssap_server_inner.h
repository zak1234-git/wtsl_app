/**
 * Copyright (c) @CompanyNameMagicTag 2022. All rights reserved.
 *
 * Description: SLE Service Access Protocol SERVER Inner module.
 */

/**
 * @defgroup sle_ssap_server Service Access Protocol SERVER API
 * @ingroup  SLE
 * @{
 */

#ifndef SLE_SSAP_SERVER_INNER_H
#define SLE_SSAP_SERVER_INNER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @if Eng
 * @brief Struct of call method request information.
 * @else
 * @brief 方法调用请求信息。
 * @endif
 */
typedef struct {
    uint16_t request_id;  /*!< @if Eng Request id.
                               @else   请求id。 @endif */
    uint16_t handle;      /*!< @if Eng method handle of the call method request.
                               @else   方法调用请求对应方法的句柄。 @endif */
    sle_uuid_t uuid;      /*!< @if Eng method uuid of method called.
                               @else   请求方法的特征描述符。 @endif */
    bool need_rsp;        /*!< @if Eng Whether response is needed.
                               @else   是否需要发送响应。 @endif */
    uint16_t length;      /*!< @if Eng Length of write request data.
                               @else   请求传入的数据长度。 @endif */
    uint8_t *value;       /*!< @if Eng Write request data.
                               @else   请求传入的数据。 @endif */
} ssaps_req_call_method_cb_t;


/**
 * @if Eng
 * @brief Callback invoked when receive call method request.
 * @par Callback invoked when  receive call method request.
 * @attention 1.This function is called in SLE service context,should not be blocked or do long time waiting.
 * @attention 2. The memories of pointer are requested and freed by the SLE service automatically.
 * @param [in] server_id     server ID.
 * @param [in] conn_id       connection ID.
 * @param [in] method_cb_para call method request parameter.
 * @retval #void no return value.
 * @par Dependency:
 * @li  sle_ssap_stru.h
 * @see sle_ssaps_callbacks_t
 * @else
 * @brief  收到方法调用请求的回调函数。
 * @par    收到方法调用请求的回调函数。
 * @attention  1. 该回调函数运行于SLE service线程，不能阻塞或长时间等待。
 * @attention  2. pointer由SLE service申请内存，也由SLE service释放，回调中不应释放。
 * @param [in] server_id     服务端 ID。
 * @param [in] conn_id       连接 ID。
 * @param [in] method_cb_para 方法调用请求参数。
 * @retval 无返回值。
 * @par 依赖:
 * @li  sle_ssap_stru.h
 * @see sle_ssaps_callbacks_t
 * @endif
 */
typedef void (*ssaps_call_method_request_callback)(uint8_t server_id, uint16_t conn_id,
    ssaps_req_call_method_cb_t *call_method_cb_para);

/**
 * @if Eng
 * @brief Struct of SSAP server service callback function.
 * @else
 * @brief SSAP server Service回调函数接口定义。
 * @endif
 */
typedef struct {
    ssaps_indicate_cfm_callback indicate_cfm_cb;                   /*!< @if Eng Indicate cfm callback.
                                                                        @else   指示确认回调函数。 @endif */
    ssaps_read_request_callback read_request_cb;                   /*!< @if Eng Read request received callback.
                                                                        @else   收到远端读请求回调函数。 @endif */
    ssaps_write_request_callback write_request_cb;                 /*!< @if Eng Write request received callback.
                                                                        @else   收到远端写请求回调函数。 @endif */
    ssaps_call_method_request_callback call_method_request_cb;     /*!< @if Eng Call method request received callback.
                                                                        @else   收到远端方法调用回调函数。 @endif */
} service_callbacks_t;

/**
 * @if Eng
 * @brief  Register SSAP service callbacks with handle.
 * @par Description: Register SSAP server service callbacks.
 * @param  [in] func callback function.
 * @param  [in] start_hdl start handle
 * @param  [in] end_hdl end handle
 * @retval error code.
 * @par Depends:
 * @li sle_ssap_stru.h
 * @else
 * @brief  注册 SSAP server 服务回调函数。
 * @par Description: 按照handle注册回调函数。
 * @param  [in] func 回调函数
 * @param  [in] start_hdl 起始handle
 * @param  [in] end_hdl 结束handle
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li sle_ssap_stru.h
 * @endif
 */
errcode_t ssaps_register_service_callbacks(service_callbacks_t *func, uint16_t start_hdl, uint16_t end_hdl);

#ifdef __cplusplus
}
#endif
#endif