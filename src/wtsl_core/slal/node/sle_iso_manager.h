/**
 * Copyright (c) @CompanyNameMagicTag 2023. All rights reserved.
 *
 * Description: SLE IOS Connection Manager module.
 */

/**
 * @defgroup sle_iso_manager connection manager API
 * @ingroup  SLE
 * @{
 */

#ifndef SLE_ISO_CONNECTION_MANAGER_H
#define SLE_ISO_CONNECTION_MANAGER_H

#include <stdint.h>
#include "errcode.h"
#include "sle_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISO_FRAMED_MAX 0x1
#define ISO_G2T_SDU_INTV_MIN 0xFF
#define ISO_G2T_SDU_INTV_MAX 0xFFFFF
#define ISO_T2G_SDU_INTV_MIN 0xFF
#define ISO_T2G_SDU_INTV_MAX 0xFFFFF
#define ISO_G2T_SDU_SIZE_MAX 0xFFF
#define ISO_T2G_SDU_SIZE_MAX 0xFFF

#define ISO_G2T_LATENCY_MIN 0x5
#define ISO_G2T_LATENCY_MAX 0x0FA0
#define ISO_T2G_LATENCY_MIN 0x5
#define ISO_T2G_LATENCY_MAX 0x0FA0

/**
 * @if Eng
 * @brief iso connection type definition.
 * @else
 * @brief 星闪ISO通道类型定义。
 * @endif
 */
typedef enum {
    SLE_ISO_CONNECTION_TYPE_MULTICAST    = 0,    /*!< @if Eng sle isochronous multicast connection.
                                                         @else   星闪同步组播链路 @endif */
    SLE_ISO_CONNECTION_TYPE_UNICAST   = 1,       /*!< @if Eng sle Asynchronous unicast connection.
                                                         @else   星闪同步单播链路 @endif */
    SLE_ISO_CONNECTION_END
} sle_iso_connection_type_t;

/**
 * @if Eng
 * @brief Packet Sending Sequence Type Definition.
 * @else
 * @brief 发包顺序类型定义。
 * @endif
 */
typedef enum {
    SLE_DATA_TYPE_SEQUENCE      = 0,  /*!< @if Eng The carried data is the data that is sent in a specified sequence.
                                         @else   承载的数据是指定顺序发包的数据 @endif */
    SLE_DATA_TYPE_CROSS_        = 1,  /*!< @if Eng The carried data is the data of the specified cross-packe.
                                         @else   承载的数据是指定交叉发包的数据 @endif */
    SLE_DATA_TYPE_MIXED         = 2,  /*!< @if Eng The carried data is the data of the specified mixed packet.
                                         @else   承载的数据是指定混合发包的数据 @endif */
    SLE_DATA_TYPE_SEQUENCE_END
} sle_packing_type_t;

/**
 * @if Eng
 * @brief Packet Slice Type Definition.
 * @else
 * @brief 数据包切片类型定义。
 * @endif
 */
typedef enum {
    SLE_DATA_TYPE_UNFRAMED    = 0,  /*!< @if Eng The carried data is the specified unframed data.
                                         @else   承载的数据是指定未切分的数据 @endif */
    SLE_DATA_TYPE_FRAMED      = 1,  /*!< @if Eng The carried data is the specified framed data.
                                         @else   承载的数据是指定切分的数据 @endif */
    SLE_DATA_TYPE_FRAMED_END
} sle_framing_type_t;

/**
 * @if Eng
 * @brief Enum of sle iso connection state.
 * @else
 * @brief SLE ISO通道连接状态。
 * @endif
 */
typedef enum {
    SLE_ISO_CONNECTION_STATE_NONE          = 0x00,   /*!< @if Eng SLE Iso connect state of none
                                                                @else   SLE ISO连接 未连接状态 @endif */
    SLE_ISO_CONNECTION_STATE_CONNECTED     = 0x01,   /*!< @if Eng SLE Iso connect state of connected
                                                                @else   SLE ISO连接 已连接 @endif */
    SLE_ISO_CONNECTION_STATE_DISCONNECTED  = 0x02,   /*!< @if Eng SLE Iso connect state of disconnected
                                                                @else   SLE ISO连接 已断接 @endif */
    SLE_ISO_CONNECTION_STATE_REJECT        = 0x03,   /*!< @if Eng Iso connection establishment rejected
                                                                @else   SLE ISO连接，连接建立被拒绝 @endif */
    SLE_ISO_CONNECTION_STATE_END
} sle_iso_connect_state_t;

/**
 * @if Eng
 * @brief Response type of the iso connection request event.
 * @else
 * @brief ISO连接请求事件响应类型定义
 * @endif
 */
typedef enum {
    SLE_ISO_REQUEST_RESULT_ACCEPT          = 0x00,  /*!< @if Eng SLE Iso Connection Response Accept Request
                                                               @else   SLE ISO连接响应 接受请求 @endif */
    SLE_ISO_REQUEST_RESULT_REJECT          = 0x01,  /*!< @if Eng SLE Iso Connection Response Reject Request
                                                               @else   SLE ISO连接响应 拒绝请求 @endif */
    SLE_ISO_REQUEST_RESULT_END
} sle_iso_connect_result_t;

/**
 * @if Eng
 * @brief ISO Disconnection Cause Definition.
 * @else
 * @brief ISO连接断开原因定义
 * @endif
 */
typedef enum {
    SLE_ISO_DISCONNECT_BY_REMOTE = 0x10,    /*!< @if Eng ISO disconnect by remote
                                                       @else   ISO远端断链 @endif */
    SLE_ISO_DISCONNECT_BY_LOCAL  = 0x11,    /*!< @if Eng ISO disconnect by local
                                                       @else   ISO本端断链 @endif */
    SLE_ISO_DISCONNECT_REASON_END,
} sle_iso_disc_reason_t;

/**
 * @if Eng
 * @brief Enum of sle ISO set icb data path direction
 * @else
 * @brief 设置icb data path的方向定义
 * @endif
 */
typedef enum {
    SLE_ISO_DATA_PATH_DIRECTION_INPUT    = 0x00,   /*!< @if Eng Input (from the host side to the controller side)
                                                        @else   输入(Host侧到Controller侧) @endif */
    SLE_ISO_DATA_PATH_DIRECTION_OUTPUT   = 0x01,   /*!< @if Output (from the controller side to the host side)
                                                        @else   输出(Controller侧到Host侧) @endif */
} sle_iso_data_path_direction_t;

/**
 * @if Eng
 * @brief Definition of ISO Connection Setup SDU, PHY and Radio Frame Type Parameters.
 * @else
 * @brief ISO连接建立SDU，PHY、无线帧类型参数定义
 * @endif
 */
typedef struct {
    uint16_t max_sdu;       /*!< @if Eng Maximum number of bytes in a payload.
                                 @else   Payload的最大字节数，以字节为单位 @endif */
    uint8_t rtn;            /*!< @if Eng TNumber of retransmissions of each IMB data PDU.
                                 @else   每一个IMB数据PDU包重传的次数。 @endif */
    uint8_t resv;           /*!< @if Eng 预留
                                 @else   Reserved。 @endif */
} sle_iso_member_param_t;

/**
 * @if Eng
 * @brief Definitions of Parameters for Establishing iso Connections.
 * @else
 * @brief ISO连接建立参数定义
 * @endif
 */
typedef struct {
    uint32_t interval_g2t;              /*!< @if Eng Interval between two consecutive SDUs, in us
                                             @else   G->T两个连续的SDU之间的时间间隔，以us为单位 @endif */
    uint32_t interval_t2g;              /*!< @if Eng Interval between two consecutive SDUs, in us
                                             @else   T->G两个连续的SDU之间的时间间隔，以us为单位 @endif */
    uint16_t latency_g2t;               /*!< @if Eng latency
                                             @else   G->T延迟周期，单位slot @endif */
    uint16_t latency_t2g;               /*!< @if Eng latency
                                             @else   T->G延迟周期，单位slot @endif */
    uint8_t packet_type;                /*!< @if Eng The carried data is the packet sending sequential data,
                                             @else 承载数据发包顺序类型，参考 { @ref sle_packing_type_t }。 @endif */
    uint8_t frame_type;                 /*!< @if Eng The carried data is unframed or framed { @ref sle_framing_type_t }
                                             @else   承载数据帧分片类型，参考 { @ref sle_framing_type_t }。 @endif */
    uint8_t resv;                       /*!< @if Eng 预留
                                             @else Reserved。 @endif */
    uint16_t member_num;                /*!< @if Eng Number of connections in a iso group.
                                             @else   ISO组中连接个数 @endif */
    sle_iso_member_param_t *g2t;        /*!< @if Eng Connection parameters of the group node in a iso group.
                                                  @else   ISO组中组长节点的连接参数 @endif */
    sle_iso_member_param_t *t2g;        /*!< @if Eng Connection parameters of member nodes in a iso group.
                                                  @else   ISO组中组员节点的连接参数 @endif */
} sle_iso_connection_param_t;

/**
 * @if Eng
 * @brief Definitions of Parameters for Establishing iso Connections.
 * @else
 * @brief ISO连接建立默认参数定义
 * @endif
 */
typedef struct {
    uint32_t sdu_interval;      /*!< @if Eng Parameters for creating a synchronization link of a iso group.
                                    @else   GT两个连续的SDU之间的时间间隔，以us为单位 @endif */
    uint16_t latency;           /*!< @if Eng Parameters for creating a synchronization link of a iso group.
                                    @else   GT延迟周期，单位slot @endif */
    uint16_t max_sdu;           /*!< @if Eng Maximum number of bytes in a payload.
                                    @else   Payload的最大字节数，以字节为单位 @endif */
    uint8_t rtn;                /*!< @if Eng TNumber of retransmissions of each IMB data PDU.
                                    @else   每一个IMB数据PDU包重传的次数。 @endif */
} sle_iso_default_connection_param_t;
typedef struct {
    uint8_t  packet_type;       /*!< @if Eng The carried data is the packet sending sequential data,
                                  { @ref sle_packing_type_t }.
                                  @else 承载数据发包顺序类型，参考 { @ref sle_packing_type_t }。 @endif */
    uint8_t  frame_type;        /*!< @if Eng The carried data is the unframed or framed data @ref sle_framing_type_t
                                  @else 承载数据帧分片类型，参考 { @ref sle_framing_type_t }。 @endif */
} sle_iso_default_common_param_t;

typedef struct {
    sle_iso_default_common_param_t common_param;
    sle_iso_default_connection_param_t phy_g2t; /*!< @if Eng Interval between two consecutive SDUs, in us
                                                               @else   G->T默认连接参数 @endif */
    sle_iso_default_connection_param_t phy_t2g; /*!< @if Eng Interval between two consecutive SDUs, in us
                                                               @else   T->G默认连接参数 @endif */
} sle_iso_default_group_param_t;

/**
 * @if Eng
 * @brief Definitions of Parameters for Establishing Iso Connections.
 * @else
 * @brief ISO连接建立参数定义
 * @endif
 */
typedef struct {
    uint16_t member_id; /*!< @if Eng iso member ID returned in { @ref sle_iso_create_group_callback }
                             @else   ISO组成员ID由 { @ref sle_iso_create_group_callback } 中返回 @endif */
    uint16_t conn_id;   /*!< @if Eng ID of the basic connection that has been established between node G and node T.
                             @else   G节点已经与T节点建立的基础连接ID由 { @ref sle_connect_state_changed_callback }中返回
                           @endif */
} sle_iso_connection_info_t;

/**
 * @if Eng
 * @brief Definitions of Parameters for updating ISO connection status.
 * @else
 * @brief 组播连接状态更新参数
 * @endif
 */
typedef struct {
    sle_iso_connect_state_t conn_state;   /*!< @if Eng SLE ISO connection status
                                               @else   SLE 组播连接状态 @endif */
    sle_iso_disc_reason_t disc_reason;   /*!< @if Eng ISO Connection Disconnection Cause
                                              @else   组播连接断开原因 @endif */
} sle_iso_connect_state_info_t;

/**
 * @if Eng
 * @brief Definitions of Parameters for Establishing ISO Connections.
 * @else
 * @brief ISO连接信息参数定义
 * @endif
 */
typedef struct {
    uint16_t member_id;   /*!< @if Eng Iso member ID returned in { @ref sle_iso_create_group_callback }
                               @else    ISO组成员ID由 { @ref sle_iso_create_group_callback } 中返回 @endif */
    uint16_t mem_conn_id;  /*!< @if Eng ID of the Iso connection established.
                                @else   已建立的ISO通道ID由 { @ref sle_iso_connect_state_changed_callback }中返回 @endif */
    uint16_t acb_conn_id;  /*!< @if Eng ID of the basic connection that has been established between node G and node T.
                                @else   G节点与T节点建立的基础连接ID由 { @ref sle_connect_state_changed_callback }中返回 @endif */
} sle_iso_member_info_t;

/**
 * @if Eng
 * @brief Iso connection create request parameter definition.
 * @else
 * @brief ISO连接建立请求参数定义
 * @endif
 */
typedef struct {
    uint16_t max_pdu_g2t; /*!< @if Eng Maximum length from the G node to the T node, in bytes.
                               @else   G节点到T节点的最大字节长度，以字节为单位 @endif */
    uint16_t max_pdu_t2g; /*!< @if Eng Maximum length from the T node to the G node, in bytes.
                               @else   T节点到G节点的最大字节长度 @endif */
} sle_iso_connection_request_param_t;

/**
 * @if Eng
 * @brief Iso connection parameter definition.
 * @else
 * @brief ISO连接参数定义
 * @endif
 */
typedef struct {
    uint16_t group_sync_delay; /*!< @if Eng G->T Time interval between two consecutive SDUs, in us.
                                  @else   G->T两个连续的SDU之间的时间间隔，以us为单位 @endif */
    uint16_t member_sync_delay; /*!< @if Eng T->G Time interval between two consecutive SDUs, in us.
                                  @else   T->G两个连续的SDU之间的时间间隔，以us为单位 @endif */
    uint16_t transport_latency_g2t; /*!< @if Eng G->T latency (unit: slot).
                                         @else   G->T延迟周期，单位slot @endif */
    uint16_t transport_latency_t2g; /*!< @if Eng T->G latency (unit: slot).
                                         @else   T->G延迟周期，单位slot */
} sle_iso_connection_result_t;

/**
 * @if Eng
 * @brief ISO default data path codec param.
 * @else
 * @brief ISO链路数据通道参数定义
 * @endif
 */
typedef struct {
    uint8_t codec_id;                    /*!< @if Eng Encoding format identifier.
                                              @else   编码格式的赋值序号 @endif */
    uint16_t company_id;                 /*!< @if Eng Company codec identifier.
                                              @else   标识公司的赋值序号，若codec_id不是0xFF，则将被忽略 @endif */
    uint16_t vendor_id;                  /*!< @if Eng Vendor-defined codec identifier.
                                              @else   Vendor定义的编解码标识，若codec_id不是0xFF，则将被忽略 @endif */
    uint32_t delay_time;                     /*!< @if Eng  delay time(3bytes).
                                              @else   Controller延迟时间(3bytes) @endif */
    uint8_t codec_configuration_length;  /*!< @if Eng Specifies the data length of the codec configuration.
                                              @else   指定编解码配置的数据长度 @endif */
    uint8_t *codec_configuration;        /*!< @if Eng Specifies the data content of the codec configuration.
                                              @else   指定编解码配置的数据内容，长度为codec_configuration_length @endif */
} sle_iso_codec_param_t;

/**
 * @if Eng
 * @brief ISO default data path parameter definition.
 * @else
 * @brief ISO链路数据通道参数定义
 * @endif
 */
typedef struct {
    uint8_t data_path_direction;         /*!< @if Eng data path direction{ @ref sle_iso_data_path_direction_t}
                                              @else   数据方向 { @ref sle_iso_data_path_direction_t}。 @endif */
    uint8_t data_path_id;                /*!< @if Eng ID of the channel used for data transmission.
                                              @else   数据传输时使用的通道标识, 0x00:HCI, 0x1~0xFF:vendor专用的数据通道
                                              0x00：HCI，0x01-0xFE：Vendor专用的逻辑信道序号 @endif */
    sle_iso_codec_param_t codec_param;  /*!< @if Eng codec param，If codec param not required, set data_path to 0.
                                                    @else  编解码相关参数，如果不需要codec参数，将data_path设置为0 @endif */
} sle_iso_data_path_param_t;

/**
 * @if Eng
 * @brief Report a iso group creation event.
 * @par Description: Iso group creation event,
                     A new iso group can be created only after the callback for creating
                     the previous iso group is returned.
 * @param [in] group_id ID of the preset iso group
 * @param [in] conn_type Preset iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] member_num Number of members in a iso group.
 * @param [in] mem_info iso group member information {@ref sle_iso_member_info_t}
 * @param [in] status Preset iso creation result.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO组创建完成事件上报。
 * @par Description: ISO组创建完成事件上报,
                     创建ISO组必须在上一个ISO组创建回调返回之后再创建新的ISO组.
 * @param [in] group_id 预置ISO组id
 * @param [in] conn_type 预置ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] member_num ISO组中成员个数.
 * @param [in] mem_info ISO组成员信息 { @ref sle_iso_member_info_t }
 * @param [in] status 预置ISO创建结果。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_create_group_callback)(
    uint16_t group_id, uint8_t conn_type, uint16_t member_num, const sle_iso_member_info_t *mem_info, errcode_t status);

/**
 * @if Eng
 * @brief Report the completion of deleting a iso group.
 * @par Description: Report a iso group creation completion event.
 * @param [in] group_id ISO group ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] status Processing result.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO组删除操作的完成事件上报。
 * @par Description: ISO组创建完成事件上报。
 * @param [in] group_id 预置ISO组id,
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] status 处理结果。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_remove_group_callback)(uint16_t group_id, uint8_t conn_type, uint8_t status);

/**
 * @if Eng
 * @brief Create a iso connection status callback interface.
 * @par Description: Create a iso connection status callback interface.
 * @param [in] group_id Iso group ID
 * @param [in] member_info Iso member information {@ref sle_iso_member_info_t}.
 * @param [in] connect_param Iso connection parameter {@ref sle_iso_connection_result_t}.
 * @param [in] conn_state_info Preset ISO connection information {@ref sle_iso_connect_state_info_t}.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  创建ISO连接状态回调接口
 * @par Description: 创建ISO连接状态回调接口。
 * @param [in] group_id ISO组ID
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] member_info ISO成员信息 { @ref sle_iso_member_info_t }。
 * @param [in] connect_param ISO连接参数 { @ref sle_iso_connection_result_t }。
 * @param [in] conn_state_info 预置ISO连接信息 { @ref sle_iso_connect_state_info_t }。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_connect_state_changed_callback)(uint16_t group_id, uint8_t conn_type,
    sle_iso_member_info_t *member_info, sle_iso_connection_result_t *connect_param,
    sle_iso_connect_state_info_t *conn_state_info);

/**
 * @if Eng
 * @brief Callback interface for creating a iso connection request
 * @par Description: Create a iso connection status callback interface.
 * @param [in] group_id ISO group ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] member_info Iso member information {@ref sle_iso_member_info_t}.
 * @param [in] request_param Connection request parameter {@ref sle_iso_connection_request_param_t}.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  创建ISO连接请求回调接口
 * @par Description: 创建ISO连接状态回调接口。
 * @param [in] group_id ISO组ID
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] member_info ISO成员信息 { @ref sle_iso_member_info_t }。
 * @param [in] request_param 连接请求参数 { @ref sle_iso_connection_request_param_t }。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_connect_request_callback)(uint16_t group_id, uint8_t conn_type,
    sle_iso_member_info_t *member_info, sle_iso_connection_request_param_t *request_param);

/**
 * @if Eng
* @brief Definition of the callback interface for sending iso data
 * @par Description: Definition of the Iso Data Sending Callback Interface
 * @param [in] group_id ISO group ID
 * @param [in] member_id ISO member ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] status Refer to {@ref errcode_t}.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO数据发送回调接口定义
 * @par Description: Definition of the Iso Data Sending Callback Interface
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO组成员ID。
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] status 参考 { @ref errcode_t }。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_data_tx_cmp_callback)(uint16_t group_id, uint16_t member_id, uint8_t conn_type,
    errcode_t status);

/**
 * @if Eng
 * @brief Definition of the callback interface for receiving iso data
 * @par Description: Definition of the callback interface for receiving iso data
 * @param [in] group_id ISO group ID
 * @param [in] member_id Iso member ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] data_len Length of the received data
 * @param [in] data Address for receiving data
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO数据接收回调接口定义
 * @par Description: ISO数据接收回调接口定义
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO成员ID
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] data_len 接收数据长度
 * @param [in] data 接收数据地址
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_data_rx_callback)(uint16_t group_id, uint16_t member_id, uint8_t conn_type,
    uint16_t data_len, uint8_t *data);

/**
 * @if Eng
 * @brief Definition of the callback interface for setting ISO data path.
 * @par Description: Definition of the callback interface for setting ISO data path.
 * @param [in] group_id ISO group ID
 * @param [in] member_id ISO member ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] status Processing result.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  设置组播imb数据通道回调接口定义
 * @par Description: 设置组播imb数据通道回调接口定义
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO组成员ID
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] status    处理结果。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_set_data_path_callback)(uint16_t group_id, uint16_t member_id, uint8_t conn_type,
    uint8_t status);

/**
 * @if Eng
 * @brief Definition of the callback interface for removing ISO data path.
 * @par Description: Definition of the callback interface for removing ISO data path.
 * @param [in] group_id ISO group ID
 * @param [in] member_id ISO member ID
 * @param [in] conn_type ISO connection type {@ref sle_iso_connection_type_t}.
 * @param [in] status Processing result.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  删除组播imb数据通道回调接口定义
 * @par Description: 删除组播imb数据通道回调接口定义
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO组成员ID
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] status    处理结果。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
typedef void (*sle_iso_remove_data_path_callback)(uint16_t group_id, uint16_t member_id, uint8_t conn_type,
    uint8_t status);

/**
 * @if Eng
 * @brief Struct of SLE iso connection manager callback function.
 * @else
 * @brief SLEISO连接管理回调函数接口定义。
 * @endif
 */
typedef struct {
    sle_iso_create_group_callback create_group_cb; /*!< @if Eng Report a iso group creation event callback.
                                                              @else   ISO组创建完成事件上报回调函数。 @endif */
    sle_iso_remove_group_callback remove_group_cb;  /*!< @if Eng Report a iso group delete event callback.
                                                               @else   ISO组删除完成事件上报回调函数。 @endif */
    sle_iso_connect_state_changed_callback connect_state_changed_cb; /*!< @if Eng Connect state changed callback.
                                                                                @else   连接状态变化回调函数。 @endif */
    sle_iso_connect_request_callback connect_request_cb; /*!< @if Eng iso connection request callback.
                                                                    @else   ISO连接建立请求回调函数。 @endif */
    sle_iso_data_tx_cmp_callback tx_cb;   /*!< @if Eng data sending callback.
                                                     @else   数据发送回调函数。 @endif */
    sle_iso_data_rx_callback rx_cb;   /*!< @if Eng data receiving callback.
                                                 @else   数据接收回调函数。 @endif */
    sle_iso_set_data_path_callback set_data_path_cb;               /*!< @if Eng set data path callback.
                                                                                  @else   设置数据通道回调函数。@endif */
    sle_iso_remove_data_path_callback remove_data_path_cb;         /*!< @if Eng remove data path callback.
                                                                                  @else   删除数据通道回调函数。@endif */
} sle_iso_connection_callbacks_t;

/**
 * @if Eng
 * @brief Definition of the SLE Iso Connection Callback Registration Interface.
 * @par Description: register callback for a iso group.
 * @param [in] cbk { @ref sle_iso_connection_callbacks_t }。
 * @retval Other        失败。参考 {@ref errcode_t}
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief SLE ISO连接回调注册接口定义。
 * @par描述：ISO组注册回调。
 * @param [in] cbk { @ref sle_iso_connection_callbacks_t }。
 * @retval其他失败。参考 {@ref errcode_t}
 * @endif
 */
errcode_t sle_iso_connection_register_callbacks(sle_iso_connection_callbacks_t *cbk);

/**
 * @if Eng
* @brief Create a iso group.
 * @par Description: Create a iso group. A iso group needs to be created
 *                   before a iso connection is set up, A new iso group can be created
                     only after the callback for creating the previous iso group is returned.
 * @param [in] conn_type iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] member_num Number of iso connections
 * @param [in] param iso group connection parameter.
 * @param [out] group_id ID of the iso group created this time.
 * @retval Error code, The iso group creation status results at. { @ref sle_iso_create_group_callback }.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  创建ISO组。
 * @par Description: 创建ISO组，建立ISO连接之前需要创建一个ISO组,
                     创建ISO组必须在上一个ISO组创建回调返回之后再创建新的ISO组.
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] member_num ISO连接个数
 * @param [in] param ISO组连接参数
 * @param [out] group_id ISO组ID，本次创建的ISO组标识。
 * @retval 执行结果错误码， ISO组创建状态结果将在 { @ref sle_iso_create_group_callback }中返回。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_create_group(
    uint8_t conn_type, uint16_t member_num, sle_iso_connection_param_t *param, uint16_t *group_id);

/**
 * @if Eng
 * @brief  Creating a Iso Data Communication Link.
 * @par Description: Create the data communication link between G and T in the iso group.
                     Before creating the iso link, the iso group must be created,
                     and the basic connection must exist between G and T.
 * @param [in] conn_type iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id Iso Group ID
 * @param [in] member_num Number of create iso connections
 * @param [in] mult_conn_id Iso Connection Link ID { @ref sle_iso_connection_info_t }。
 * @retval error code, connection state change result will be returned
           at { @ref sle_iso_connect_state_changed_callback }.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  创建ISO数据通信链路。
 * @par Description: 创建ISO组中G-T之间的数据通信连接链路，创建ISO通讯链路之前必须先建立ISO组,
                     同时G-T之间必须已经存在基础连接.
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] group_id ISO组ID
 * @param [in] member_num ISO组成员ID由 { @ref sle_iso_create_group_callback } 中返回
 * @param [in] conn_info ISO连接数据链路信息。
 * @retval 执行结果错误码， 连接状态改变结果将在 { @ref sle_iso_connect_state_changed_callback }中返回。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_create_connections(uint8_t conn_type, uint16_t group_id, uint16_t member_num,
    sle_iso_connection_info_t *conn_info);

/**
 * @if Eng
 * @brief use this function to set iso connection default param.
 * @par Description: This method is used to set the default parameters for creating a iso group.
                     The sle_iso_connection_param_t parameter does not need to be transferred when
                     the sle_iso_create_group interface is used.
 * @param [in] conn_type iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] common_param: connection common parameter {@ref sle_iso_default_common_param_t}.
 * @param [in] g2t: connection parameter {@ref sle_iso_default_connection_param_t} of
                    the group leader in the iso group.
 * @param [in] t2g: connection parameter {@ref sle_iso_default_connection_param_t} of
                    the member POP in the iso group.
 * @retval Other Failure. For details, see {@ref errcode_t}
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  使用该函数设置多播连接默认参数。
 * @par Description: 使用此方法可以设置ISO建立默认参数，
                     使用sle_iso_create_group接口时可以不传递sle_iso_connection_param_t.
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] common_param: ISO通用参数{@ref sle_iso_default_common_param_t}。
 * @param [in] g2t：多播组中组长连接参数{@ref sle_iso_default_connection_param_t}。
 * @param [in] t2g：ISO组成员POP点连接参数{@ref sle_iso_default_connection_param_t}。
 * @retval Other 失败。参考 {@ref errcode_t}t
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_set_default_connection_param(uint8_t conn_type, sle_iso_default_common_param_t *common_param,
    sle_iso_default_connection_param_t *g2t, sle_iso_default_connection_param_t *t2g);

/**
 * @if Eng
 * @brief Use this function to delete a specified iso group.
 * @par Description: This function is used to delete a specified iso group.
 * @param [in] conn_type iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id Iso group ID
 * @retval Other failed. ref {@ref errcode_t}
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  使用该函数删除指定ISO组。
 * @par Description: 使用该函数删除指定ISO组。
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] group_id ISO组ID
 * @retval Other 失败。参考 {@ref errcode_t}
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_remove_group(uint8_t conn_type, uint16_t group_id);

/**
 * @if Eng
* @brief Iso group members use this interface to reject or receive connection requests from the group leader.
 * @par Description: Iso group members use this interface to reject or receive connection requests
                     from the group leader.
 * @param [in] conn_type iso connection type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id Iso group ID
 * @param [in] member_id Iso group member ID
 * @param [in] result Receive or reject the iso connection setup request {@ref sle_iso_connect_result_t}.
 * @retval Other failed. Refer to {@ref errcode_t}
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO组成员使用此接口拒绝或接收组长节点的连接请求。
 * @par Description: ISO组成员使用此接口拒绝或接收组长节点的连接请求。
 * @param [in] conn_type ISO连接类型 { @ref sle_iso_connection_type_t }。
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO组成员ID
 * @param [in] result 接收或者拒绝ISO连接建立请求 {@ref sle_iso_connect_result_t}。
 * @retval Other 失败。参考 {@ref errcode_t}
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_send_connection_response(uint8_t conn_type, uint16_t group_id, uint16_t member_id,
    sle_iso_connect_result_t result);

/**
 * @if Eng
* @brief ISO group members use this interface to reject or receive connection requests from the group leader.
 * @par Description: ISO group members use this interface to reject or receive connection requests
                     from the group leader.
 * @param [in] conn_type Channel type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id ISO group ID
 * @param [in] member_id ISO member ID.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO组成员使用此接口拒绝或接收组长节点的连接请求。
 * @par Description: ISO组成员使用此接口拒绝或接收组长节点的连接请求。
 * @param [in] conn_type 通道类型 { @ref sle_iso_connection_type_t}。
 * @param [in] group_id  ISO组ID。
 * @param [in] member_id ISO组成员ID。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_release_connection(uint8_t conn_type, uint16_t group_id, uint16_t member_id);

/**
 * @if Eng
 * @brief use this function to set ISO data path param.
 * @par Description: This method is used to set ISO data path param.
 * @param [in] conn_type Channel type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id ISO group ID
 * @param [in] member_id ISO member ID.
 * @param [in] param: data path parameter {@ref sle_iso_data_path_param_t}.
 * @retval Error code, The ISO data path creation status results at.
            { @ref sle_iso_set_data_path_callback }.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  使用该函数设置ISO icb链路数据通道参数。
 * @param [in] conn_type 通道类型 { @ref sle_iso_connection_type_t}。
 * @param [in] group_id  ISO组ID。
 * @param [in] member_id ISO组成员ID。
 * @param [in] param: 默认组播icb链路数据通道参数{@ref sle_iso_data_path_param_t}。
 * @retval 执行结果错误码，设置数据通道结果将在 { @ref sle_iso_set_data_path_callback }中返回。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_set_data_path(uint8_t conn_type, uint16_t group_id, uint16_t member_id,
    sle_iso_data_path_param_t *param);

/**
 * @if Eng
 * @brief use this function to remove ISO data path param.
 * @par Description: This method is used to remove ISO data path param.
 * @param [in] conn_type Channel type {@ref sle_iso_connection_type_t}.
 * @param [in] group_id ISO group ID.
 * @param [in] member_id ISO member ID.
 * @param [in] data_path_direction Data direction {@ref sle_iso_data_path_direction_t}.
 * @retval Error code, The ISO data path remove status results at.
        { @ref sle_iso_remove_data_path_callback }.
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  使用该函数删除多播icb链路数据通道参数。
 * @param [in] conn_type 通道类型 { @ref sle_iso_connection_type_t}。
 * @param [in] group_id  ISO组ID。
 * @param [in] member_id ISO组成员ID。
 * @param [in] data_path_direction 数据方向 { @ref sle_iso_data_path_direction_t}。
 * @retval 执行结果错误码，删除数据通道结果将在 { @ref sle_iso_remove_data_path_callback }中返回。
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_remove_data_path(uint8_t conn_type, uint16_t group_id, uint16_t member_id,
    uint8_t data_path_direction);

/**
 * @if Eng
 * @brief Interface for sending iso group data.
 * @par Description: Interface for sending iso group data.
 * @param [in] group_id Iso group ID
 * @param [in] member_id ISO member ID.
 * @param [in] conn_type Channel type {@ref sle_iso_connection_type_t}.
 * @param [in] data_len Length of the data to be sent
 * @param [in] data Address of the data to be sent
 * @retval Other failed. Refer to @ref errcode_t
 * @par Depends:
 * @li sle_common.h
 * @else
 * @brief  ISO组数据发送接口。
 * @par Description: ISO组数据发送接口。。
 * @param [in] group_id ISO组ID
 * @param [in] member_id ISO组成员ID。组播G节点发送数据时，传入任意值，其余情况，传入对应的成员节点的member_id
 * @param [in] conn_type 通道类型 { @ref sle_iso_connection_type_t}。
 * @param [in] data_len 需要发送的数据长
 * @param [in] data 需要发送的数据地址
 * @retval Other 失败。参考 @ref errcode_t
 * @par 依赖：
 * @li sle_common.h
 * @endif
 */
errcode_t sle_iso_send_data(uint8_t conn_type, uint16_t group_id, uint16_t member_id,
    uint16_t data_len, uint8_t *data);

#ifdef __cplusplus
}
#endif
#endif /* SLE_ISO_CONNECTION_MANAGER_H */
/**
 * @}
 */
