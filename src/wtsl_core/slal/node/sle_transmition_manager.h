/**
 * Copyright (c) @CompanyNameMagicTag 2022. All rights reserved.
 *
 * Description: SLE Trans Manager module.
 */

/**
 * @defgroup sle_trans_manager trans manager API
 * @ingroup  SLE
 * @{
 */

#ifndef SLE_TM_SIGNAL_H
#define SLE_TM_SIGNAL_H

#include <stdint.h>
#include "errcode.h"
#include "sle_connection_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLE_CUST_CONFIG_SIZE_MAX 251

typedef struct {
    uint8_t node_id;
    uint8_t evt_num;
    uint8_t evt_period;
    uint8_t evt_offset;
    uint8_t node_addr[6];
} sle_tm_set_node_info_t;
 
typedef struct {
    uint16_t timestamp;
} sle_tm_sync_timer_t;

typedef struct {
    uint8_t len;
    uint8_t data[GLE_CUST_CONFIG_SIZE_MAX];
} sle_set_config_t;

/**
 * @if Eng
 * @brief  Callback invoked when receiving data via direct LLCP.
 * @par Description: Callback function called when data is received through direct LLCP.
 * @param [in] con_hdl       Connection handle.
 * @param [in] data          recv data.
 * @param [in] len           len.
 * @par Depends: sle_cipher_enhance_callback_t.
 * @else
 * @brief  LLCP接收数据的回调函数。
 * @par Description: LLCP接收数据时调用的回调函数。
 * @param [in] con_hdl       连接句柄。
 * @param [in] data          接收数据。
 * @param [in] len           数据长度。
 * @par 依赖：sle_cipher_enhance_callback_t.
 * @endif
 */
typedef void (*sle_direct_llcp_recv_callback)(uint8_t con_hdl, uint8_t *data, uint32_t len);

typedef struct {
    sle_connect_status_changed_callback connect_status_cb;
    sle_direct_llcp_recv_callback direct_llcp_recv_cb;
} sle_cipher_enhance_callback_t;

/**
 * @if Eng
 * @brief Enum of sle link qos state.
 * @else
 * @brief 星闪链路忙闲状态。
 * @endif
 */
typedef enum {
    SLE_QOS_IDLE       = 0x00,    /*!< @if Eng Link state of idle
                                       @else   空闲状态 @endif */
    SLE_QOS_FLOWCTRL   = 0x01,    /*!< @if Eng Link state of flowctrl
                                       @else   流控状态 @endif */
    SLE_QOS_BUSY       = 0x02,    /*!< @if Eng Link state of busy
                                       @else   繁忙状态 @endif */
} sle_link_qos_state_t;

/**
 * @brief  GLE传输模式
 */
typedef enum {
    SLE_DEFAULT_UNICAST_DATA_TRANSMISSION_CHANNEL = 0,      /*!< @if Eng SLE Default Unicast Data Transmission Channel
                                                                 @else   SLE默认单播数据传输通道 @endif */
} sle_transmission_channel_type;

/**
 * @if Eng
 * @brief  Callback invoked when trans data busy.
 * @par Callback invoked when trans data busy.
 * @param [in] conn_id       connection ID.
 * @param [in] link_state    link state.
 * @par Depends:
 * @see  sle_transmission_callbacks_t
 * @else
 * @brief  发送数据繁忙回调函数。
 * @par Description: 发送数据繁忙回调函数。
 * @param [in] conn_id       发送数据繁忙回调信息.
 * @param [in] link_state    链路状态。
 * @par 依赖：
 * @see  sle_transmission_callbacks_t
 * @endif
 */
typedef void (*sle_trans_data_busy_callback)(uint16_t conn_id, sle_link_qos_state_t link_state);

/**
 * @if Eng
 * @brief Struct of SLE transmission manager callback function.
 * @else
 * @brief SLE传输管理回调函数接口定义。
 * @endif
 */
typedef struct {
    sle_trans_data_busy_callback send_data_cb;             /*!< @if Eng Trans data busy callback.
                                                                @else   传输数据繁忙回调函数。 @endif */
} sle_transmission_callbacks_t;

/**
 * @brief  连接管理查询能力比特位参数
 */
typedef struct {
    uint32_t relay_capability : 1;         /*!< @if Eng relay capability
                                                @else   中继能力 @endif */
    uint32_t trans_mode : 1;               /*!< @if Eng trans mode
                                                @else   传输模式 @endif */
    uint32_t measurement_capability : 1;   /*!< @if Eng measurement capability
                                                @else   测量能力 @endif */
    uint32_t access_sle : 1;               /*!< @if Eng access sle
                                                @else   sle接入 @endif */
    uint32_t access_slb : 1;               /*!< @if Eng access slb
                                                @else   slb接入 @endif */
    uint32_t mtu : 1;                      /*!< @if Eng max mtu
                                                @else   最大支持mtu @endif */
    uint32_t mps : 1;                      /*!< @if Eng max mps
                                                @else   最大支持mps @endif */
    uint32_t reverse : 25;                 /*!< @if Eng reverse
                                                @else   保留比特位 @endif */
} sle_transmission_signal_capability_bit_t;

/**
 * @if Eng
 * @brief  Send signal capability request.
 * @par Description: Send signal capability request.
 * @param [in] conn_id   Connection id.
 * @param [in] param     Capability info.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  发送连接管理能力查询请求。
 * @par Description: 发送连接管理能力查询请求。
 * @param [in] conn_id   连接 ID。
 * @param [in] param     查询的能力信息。
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_transmission_signal_capability_request(uint16_t conn_id, const sle_transmission_signal_capability_bit_t *param);

/**
 * @if Eng
 * @brief  Register SLE transmission manager callbacks.
 * @par Description: Register SLE transmission manager callbacks.
 * @param  [in]  func Callback function.
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  注册SLE传输管理回调函数。
 * @par Description: 注册SLE传输管理回调函数。
 * @param  [in]  func 回调函数。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_transmission_register_callbacks(sle_transmission_callbacks_t *func);

/**
 * @if Eng
 * @brief  Create SLE default transmission channel.
 * @par Description: Create SLE default transmission channel.
 * @param  [in]  channel_type   channel type. see @ref sle_transmission_channel_type
 * @param  [out] tc_handle  transmission channel handle.
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  创建SLE缺省传输通道。
 * @par Description: 创建SLE缺省传输通道。
 * @param  [in]  channel_type   传输通道类型。参考 @ref sle_transmission_channel_type
 * @param  [out] tc_handle  传输通道句柄。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_transmission_create_default_trans_channel(uint8_t channel_type, uint8_t *tc_handle);

/**
 * @if Eng
 * @brief  Send fast data.
 * @par Description: Send fast data.
 * @param [in] conn_id   Connection id.
 * @param [in] data      Send data.
 * @param [in] data_len  Send data len.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  发送快速通道数据。
 * @par Description: 发送快速通道数据。
 * @param [in] conn_id   连接 ID。
 * @param [in] data      待发送数据。
 * @param [in] data_len  待发送数据长度。
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_tm_send_fast_data(uint16_t conn_id, uint8_t *data, uint16_t data_len);
 
typedef void (*fast_data_cbk)(uint16_t rmt_hdl, uint8_t *data, uint16_t data_len);
 
/**
 * @if Eng
 * @brief  fast register cbks.
 * @par Description:  fast register cbks.
 * @param [in] cbk
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  接收快速通道数据回调注册。
 * @par Description: 接收快速通道数据回调注册。
 * @param [in] cbk       接收快速通道数据回调函数。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_tm_fast_register_cbks(fast_data_cbk cbk);
 
/**
 * @if Eng
 * @brief  set node info.
 * @par Description:  set node info.
 * @param [in] node_info
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  设置节点信息。
 * @par Description: 设置节点信息。
 * @param [in] node_info       设置节点信息。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_tm_set_node_info(sle_tm_set_node_info_t *node_info);
 
/**
 * @if Eng
 * @brief  sync timer.
 * @par Description:  sync timer.
 * @param [in] timer
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  同步定时器。
 * @par Description: 同步定时器。
 * @param [in] timer       定时器函数。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_tm_sync_timer(sle_tm_sync_timer_t *timer);

/**
 * @if Eng
 * @brief  Set custom configuration.
 * @par Description: Set custom configuration.
 * @param [in] config
 * @retval ERRCODE_SUCC Success.
 * @retval Other        Failure. For details, see @ref errcode_t
 * @else
 * @brief  设置配置参数。
 * @par Description: 设置配置参数。
 * @param [in] config       指向配置参数结构体的指针。
 * @retval ERRCODE_SUCC 成功。
 * @retval Other        失败。参考 @ref errcode_t
 * @endif
 */
errcode_t sle_tm_custom_set_config(const sle_set_config_t *config);

#ifdef __cplusplus
}
#endif
#endif /* SLE_TM_SIGNAL */
/**
 * @}
 */
