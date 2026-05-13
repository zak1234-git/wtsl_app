/**
 * Copyright (c) @CompanyNameMagicTag 2022. All rights reserved.
 *
 * Description: SLE Trans DATA Manager module.
 */

/**
 * @defgroup sle_trans_manager trans manager API
 * @ingroup  SLE
 * @{
 */

#ifndef SLE_TRANS_DATA_MANAGER_H
#define SLE_TRANS_DATA_MANAGER_H

#include <stdint.h>
#include "errcode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t conn_id;            /*!< @if Eng Connection ID.
                                      @else   链路ID。 @endif */
    uint16_t src_port;           /*!< @if Eng Source port.
                                      @else   源端口。 @endif */
    uint16_t dst_port;           /*!< @if Eng Destination port.
                                      @else   目的端口。 @endif */
} sle_connectionless_pkt_header_t;

/**
 * @if Eng
 * @brief  Callback invoked when connectionless trans data is received.
 * @par Callback invoked when connectionless trans data is received.
 * @param [in] tc_handle     Transmission channel handle.
 * @param [in] data_header   Data header. { @ref sle_connectionless_pkt_header_t }.
 * @param [in] data          Data.
 * @param [in] data_len      Data length.
 * @par Depends:
 * @else
 * @brief  收到无连接传输数据时调用的回调。
 * @par Description: 收到无连接传输数据时调用的回调。
 * @param [in] tc_handle     传输通道句柄。
 * @param [in] data_header   业务数据头。 { @ref sle_connectionless_pkt_header_t }。
 * @param [in] data          业务数据。
 * @param [in] data_len      数据长度。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
typedef void (*sle_receive_connectionless_trans_data_callback)(uint8_t tc_handle,
    const sle_connectionless_pkt_header_t *data_header, const uint8_t *data, uint16_t data_len);

/**
 * @if Eng
 * @brief  Register SLE connectionless trans data callbacks.
 * @par Description: Register SLE connectionless trans data callbacks.
 * @param [in] port      Port.
 * @param [in] func      Callback function.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  注册无连接传输数据回调接口
 * @par Description: 注册无连接传输数据回调接口。
 * @param [in] port      端口。
 * @param [in] func      收到数据对应的回调函数。
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_register_connectionless_trans_data_callback(uint16_t port,
    sle_receive_connectionless_trans_data_callback func);

/**
 * @if Eng
 * @brief  Unregister SLE connectionless trans data callbacks.
 * @par Description: Unregister SLE connectionless trans data callbacks.
 * @param [in] port      Port.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  取消注册端口数据回调接口
 * @par Description: 取消注册端口数据回调接口。
 * @param [in] port      端口.
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_unregister_connectionless_trans_data_callback(uint16_t port);

/**
 * @if Eng
 * @brief  Send connectionless trans data.
 * @par Description: Send connectionless trans data.
 * @param [in] tc_handle     Transmission channel handle.
 * @param [in] data_header   Data header. { @ref sle_connectionless_pkt_header_t }.
 * @param [in] data          Data.
 * @param [in] data_len      Data length.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  发送无连接传输数据。
 * @par Description: 发送无连接传输数据。
 * @param [in] tc_handle     传输通道句柄。
 * @param [in] data_header   业务数据头。 { @ref sle_connectionless_pkt_header_t }。
 * @param [in] data          业务数据。
 * @param [in] data_len      数据长度。
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_send_connectionless_trans_data(uint8_t tc_handle,
    sle_connectionless_pkt_header_t *data_header, uint8_t *data, uint16_t data_len);

/**
 * @if Eng
 * @brief  Set connectionless trans data optional mask.
 * @par Description: Set light weight data optional mask.
 * @param [in] port          Port.
 * @param [in] optional_mask Optional field mask.
 * @retval error code
 * @par Depends:
 * @li  sle_common.h
 * @else
 * @brief  设置无连接传输数据可选字段标识。
 * @par Description: 设置数据可选字段标识。
 * @param [in] port          端口.
 * @param [in] optional_mask 可选字段标识。
 * @retval 执行结果错误码。
 * @par 依赖：
 * @li  sle_common.h
 * @endif
 */
errcode_t sle_set_connectionless_trans_data_optional_mask(uint16_t port, uint16_t optional_mask);

#ifdef __cplusplus
}
#endif
#endif /* SLE_TRANS_DATA_MANAGER_H */
/**
 * @}
 */
