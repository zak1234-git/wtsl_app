# 微泰星闪网关 QoS API 完整指南

> **版本**：v2.0  
> **基础路径**：`/api/v1/nodes/{id}/qos`  
> **鉴权方式**：`Authorization: Bearer <token>`  
> **数据格式**：JSON  

---

## 架构说明

本系统采用三级 QoS 分级管控架构：

| 层级 | 角色 | 职责 | 核心技术 |
|------|------|------|---------|
| 第一级 | T 节点（子卡） | 流量识别与标记 | iptables DSCP/MARK 打标 |
| 第二级 | G 节点（微基站） | 汇聚整形与调度 | tc qdisc/class/filter HTB 层级调度 |
| 第三级 | 网管系统 | 全局统筹与下发 | 策略编译、批量下发、统一监控 |

### T 节点（子卡）— 流量识别与标记

T 节点是数据流入口，通过 iptables 对数据包打标：

```bash
# DSCP 打标（设置优先级类别）
iptables -A FORWARD -p tcp --dport 80 -j DSCP --set-dscp-class EF

# MARK 打标（设置标记值）
iptables -A FORWARD -p udp --dport 443 -j MARK --set-mark 0x10
```

### G 节点（微基站）— 汇聚整形与调度

G 节点读取 T 节点打上的 Mark/DSCP 值，将流量引导至不同队列：

```bash
# 匹配 MARK 值
tc filter add dev br0 parent 1:0 protocol ip handle 0x10 flowid 1:10

# 匹配 DSCP 值（dsfield）
tc filter add dev br0 parent 1:0 protocol ip dsfield 0x2e flowid 1:20
```

### 网管系统 — 全局统筹

通过本 API 实现策略编译、命令下发和统一监控（`tc -s` 统计 + `iptables -L -v -n -x` 计数）。

---

## 目录

- [一、状态管理（2 个）](#一状态管理)
- [二、规则管理（5 个）](#二规则管理)
- [三、访问控制/防火墙（7 个）](#三访问控制防火墙)
- [四、Raw 操作（4 个）](#四raw-操作)
- [五、场景 API（2 个）](#五场景-api)
- [六、快速上手](#六快速上手)
- [七、错误码](#七错误码)
- [八、参数速查表](#八参数速查表)
- [九、API 总览](#九api-总览)

---

## 一、状态管理

### 1.1 获取 QoS 状态

```
GET /api/v1/nodes/{id}/qos/status
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "enabled": true,
  "snapshot_count": 5,
  "device": "br0"
}
```

**字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| enabled | bool | QoS 总开关状态，true=开启 |
| snapshot_count | int | 当前快照中的规则数量 |
| device | string | 默认管理的网络接口名称 |

---

### 1.2 QoS 总开关

```
POST /api/v1/nodes/{id}/qos/switch
Authorization: Bearer <token>
Content-Type: application/json
```

**请求**：

```json
{
  "enabled": false
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| enabled | bool | 是 | true=开启（恢复快照规则），false=关闭（清除所有规则） |

**响应**（200 OK，状态切换成功）：

```json
{
  "message": "QoS enabled",
  "enabled": true
}
```

**响应**（200 OK，已在目标状态，无需切换）：

```json
{
  "message": "QoS already in target state",
  "enabled": true
}
```

---

## 二、规则管理

### 2.1 获取规则列表

```
GET /api/v1/nodes/{id}/qos/rules
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "total": 3,
  "rules": [
    {
      "rule_id": "rule_001",
      "name": "HTB Root Queue",
      "type": "tc",
      "action": "add",
      "active": true,
      "created_at": 1746928800,
      "raw_command": "tc qdisc add dev br0 root handle 1: htb default 30",
      "params": {
        "device": "br0",
        "parent": "root",
        "handle": "1:",
        "kind": "htb",
        "args": { "default": "30" }
      }
    },
    {
      "rule_id": "rule_002",
      "name": "Rate Limit Class",
      "type": "tc",
      "action": "add",
      "active": true,
      "created_at": 1746928810,
      "raw_command": "tc class add dev br0 parent 1:1 classid 1:10 htb rate 100mbit ceil 200mbit burst 15k",
      "params": {
        "device": "br0",
        "parent": "1:1",
        "handle": "10:",
        "kind": "htb",
        "args": { "rate": "100mbit", "ceil": "200mbit", "burst": "15k" }
      }
    },
    {
      "rule_id": "rule_003",
      "name": "Allow HTTP",
      "type": "iptables",
      "action": "add",
      "active": true,
      "created_at": 1746928820,
      "raw_command": "iptables -A FORWARD -p tcp -i br0 --dport 80 -j ACCEPT",
      "params": {
        "chain": "FORWARD",
        "protocol": "tcp",
        "device": "br0",
        "args": { "dport": "80", "jump": "ACCEPT" }
      }
    }
  ]
}
```

---

### 2.2 创建规则

```
POST /api/v1/nodes/{id}/qos/rules
Authorization: Bearer <token>
Content-Type: application/json
```

#### T 节点：iptables 打标规则

```json
{
  "type": "iptables",
  "name": "DSCP Mark Web Traffic",
  "action": "add",
  "chain": "FORWARD",
  "params": {
    "protocol": "tcp",
    "device": "eth0",
    "args": {
      "dport": "80",
      "set_dscp": "AF41"
    }
  }
}
```

```json
{
  "type": "iptables",
  "name": "MARK Gaming Traffic",
  "action": "add",
  "chain": "FORWARD",
  "params": {
    "protocol": "udp",
    "device": "eth0",
    "args": {
      "dport": "27015",
      "set_mark": "0x10"
    }
  }
}
```

#### G 节点：tc filter 匹配 Mark/DSCP 规则

```json
{
  "type": "tc",
  "name": "Match MARK 0x10",
  "action": "add",
  "obj_type": "filter",
  "params": {
    "device": "br0",
    "parent": "1:0",
    "match_handle": "0x10",
    "flowid": "1:10"
  }
}
```

```json
{
  "type": "tc",
  "name": "Match DSCP EF",
  "action": "add",
  "obj_type": "filter",
  "params": {
    "device": "br0",
    "parent": "1:0",
    "dsfield": "0x2e",
    "flowid": "1:20"
  }
}
```

**字段说明 — TC 规则**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | 是 | 固定 `"tc"` |
| name | string | 是 | 规则名称，最长 64 字符 |
| action | string | 是 | `add` / `delete` / `replace` / `change` |
| obj_type | string | 是 | `qdisc` / `class` / `filter` |
| params.device | string | 否 | 网络接口名，默认使用系统配置值 |
| params.parent | string | 否 | 父节点，如 `root`、`1:1` |
| params.handle | string | 否 | 句柄，如 `1:`、`10:` |
| params.kind | string | 否 | 类型，如 `htb`、`sfq`、`u32` |
| params.args | object | 否 | 动态参数，键值对形式 |
| params.match_handle | string | filter 匹配 Mark 时必填 | Mark 值，如 `0x10` |
| params.dsfield | string | filter 匹配 DSCP 时必填 | DSCP 十六进制值，如 `0x2e`（对应 EF） |
| params.flowid | string | filter 类型必填 | 流 ID，如 `1:10` |

**字段说明 — iptables 规则**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | 是 | 固定 `"iptables"` |
| name | string | 是 | 规则名称 |
| action | string | 是 | `add` / `delete` / `insert` / `replace` |
| chain | string | 是 | `INPUT` / `OUTPUT` / `FORWARD` |
| params.protocol | string | 否 | `tcp` / `udp` / `icmp` |
| params.device | string | 否 | 网络接口名 |
| params.args.src | string | 否 | 源 IP 或网段 |
| params.args.dst | string | 否 | 目标 IP 或网段 |
| params.args.sport | string | 否 | 源端口 |
| params.args.dport | string | 否 | 目标端口 |
| params.args.jump | string | 否 | 动作：`ACCEPT` / `DROP` / `REJECT` |
| params.args.set_dscp | string | 否 | DSCP 打标值，如 `EF`、`AF41`、`CS3` |
| params.args.set_mark | string | 否 | MARK 打标值，如 `0x10` |
| params.args.match_dscp | string | 否 | DSCP 匹配值，如 `EF` |
| params.args.match_mark | string | 否 | MARK 匹配值，如 `0x10` |

**响应**（200 OK，成功）：

```json
{
  "status": "success",
  "rule_id": "rule_004"
}
```

---

### 2.3 获取单条规则

```
GET /api/v1/nodes/{id}/qos/rules/{rule_id}
Authorization: Bearer <token>
```

**请求**：无 body（rule_id 从 URL 路径提取）

**响应**（200 OK，找到）：

```json
{
  "status": "success",
  "data": {
    "rule_id": "rule_001",
    "name": "HTB Root Queue",
    "type": "tc",
    "action": "add",
    "obj_type": "qdisc",
    "active": true,
    "created_at": 1746928800,
    "raw_command": "tc qdisc add dev br0 root handle 1: htb default 30",
    "params": {
      "device": "br0",
      "parent": "root",
      "handle": "1:",
      "kind": "htb",
      "args": { "default": "30" }
    }
  }
}
```

**响应**（404 Not Found）：

```json
{
  "status": "error",
  "message": "Rule not found"
}
```

---

### 2.4 删除规则

```
POST /api/v1/nodes/{id}/qos/rules/{rule_id}
Authorization: Bearer <token>
```

**请求**：无 body（rule_id 从 URL 路径提取）

**响应**（200 OK，成功）：

```json
{
  "status": "success",
  "rule_id": "rule_001"
}
```

**响应**（404 Not Found）：

```json
{
  "status": "error",
  "message": "Rule not found"
}
```

---

### 2.5 清除所有规则

```
POST /api/v1/nodes/{id}/qos/clear
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "message": "All rules cleared"
}
```

---

## 三、访问控制（防火墙）

> 访问控制功能专为 T 节点（高带宽子卡）设计，相当于在 T 节点上增加防火墙功能。可以控制连接到 T 节点的设备（如 AGV 车）与外部通信的数据流。

### 设计说明

- **关闭状态**：所有数据正常转发，不做任何拦截（AGV 车与上位机直接通信无阻拦）
- **开启状态**：根据策略规则匹配数据包的源 IP、目的 IP、源端口、目的端口、源 MAC、目的 MAC、协议类型，执行放行或阻断
- 开启后仅影响匹配到的流量，未匹配到的流量默认**阻断**（安全优先）

### 3.1 获取访问控制开关状态

```
GET /api/v1/nodes/{id}/qos/acl
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "enabled": false,
  "rules_count": 0
}
```

**字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| enabled | bool | 访问控制开关状态，false=关闭（所有数据正常转发），true=开启（按策略规则拦截/放行） |
| rules_count | int | 当前配置的访问控制规则数量 |

---

### 3.2 设置访问控制开关

```
POST /api/v1/nodes/{id}/qos/acl/enable
Authorization: Bearer <token>
Content-Type: application/json
```

**请求**：

```json
{
  "enabled": true
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| enabled | bool | 是 | true=开启访问控制（启用防火墙），false=关闭访问控制（所有数据正常转发） |

**响应**（200 OK）：

```json
{
  "status": "success",
  "enabled": true
}
```

---

### 3.3 获取访问控制规则列表

```
GET /api/v1/nodes/{id}/qos/acl/rules
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "data": {
    "total": 2,
    "rules": [
      {
        "rule_id": "acl_001",
        "name": "Allow AGV to Server",
        "action": "add",
        "active": true,
        "created_at": 1746928800,
        "raw_command": "iptables -A FORWARD -s 192.168.1.100 -d 192.168.1.10 -p tcp --sport 5000 --dport 8080 -j ACCEPT",
        "params": {
          "src_ip": "192.168.1.100",
          "dst_ip": "192.168.1.10",
          "protocol": "tcp",
          "src_port": "5000",
          "dst_port": "8080",
          "policy": "ACCEPT"
        }
      },
      {
        "rule_id": "acl_002",
        "name": "Block Unknown Traffic",
        "action": "add",
        "active": true,
        "created_at": 1746928810,
        "raw_command": "iptables -A FORWARD -s 192.168.1.100 -j DROP",
        "params": {
          "src_ip": "192.168.1.100",
          "policy": "DROP"
        }
      }
    ]
  }
}
```

---

### 3.4 添加访问控制规则

```
POST /api/v1/nodes/{id}/qos/acl/rules
Authorization: Bearer <token>
Content-Type: application/json
```

**请求 — 放行 AGV 车到上位机的通信**：

```json
{
  "name": "Allow AGV to Server",
  "params": {
    "src_ip": "192.168.1.100",
    "dst_ip": "192.168.1.10",
    "protocol": "tcp",
    "src_port": "5000",
    "dst_port": "8080",
    "policy": "ACCEPT"
  }
}
```

**请求 — 按 MAC 地址放行**：

```json
{
  "name": "Allow AGV by MAC",
  "params": {
    "src_mac": "AA:BB:CC:DD:EE:01",
    "dst_ip": "192.168.1.10",
    "protocol": "tcp",
    "dst_port": "8080",
    "policy": "ACCEPT"
  }
}
```

**请求 — 阻断所有其他流量**：

```json
{
  "name": "Block All Other",
  "params": {
    "src_ip": "192.168.1.100",
    "policy": "DROP"
  }
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| name | string | 是 | 规则名称 |
| params.src_ip | string | 否 | 源 IP 地址或网段，如 `192.168.1.100` |
| params.dst_ip | string | 否 | 目的 IP 地址或网段，如 `192.168.1.10` |
| params.protocol | string | 否 | 协议类型：`tcp` / `udp` / `icmp` |
| params.src_port | string | 否 | 源端口号，如 `5000` |
| params.dst_port | string | 否 | 目的端口号，如 `8080` |
| params.src_mac | string | 否 | 源 MAC 地址，如 `AA:BB:CC:DD:EE:01` |
| params.dst_mac | string | 否 | 目的 MAC 地址 |
| params.policy | string | 是 | 安全策略：`ACCEPT`=放行，`DROP`=阻断 |

**响应**（200 OK）：

```json
{
  "status": "success",
  "rule_id": "acl_001"
}
```

---

### 3.5 删除访问控制规则

```
POST /api/v1/nodes/{id}/qos/acl/rules/{rule_id}
Authorization: Bearer <token>
```

**请求**：无 body（rule_id 从 URL 路径提取）

**响应**（200 OK）：

```json
{
  "status": "success",
  "rule_id": "acl_001"
}
```

---

### 3.6 清除所有访问控制规则

```
POST /api/v1/nodes/{id}/qos/acl/clear
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "message": "All ACL rules cleared"
}
```

---

## 四、Raw 操作

### 3.1 查看 TC 当前状态

```
GET /api/v1/nodes/{id}/qos/tc
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "data": {
    "qdiscs": [
      "qdisc htb 1: root refcnt 2 r2q 10 default 30",
      "qdisc sfq 30: parent 1:30 limit 127p quantum 1514b"
    ],
    "classes": [
      "class htb 1:1 root prio 0 rate 1Gbit ceil 1Gbit burst 1599Kb",
      "class htb 1:10 parent 1:1 prio 0 rate 100Mbit ceil 200Mbit burst 15Kb"
    ],
    "filters": [
      "filter parent 1:0 protocol ip pref 49152 u32 match ip dport 80 0xffff flowid 1:10"
    ]
  }
}
```

---

### 3.2 查看 TC + iptables 统计信息（带 -s）

```
GET /api/v1/nodes/{id}/qos/tc/stats
Authorization: Bearer <token>
```

**请求**：无 body

**说明**：返回 `tc -s` 统计（Sent bytes/packets/dropped/overlimits）和 `iptables -L -v -n -x` 计数，用于网管系统实时监控吞吐量和丢包率。

**响应**（200 OK）：

```json
{
  "status": "success",
  "data": {
    "qdiscs": [
      "qdisc htb 1: root refcnt 2 r2q 10 default 30",
      " Sent 1234567890 byte 987654 pkt (dropped 0, overlimits 0 requeues 0)",
      " rate 100Mbit 8000pps backlog 0b 0p requeues 0"
    ],
    "classes": [
      "class htb 1:10 parent 1:1 prio 0 rate 100Mbit ceil 200Mbit burst 15Kb",
      " Sent 567890123 byte 456789 pkt (dropped 12, overlimits 5 requeues 1)",
      " rate 50Mbit 4000pps backlog 0b 0p requeues 1"
    ],
    "filters": [
      "filter parent 1:0 protocol ip pref 49152 u32",
      "filter parent 1:0 protocol ip pref 49152 u32 fh 800::800 order 2048 key ht 800 flowid 1:10"
    ],
    "iptables_stats": [
      "Chain FORWARD (policy ACCEPT 0 packets, 0 bytes)",
      " pkts bytes target     prot opt in     out     source               destination",
      "12345 6789012 ACCEPT     tcp  --  br0    *       0.0.0.0/0            0.0.0.0/0            tcp dpt:80",
      " 6789 3456789 DROP       udp  --  br0    *       0.0.0.0/0            0.0.0.0/0            udp dpt:445"
    ]
  }
}
```

**统计字段说明**：

| 字段 | 来源 | 说明 |
|------|------|------|
| data.qdiscs | `tc -s qdisc show` | 队列规程统计：Sent 字节数/包数、dropped 丢弃数、overlimits 超限数 |
| data.classes | `tc -s class show` | 流量类统计：Sent 字节数/包数、dropped 丢弃数、overlimits 超限数 |
| data.filters | `tc -s filter show` | 过滤器统计 |
| data.iptables_stats | `iptables -L FORWARD -v -n -x` | 每条规则的包数和字节数（-x 显示精确值） |

---

### 3.3 执行 TC 操作

```
POST /api/v1/nodes/{id}/qos/tc
Authorization: Bearer <token>
Content-Type: application/json
```

**请求 — 添加 class**：

```json
{
  "action": "add",
  "obj_type": "class",
  "params": {
    "device": "br0",
    "parent": "1:1",
    "handle": "20:",
    "kind": "htb",
    "args": {
      "rate": "50mbit",
      "ceil": "100mbit",
      "burst": "15k"
    }
  }
}
```

**请求 — 删除 qdisc**：

```json
{
  "action": "del",
  "obj_type": "qdisc",
  "params": {
    "device": "br0",
    "parent": "root",
    "handle": "1:"
  }
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| action | string | 是 | `add` / `del` / `replace` / `change` |
| obj_type | string | 是 | `qdisc` / `class` / `filter` |
| params | object | 是 | 与 2.2 节 TC 规则的 params 结构相同 |

**响应**（200 OK）：

```json
{
  "status": "success"
}
```

失败时：

```json
{
  "status": "error",
  "message": "tc command execution failed"
}
```

---

### 3.4 执行 iptables 操作

```
POST /api/v1/nodes/{id}/qos/iptables
Authorization: Bearer <token>
Content-Type: application/json
```

**请求 — DSCP 打标**：

```json
{
  "action": "add",
  "chain": "FORWARD",
  "params": {
    "protocol": "tcp",
    "device": "eth0",
    "args": {
      "dport": "80",
      "set_dscp": "EF"
    }
  }
}
```

**请求 — MARK 打标**：

```json
{
  "action": "add",
  "chain": "FORWARD",
  "params": {
    "protocol": "udp",
    "device": "eth0",
    "args": {
      "dport": "27015",
      "set_mark": "0x10"
    }
  }
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| action | string | 是 | `add` / `delete` / `insert` / `replace` |
| chain | string | 是 | `INPUT` / `OUTPUT` / `FORWARD` |
| params.args.set_dscp | string | 否 | DSCP 打标值：`EF`、`AF41`、`AF42`、`AF43`、`CS3` 等 |
| params.args.set_mark | string | 否 | MARK 打标值，十六进制如 `0x10` |
| params.args.match_dscp | string | 否 | DSCP 匹配过滤值 |
| params.args.match_mark | string | 否 | MARK 匹配过滤值 |

**响应**（200 OK）：

```json
{
  "status": "success"
}
```

失败时：

```json
{
  "status": "error",
  "message": "iptables command execution failed"
}
```

---

## 四、场景 API

### 4.1 获取所有可用场景

```
GET /api/v1/nodes/{id}/qos/scenes
Authorization: Bearer <token>
```

**请求**：无 body

**响应**（200 OK）：

```json
{
  "status": "success",
  "data": {
    "scenes": [
      {
        "scene": "bandwidth_limit",
        "title": "Bandwidth Limit",
        "description": "Limit bandwidth for a specific port",
        "request_example": {
          "class_id": 10,
          "port": 80,
          "protocol": "tcp",
          "rate": "100mbit",
          "ceil": "200mbit"
        }
      },
      {
        "scene": "traffic_block",
        "title": "Traffic Block",
        "description": "Block traffic on a specific port",
        "request_example": {
          "port": 445,
          "protocol": "tcp"
        }
      },
      {
        "scene": "traffic_allow",
        "title": "Traffic Allow",
        "description": "Allow traffic on a port with high priority",
        "request_example": {
          "port": 8080,
          "protocol": "tcp"
        }
      },
      {
        "scene": "device_limit",
        "title": "Device Rate Limit",
        "description": "Limit bandwidth for a device by IP",
        "request_example": {
          "ip": "192.168.1.100",
          "rate": "50mbit",
          "ceil": "100mbit"
        }
      },
      {
        "scene": "gaming_boost",
        "title": "Gaming Boost",
        "description": "Set highest priority for gaming ports",
        "request_example": {
          "port": 0
        },
        "default_ports": "8080, 27015-27030, 4380"
      },
      {
        "scene": "video_smooth",
        "title": "Video Smooth",
        "description": "Guaranteed bandwidth for video streaming ports",
        "request_example": {},
        "default_ports": "80, 443, 1935, 5000-5100"
      },
      {
        "scene": "iot_qos",
        "title": "IoT Device QoS",
        "description": "Guaranteed minimum bandwidth for IoT devices",
        "request_example": {
          "ip": "192.168.1.50",
          "rate": "10mbit"
        }
      },
      {
        "scene": "web_browse",
        "title": "Web Browse Guarantee",
        "description": "Guaranteed bandwidth for HTTP and HTTPS",
        "request_example": {}
      }
    ]
  }
}
```

---

### 4.2 应用场景（统一入口）

```
POST /api/v1/nodes/{id}/qos/scenes/apply
Authorization: Bearer <token>
Content-Type: application/json
```

所有场景都通过此接口调用，通过 `scene` 字段区分具体场景。

**统一响应格式（成功）**：

```json
{
  "status": "success",
  "scene": "bandwidth_limit",
  "message": "scene applied successfully"
}
```

**统一响应格式（失败）**：

```json
{
  "status": "error",
  "scene": "bandwidth_limit",
  "message": "bandwidth_limit needs class_id(int), port(int), rate(string), ceil(string)"
}
```

---

#### 场景 1：带宽限速

```json
{
  "scene": "bandwidth_limit",
  "class_id": 10,
  "port": 80,
  "protocol": "tcp",
  "rate": "100mbit",
  "ceil": "200mbit"
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| scene | string | 是 | 固定 `"bandwidth_limit"` |
| class_id | int | 是 | 类 ID，需唯一，建议从 10 起递增 |
| port | int | 是 | 目标端口号 |
| protocol | string | 否 | `"tcp"` / `"udp"` / `"both"`，默认 `"tcp"` |
| rate | string | 是 | 限制速率，如 `"100mbit"` |
| ceil | string | 是 | 峰值上限，如 `"200mbit"` |

---

#### 场景 2：流量阻断

```json
{
  "scene": "traffic_block",
  "port": 445,
  "protocol": "tcp"
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| scene | string | 是 | 固定 `"traffic_block"` |
| port | int | 是 | 目标端口号 |
| protocol | string | 否 | `"tcp"` / `"udp"` / `"both"`，默认 `"tcp"` |

---

#### 场景 3：流量放行

```json
{
  "scene": "traffic_allow",
  "port": 8080,
  "protocol": "tcp"
}
```

---

#### 场景 4：设备限速

```json
{
  "scene": "device_limit",
  "ip": "192.168.1.100",
  "rate": "50mbit",
  "ceil": "100mbit"
}
```

---

#### 场景 5：游戏加速

```json
{
  "scene": "gaming_boost",
  "port": 27015
}
```

port=0 时使用默认端口列表：8080、27015-27020（Steam）、4380（暴雪）。

---

#### 场景 6：视频流畅

```json
{
  "scene": "video_smooth"
}
```

自动处理：80（HTTP）、443（HTTPS）、1935（RTMP）。

---

#### 场景 7：IoT 设备保障

```json
{
  "scene": "iot_qos",
  "ip": "192.168.1.50",
  "rate": "10mbit"
}
```

---

#### 场景 8：网页浏览保障

```json
{
  "scene": "web_browse"
}
```

自动处理端口 80 和 443。

---

## 五、快速上手

### 5.1 T 节点：iptables DSCP 打标

```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/iptables \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "chain": "FORWARD",
    "params": {
      "protocol": "tcp",
      "device": "eth0",
      "args": {
        "dport": "80",
        "set_dscp": "AF41"
      }
    }
  }'
```

### 5.2 G 节点：tc filter 匹配 Mark 值

```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/tc \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "obj_type": "filter",
    "params": {
      "device": "br0",
      "parent": "1:0",
      "match_handle": "0x10",
      "flowid": "1:10"
    }
  }'
```

### 5.3 G 节点：tc filter 匹配 DSCP 值

```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/tc \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "obj_type": "filter",
    "params": {
      "device": "br0",
      "parent": "1:0",
      "dsfield": "0x2e",
      "flowid": "1:20"
    }
  }'
```

### 5.4 网管：查看实时统计

```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/tc/stats \
  -H "Authorization: Bearer <token>"
```

### 5.5 一键限速（推荐新手）

```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/scenes/apply \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "scene": "bandwidth_limit",
    "class_id": 10,
    "port": 80,
    "protocol": "tcp",
    "rate": "100mbit",
    "ceil": "200mbit"
  }'
```

### 5.6 查看当前状态

```bash
# 查看 QoS 开关状态
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/status \
  -H "Authorization: Bearer <token>"

# 查看所有规则
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Authorization: Bearer <token>"

# 查看 TC 实时状态
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/tc \
  -H "Authorization: Bearer <token>"
```

### 5.7 删除规则

```bash
# 删除单条规则
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules/rule_001 \
  -H "Authorization: Bearer <token>"

# 清除所有规则
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/clear \
  -H "Authorization: Bearer <token>"
```

---

## 六、错误码

| HTTP 状态码 | 说明 | 示例场景 |
|------------|------|---------|
| 200 | 请求成功 | 规则创建成功 |
| 400 | 请求参数错误 | 缺少必填字段、格式错误 |
| 401 | 未认证 | Token 无效或缺失 |
| 403 | 无权限 | 用户权限不足 |
| 404 | 资源不存在 | 规则 ID 不存在 |
| 500 | 服务器错误 | tc/iptables 命令执行失败 |

**错误响应格式**：

```json
{
  "status": "error",
  "message": "Error description"
}
```

---

## 七、参数速查表

### DSCP 值对照表

| DSCP 名称 | 十六进制 | 十进制 | 用途 |
|-----------|---------|--------|------|
| EF | 0x2e | 46 | 低延迟/游戏/语音 |
| AF41 | 0x22 | 34 | 视频流（高优先级） |
| AF42 | 0x24 | 36 | 视频流（中优先级） |
| AF43 | 0x26 | 38 | 视频流（低优先级） |
| CS3 | 0x18 | 24 | 信令/控制流量 |
| AF31 | 0x1a | 26 | 企业应用 |
| AF21 | 0x12 | 18 | 一般业务 |
| CS0/BE | 0x00 | 0 | 尽力而为（默认） |

### MARK 值建议

| MARK 值 | 用途 |
|---------|------|
| 0x10 | 游戏/低延迟业务 |
| 0x20 | 视频流业务 |
| 0x30 | 网页浏览 |
| 0x40 | IoT 设备 |
| 0x50 | 普通业务 |
| 0xFF | 最低优先级/阻断 |

### 速率单位

| 单位 | 说明 | 示例 |
|------|------|------|
| `kbit` | 千比特/秒 | `"500kbit"` |
| `mbit` | 兆比特/秒 | `"100mbit"` |
| `gbit` | 吉比特/秒 | `"1gbit"` |

### iptables args 字段映射

| JSON 字段 | iptables 命令行参数 |
|-----------|-------------------|
| `src` / `source` | `-s` |
| `dst` / `destination` | `-d` |
| `sport` | `--sport` |
| `dport` | `--dport` |
| `jump` / `target` | `-j` |
| `set_dscp` / `dscp_class` | `-j DSCP --set-dscp-class` |
| `set_mark` / `mark` | `-j MARK --set-mark` |
| `match_dscp` | `-m dscp --dscp` |
| `match_mark` | `-m mark --mark` |
| 其他字段 | `--<字段名>` |

### iptables 动作（jump/target）

| 值 | 说明 |
|----|------|
| `ACCEPT` | 放行 |
| `DROP` | 静默丢弃 |
| `REJECT` | 拒绝并返回错误 |
| `DSCP` | DSCP 打标 |
| `MARK` | MARK 打标 |

### TC 类型（kind）

| 值 | 说明 |
|----|------|
| `htb` | 分层令牌桶（限速常用） |
| `sfq` | 随机公平队列 |
| `fq_codel` | 公平队列 + 主动队列管理 |
| `u32` | 通用过滤器（filter 类型使用） |

### TC filter 匹配方式

| 匹配方式 | params 字段 | 生成的命令 | 用途 |
|----------|------------|-----------|------|
| u32 端口匹配 | `args: { "protocol": "ip", "match": "ip dport 80 0xffff" }` | `u32 match ip tcp dport 80 0xffff` | 按端口过滤 |
| Mark 匹配 | `match_handle: "0x10"` | `handle 0x10` | 读取 iptables MARK 值 |
| DSCP 匹配 | `dsfield: "0x2e"` | `dsfield 0x2e` | 读取 iptables DSCP 值 |

### TC 优先级（prio）

| 值 | 说明 |
|----|------|
| 1 | 最高优先级 |
| 2 | 高优先级 |
| 3 | 中优先级 |
| ... | 依次降低 |
| 99 | 默认队列（未匹配的流量） |

---

## 九、API 总览

| Method | URL | 说明 | 适合角色 |
|--------|-----|------|---------|
| GET | `/qos/status` | 获取 QoS 状态 | 所有节点 |
| POST | `/qos/switch` | QoS 总开关 | 所有节点 |
| GET | `/qos/rules` | QoS 规则列表 | 所有节点 |
| POST | `/qos/rules` | 创建 QoS 规则 | 所有节点 |
| GET | `/qos/rules/{id}` | 获取单条 QoS 规则 | 所有节点 |
| POST | `/qos/rules/{id}` | 删除 QoS 规则 | 所有节点 |
| POST | `/qos/clear` | 清除所有 QoS 规则 | 所有节点 |
| GET | `/qos/tc` | TC 当前状态 | G 节点 |
| GET | `/qos/tc/stats` | TC + iptables 统计 | 网管系统 |
| POST | `/qos/tc` | 执行 TC 操作 | G 节点 |
| POST | `/qos/iptables` | 执行 iptables 操作 | T 节点 |
| **GET** | **`/qos/acl`** | **获取访问控制开关状态** | **T 节点** |
| **POST** | **`/qos/acl/enable`** | **设置访问控制开关** | **T 节点** |
| **GET** | **`/qos/acl/rules`** | **访问控制规则列表** | **T 节点** |
| **POST** | **`/qos/acl/rules`** | **添加访问控制规则** | **T 节点** |
| **POST** | **`/qos/acl/rules/{id}`** | **删除访问控制规则** | **T 节点** |
| **POST** | **`/qos/acl/clear`** | **清除所有访问控制规则** | **T 节点** |
| GET | `/qos/scenes` | 获取场景列表 | 所有节点 |
| POST | `/qos/scenes/apply` | 应用场景 | 所有节点 |
