# 防火墙 (ACL) 与 QoS API 文档

## 概述

本文档描述新增的防火墙 (ACL) 和增强 QoS 功能的 RESTful API。

## 基础信息

- **基础路径**: `/api/v1`
- **节点 ID**: `{id}` (通常为 0，表示当前节点)
- **数据格式**: JSON

---

## 防火墙 (ACL) API

### 1. 获取防火墙状态

**URL**: `/api/v1/nodes/{id}/acl/stats`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/stats
```

**响应示例** (200 OK):
```json
{
  "enabled": true,
  "rule_count": 5,
  "device": "wt_br0",
  "type": "iptables"
}
```

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| enabled | bool | 防火墙开关状态 |
| rule_count | int | 当前规则数量 |
| device | string | 默认网络设备 |
| type | string | 防火墙类型 (iptables) |

---

### 2. 设置防火墙开关

**URL**: `/api/v1/nodes/{id}/acl/stats`

**方法**: `POST`

**请求体**:
```json
{
  "enabled": true
}
```

**请求示例**:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/stats \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

**响应示例** (200 OK):
```json
{
  "status": "success"
}
```

**说明**:
- `enabled: true` - 启用防火墙（设置默认 DROP 策略）
- `enabled: false` - 禁用防火墙（清空规则，设置 ACCEPT 策略）

---

### 3. 获取防火墙规则列表

**URL**: `/api/v1/nodes/{id}/acl/rules`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/rules
```

**响应示例** (200 OK):
```json
{
  "INPUT": [
    "-A INPUT -i lo -j ACCEPT",
    "-A INPUT -p tcp --dport 22 -j ACCEPT",
    "-A INPUT -p tcp --dport 80 -j ACCEPT"
  ],
  "OUTPUT": [],
  "FORWARD": []
}
```

---

### 4. 管理防火墙规则

**URL**: `/api/v1/nodes/{id}/acl/rules`

**方法**: `POST`

**请求体**:
```json
{
  "action": "add",
  "chain": "INPUT",
  "rule": {
    "protocol": "tcp",
    "dest_port": "22",
    "target": "ACCEPT"
  }
}
```

**请求示例** - 添加规则:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "chain": "INPUT",
    "rule": {
      "protocol": "tcp",
      "dest_port": "22",
      "target": "ACCEPT"
    }
  }'
```

**请求示例** - 删除规则（按行号）:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "delete",
    "chain": "INPUT",
    "rule_num": 1
  }'
```

**请求示例** - 删除规则（按匹配条件）:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "delete",
    "chain": "INPUT",
    "rule": {
      "protocol": "tcp",
      "dest_port": "22",
      "target": "ACCEPT"
    }
  }'
```

**请求示例** - 替换规则:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "replace",
    "chain": "INPUT",
    "rule_num": 1,
    "rule": {
      "protocol": "tcp",
      "dest_port": "2222",
      "target": "ACCEPT"
    }
  }'
```

**响应示例** (200 OK):
```json
{
  "success": true,
  "action": "add",
  "chain": "INPUT",
  "message": "Operation successful"
}
```

**请求参数说明**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| action | string | 是 | 操作类型：`add`, `delete`, `replace`, `list` |
| chain | string | 是 | 链名：`INPUT`, `OUTPUT`, `FORWARD` |
| rule_num | int | 否 | 规则行号（用于 delete/replace） |
| rule.protocol | string | 否 | 协议：`tcp`, `udp`, `icmp` |
| rule.source | string | 否 | 源地址：`192.168.1.100`, `10.0.0.0/8` |
| rule.destination | string | 否 | 目标地址 |
| rule.in_interface | string | 否 | 入站接口：`eth0`, `wt_br0` |
| rule.out_interface | string | 否 | 出站接口 |
| rule.source_port | string | 否 | 源端口：`1024:65535` |
| rule.dest_port | string | 否 | 目标端口：`22`, `80` |
| rule.target | string | 是 | 动作：`ACCEPT`, `DROP`, `REJECT` |

---

## QoS API

### 1. 获取 QoS 状态

**URL**: `/api/v1/nodes/{id}/qos/stats`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/stats
```

**响应示例** (200 OK):
```json
{
  "enabled": true,
  "snapshot_count": 10,
  "device": "wt_br0",
  "type": "tc"
}
```

---

### 2. 设置 QoS 开关

**URL**: `/api/v1/nodes/{id}/qos/stats`

**方法**: `POST`

**请求体**:
```json
{
  "enabled": true
}
```

**请求示例**:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/stats \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

**说明**:
- `enabled: true` - 启用 QoS（恢复保存的规则）
- `enabled: false` - 禁用 QoS（清除所有 tc 规则）

---

### 3. 获取 QoS 规则树

**URL**: `/api/v1/nodes/{id}/qos/rules`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/rules
```

**响应示例** (200 OK):
```json
{
  "device": "wt_br0",
  "qdiscs": [
    "qdisc htb 1: root refcnt 2 r2q 10 default 0x10 direct_packets_stat 0 direct_qlen 1000",
    " Sent 1234567 bytes 9876 pkt (dropped 0, overlimits 0 requeues 0)"
  ],
  "classes": [
    "class htb 1:10 root prio 0 rate 50Mbit ceil 50Mbit burst 1600b cburst 1600b",
    " Sent 1234567 bytes 9876 pkt (dropped 0, overlimits 0 requeues 0)"
  ],
  "filters": [
    "filter parent 1: protocol ip pref 1 u32 chain 0",
    "filter parent 1: protocol ip pref 1 u32 chain 0 fh 800: ht divisor 1",
    "filter parent 1: protocol ip pref 1 u32 chain 0 fh 800::800 order 2048 key ht 800 bkt 0 flowid 1:10 not_in_hw match 0a016301/ffffffff at 16"
  ],
  "success": true
}
```

---

### 4. 管理 QoS 规则

**URL**: `/api/v1/nodes/{id}/qos/rules`

**方法**: `POST`

**请求示例** - 添加 qdisc:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "type": "qdisc",
    "device": "wt_br0",
    "handle": "1:",
    "kind": "htb",
    "args": {
      "default": "10"
    }
  }'
```

**请求示例** - 添加 class:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "type": "class",
    "device": "wt_br0",
    "parent": "1:",
    "classid": "1:10",
    "kind": "htb",
    "args": {
      "rate": "50mbit",
      "ceil": "50mbit"
    }
  }'
```

**请求示例** - 添加 filter（限制特定 IP）:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "type": "filter",
    "device": "wt_br0",
    "parent": "1:",
    "protocol": "ip",
    "prio": 1,
    "kind": "u32",
    "args": {
      "match": "ip dst 192.168.99.2/32"
    },
    "flowid": "1:10"
  }'
```

**请求示例** - 删除 class:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "delete",
    "type": "class",
    "device": "wt_br0",
    "parent": "1:",
    "classid": "1:10"
  }'
```

**响应示例** (200 OK):
```json
{
  "success": true,
  "action": "add",
  "type": "class",
  "message": "Operation successful"
}
```

**请求参数说明**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| action | string | 是 | 操作类型：`add`, `delete`, `replace`, `change`, `show` |
| type | string | 是 | 对象类型：`qdisc`, `class`, `filter` |
| device | string | 否 | 网络设备，默认 `wt_br0` |
| parent | string | 否 | 父节点：`1:`, `1:1` |
| handle | string | 否 | 句柄：`1:`, `1:10` |
| classid | string | 否 | 类 ID: `1:10` |
| kind | string | 否 | 类型：`htb`, `sfq`, `tbf`, `u32` |
| protocol | string | 否 | 协议：`ip`, `tcp`, `udp` |
| prio | int | 否 | 优先级：1, 2, 3 |
| flowid | string | 否 | 流向 ID: `1:10` |
| args | object | 否 | 动态参数（rate, ceil, burst 等） |

---

## 错误响应

**400 Bad Request** - 请求参数错误:
```json
{
  "error": "Invalid JSON data"
}
```

**500 Internal Server Error** - 服务器错误:
```json
{
  "success": false,
  "error": "Command execution failed"
}
```

---

## 注意事项

### iptables 锁文件问题

如果遇到错误：
```
Fatal: can't open lock file /tmp/run/xtables.lock: No such file or directory
```

**解决方案**: 代码已自动处理，所有 iptables 命令都使用 `-w` 参数等待锁，并自动创建 `/tmp/run` 目录。

### 内核模块限制

当前环境内核已裁剪，缺少大多数 netfilter 扩展模块。

**支持的匹配条件**:
- 协议 (`-p tcp/udp/icmp`)
- 源/目标地址 (`-s`, `-d`)
- 接口 (`-i`, `-o`)
- 端口 (`--dport`, `--sport`)
- ICMP 类型 (`--icmp-type`)

**不支持的扩展**:
- `state` / `conntrack` 模块（部分系统可能支持）
- `multiport` 模块
- `LOG` 目标

### 权限要求

API 服务需要 root 权限才能执行 iptables 和 tc 命令。

---

## API 总览表

| 功能 | 方法 | URL |
|------|------|-----|
| 获取防火墙状态 | GET | `/api/v1/nodes/{id}/acl/stats` |
| 设置防火墙开关 | POST | `/api/v1/nodes/{id}/acl/stats` |
| 获取防火墙规则 | GET | `/api/v1/nodes/{id}/acl/rules` |
| 管理防火墙规则 | POST | `/api/v1/nodes/{id}/acl/rules` |
| 获取 QoS 状态 | GET | `/api/v1/nodes/{id}/qos/stats` |
| 设置 QoS 开关 | POST | `/api/v1/nodes/{id}/qos/stats` |
| 获取 QoS 规则 | GET | `/api/v1/nodes/{id}/qos/rules` |
| 管理 QoS 规则 | POST | `/api/v1/nodes/{id}/qos/rules` |
