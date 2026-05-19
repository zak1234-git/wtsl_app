# 防火墙 (ACL) 与 QoS API 完整指南

**版本**: 1.1  
**更新时间**: 2026-05-18  
**适用设备**: 星闪网关设备  
**固件版本**: v1.1.22 及以上

---

## 📋 目录

1. [概述](#概述)
2. [快速开始](#快速开始)
3. [API 参考](#api-参考)
4. [使用示例](#使用示例)
5. [技术细节](#技术细节)
6. [故障排查](#故障排查)
7. [附录](#附录)

---

## 概述

### 功能简介

本文档描述星闪网关设备的防火墙 (ACL) 和流量控制 (QoS) RESTful API。

**防火墙 (ACL)**: 基于 iptables 的网络访问控制
- 包过滤（允许/拒绝特定流量）
- 基于 IP、端口、协议的规则
- 支持 INPUT、OUTPUT、FORWARD 链

**流量控制 (QoS)**: 基于 tc (Traffic Control) 的带宽管理
- 带宽限制（上传/下载）
- 基于 HTB、SFQ、TBF 等队列规则
- 支持 qdisc、class、filter 三层管理

### API 端点总览

| 功能 | 方法 | URL | 说明 |
|------|------|-----|------|
| **防火墙 (ACL)** | | | |
| 获取状态 | GET | `/api/v1/nodes/{id}/acl/status` | 查看防火墙开关状态 |
| 设置开关 | POST | `/api/v1/nodes/{id}/acl/switch` | 启用/禁用防火墙 |
| 获取规则 | GET | `/api/v1/nodes/{id}/acl/rules` | 查看当前规则列表 |
| 管理规则 | POST | `/api/v1/nodes/{id}/acl/rules` | 增删改规则 |
| **QoS** | | | |
| 获取状态 | GET | `/api/v1/nodes/{id}/qos/status` | 查看 QoS 开关状态 |
| 设置开关 | POST | `/api/v1/nodes/{id}/qos/switch` | 启用/禁用 QoS |
| 获取规则 | GET | `/api/v1/nodes/{id}/qos/rules` | 查看规则树 |
| 管理规则 | POST | `/api/v1/nodes/{id}/qos/rules` | 增删改规则 |

### 基础信息

- **基础路径**: `/api/v1`
- **节点 ID**: `{id}` (通常为 0，表示当前节点)
- **默认地址**: `http://192.168.99.1:8080`
- **数据格式**: JSON
- **认证**: 当前版本无需认证（生产环境建议添加）

---

## 快速开始

### 1. 启动服务

```bash
cd /home/admin/Downloads/wtsl_app/wtsl_app
sudo ./bin/arm/wtsl_app
```

**注意**: 需要 root 权限以执行 iptables 和 tc 命令

### 2. 验证服务

```bash
# 检查服务是否运行
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/status
```

预期响应:
```json
{
  "enabled": false,
  "rule_count": 0,
  "device": "wt_br0",
  "type": "iptables"
}
```

### 3. 快速测试

```bash
# 启用防火墙
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/switch \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'

# 添加 SSH 允许规则
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

# 启用 QoS
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/switch \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

### 4. 运行测试脚本

```bash
cd /home/admin/Downloads/wtsl_app/wtsl_app/docs
./test_api.sh
```

---

## API 参考

### 防火墙 (ACL) API

#### 1. 获取防火墙状态

**URL**: `/api/v1/nodes/{id}/acl/status`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/status
```

**响应示例** (200 OK):
```json
{
  "enabled": false,
  "rule_count": 0,
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

#### 2. 设置防火墙开关

**URL**: `/api/v1/nodes/{id}/acl/switch`

**方法**: `POST`

**请求体**:
```json
{
  "enabled": true
}
```

**请求示例**:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/switch \
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

**⚠️ 警告**: 远程连接时，启用防火墙前必须先添加 SSH 允许规则，否则会断开连接！

---

#### 3. 获取防火墙规则列表

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

#### 4. 管理防火墙规则

**URL**: `/api/v1/nodes/{id}/acl/rules`

**方法**: `POST`

**请求参数**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| action | string | 是 | 操作类型：`add`, `delete`, `replace`, `list` |
| chain | string | 是 | 链名：`INPUT`, `OUTPUT`, `FORWARD` |
| rule_num | int | 否 | 规则行号（用于 delete/replace） |
| rule | object | 条件必填 | 规则详情（见下方） |

**rule 对象参数**:

| 参数 | 类型 | 说明 | 示例 |
|------|------|------|------|
| protocol | string | 协议 | `tcp`, `udp`, `icmp` |
| source | string | 源地址 | `192.168.1.100`, `10.0.0.0/8` |
| destination | string | 目标地址 | `172.16.0.1` |
| in_interface | string | 入站接口 | `eth0`, `wt_br0` |
| out_interface | string | 出站接口 | `eth1` |
| source_port | string | 源端口 | `1024:65535` |
| dest_port | string | 目标端口 | `22`, `80` |
| target | string | 动作 | `ACCEPT`, `DROP`, `REJECT` |

**请求示例 - 添加规则**:
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

**请求示例 - 删除规则（按行号）**:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "delete",
    "chain": "INPUT",
    "rule_num": 1
  }'
```

**请求示例 - 删除规则（按匹配条件）**:
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

**请求示例 - 替换规则**:
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

---

### QoS API

#### 1. 获取 QoS 状态

**URL**: `/api/v1/nodes/{id}/qos/status`

**方法**: `GET`

**请求示例**:
```bash
curl http://192.168.99.1:8080/api/v1/nodes/0/qos/status
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

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| enabled | bool | QoS 开关状态 |
| snapshot_count | int | 保存的规则数量 |
| device | string | 默认网络设备 |
| type | string | QoS 类型 (tc) |

---

#### 2. 设置 QoS 开关

**URL**: `/api/v1/nodes/{id}/qos/switch`

**方法**: `POST`

**请求体**:
```json
{
  "enabled": true
}
```

**请求示例**:
```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/switch \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

**说明**:
- `enabled: true` - 启用 QoS（恢复保存的规则）
- `enabled: false` - 禁用 QoS（清除所有 tc 规则）

---

#### 3. 获取 QoS 规则树

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
    "qdisc htb 1: root refcnt 2 r2q 10 default 0x10 direct_packets_stat 0 direct_qlen 1000"
  ],
  "classes": [
    "class htb 1:10 root prio 0 rate 50Mbit ceil 50Mbit burst 1600b cburst 1600b"
  ],
  "filters": [
    "filter parent 1: protocol ip pref 1 u32 chain 0"
  ],
  "success": true
}
```

---

#### 4. 管理 QoS 规则

**URL**: `/api/v1/nodes/{id}/qos/rules`

**方法**: `POST`

**请求参数**:

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

**请求示例 - 添加 qdisc**:
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

**请求示例 - 添加 class**:
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

**请求示例 - 添加 filter（限制特定 IP）**:
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

**响应示例** (200 OK):
```json
{
  "success": true,
  "action": "add",
  "type": "class",
  "message": "Operation successful"
}
```

---

## 使用示例

### 场景 1: 只允许 SSH 和 HTTP 访问

```bash
# 1. 先添加 SSH 允许规则（防止断开）
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{"action":"add","chain":"INPUT","rule":{"protocol":"tcp","dest_port":"22","target":"ACCEPT"}}'

# 2. 添加 HTTP 允许规则
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{"action":"add","chain":"INPUT","rule":{"protocol":"tcp","dest_port":"80","target":"ACCEPT"}}'

# 3. 启用防火墙
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/switch \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

### 场景 2: 禁止特定 IP 访问

```bash
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "chain": "INPUT",
    "rule": {
      "source": "192.168.1.200",
      "target": "DROP"
    }
  }'
```

---

### 场景 3: 限制特定 IP 带宽为 10Mbps

```bash
# 1. 启用 QoS
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/switch \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'

# 2. 添加 HTB qdisc
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "type": "qdisc",
    "device": "wt_br0",
    "handle": "1:",
    "kind": "htb",
    "args": {"default": "10"}
  }'

# 3. 添加 10Mbps class
curl -X POST http://192.168.99.1:8080/api/v1/nodes/0/qos/rules \
  -H "Content-Type: application/json" \
  -d '{
    "action": "add",
    "type": "class",
    "device": "wt_br0",
    "parent": "1:",
    "classid": "1:10",
    "kind": "htb",
    "args": {"rate": "10mbit", "ceil": "10mbit"}
  }'

# 4. 添加 filter 匹配目标 IP
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
    "args": {"match": "ip dst 192.168.99.2/32"},
    "flowid": "1:10"
  }'
```

---

## 技术细节

### 系统架构

```
用户请求 (curl/HTTP)
    ↓
wtsl_app (HTTP 服务器)
    ↓
RESTful API 路由
    ↓
┌─────────────┬─────────────┐
│   ACL API   │   QoS API   │
│  (iptables) │     (tc)    │
└──────┬──────┴──────┬──────┘
       ↓             ↓
┌─────────────┬─────────────┐
│  iptables   │      tc     │
│   命令执行  │   命令执行  │
└──────┬──────┴──────┬──────┘
       ↓             ↓
┌─────────────┬─────────────┐
│  Netfilter  │  Kernel TC  │
│  (内核模块) │  (内核模块) │
└─────────────┴─────────────┘
```

### URL 路由规则

代码中的 URL 解析逻辑：
```c
// GET 请求
/api/v1/nodes/{id}/{func}/{way}
// func = "acl" 或 "qos"
// way = "status" 或 "rules"

// POST 请求
/api/v1/nodes/{id}/{func}/{way}
// func = "acl" 或 "qos"  
// way = "switch" 或 "rules"
```

### iptables 锁文件问题

**问题**:
```
Fatal: can't open lock file /tmp/run/xtables.lock: No such file or directory
```

**解决方案**:
代码已自动处理，在应用启动时：
```c
// 1. 创建目录
mkdir("/tmp/run", 0755);

// 2. 设置环境变量
setenv("XTABLES_LIBDIR", "/usr/lib/xtables", 1);

// 3. 所有命令使用 -w 参数等待锁
iptables -w -A INPUT ...
```

### 内核模块限制

当前内核已裁剪，缺少大多数 netfilter 扩展模块。

**支持的匹配条件**:
- ✅ 协议 (`-p tcp/udp/icmp`)
- ✅ 源/目标地址 (`-s`, `-d`)
- ✅ 接口 (`-i`, `-o`)
- ✅ 端口 (`--dport`, `--sport`)
- ✅ ICMP 类型 (`--icmp-type`)

**不支持的扩展**:
- ❌ `state` / `conntrack` 模块（部分系统可能支持）
- ❌ `multiport` 模块
- ❌ `LOG` 目标

---

## 故障排查

### 常见问题

#### 1. iptables 报错 "can't open lock file"

**检查**:
```bash
ls -la /tmp/run/
```

**解决**:
```bash
sudo mkdir -p /tmp/run
sudo chmod 755 /tmp/run
```

---

#### 2. API 返回 404 错误

**可能原因**: URL 路径错误

**检查**:
```bash
# 正确的 URL
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/status
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/switch
curl http://192.168.99.1:8080/api/v1/nodes/0/acl/rules

# 错误的 URL (不要用 stats)
# curl http://192.168.99.1:8080/api/v1/nodes/0/acl/stats  # 404
```

---

#### 3. API 返回 500 错误

**检查**:
```bash
# 1. 检查 root 权限
whoami

# 2. 查看应用日志
tail -f /var/log/wtsl_app.log

# 3. 手动测试命令
iptables -L -n
tc -s qdisc show dev wt_br0
```

**解决**:
```bash
sudo ./bin/arm/wtsl_app
```

---

### 调试命令

#### iptables 调试

```bash
# 查看所有规则
iptables -L -n -v

# 查看规则（带行号）
iptables -L -n --line-numbers

# 查看规则（精确格式）
iptables -S
```

#### tc 调试

```bash
# 查看 qdisc
tc -s qdisc show dev wt_br0

# 查看 class
tc -s class show dev wt_br0

# 查看 filter
tc -s filter show dev wt_br0
```

---

## 附录

### A. 编译说明

#### 环境要求

- **工具链**: arm-mix510-linux-gcc 10.3.0
- **依赖库**: libcjson, libmicrohttpd, libsle_host_arm

#### 编译步骤

```bash
# 1. 设置工具链路径
export PATH="/home/admin/Downloads/toolchain/arm-mix510-linux/bin:$PATH"

# 2. 进入项目目录
cd /home/admin/Downloads/wtsl_app/wtsl_app

# 3. 清理并编译
make clean && make

# 4. 验证输出
ls -la bin/arm/wtsl_app
```

---

### B. 测试脚本

```bash
#!/bin/bash
# test_api.sh - API 快速测试

BASE_URL="http://192.168.99.1:8080"

echo "=== 防火墙 API 测试 ==="

# 获取状态
curl $BASE_URL/api/v1/nodes/0/acl/status

# 启用防火墙
curl -X POST $BASE_URL/api/v1/nodes/0/acl/switch \
  -H "Content-Type: application/json" -d '{"enabled": true}'

# 添加规则
curl -X POST $BASE_URL/api/v1/nodes/0/acl/rules \
  -H "Content-Type: application/json" \
  -d '{"action":"add","chain":"INPUT","rule":{"protocol":"tcp","dest_port":"22","target":"ACCEPT"}}'

echo ""
echo "=== QoS API 测试 ==="

# 获取状态
curl $BASE_URL/api/v1/nodes/0/qos/status

# 启用 QoS
curl -X POST $BASE_URL/api/v1/nodes/0/qos/switch \
  -H "Content-Type: application/json" -d '{"enabled": true}'
```

---

### C. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.1 | 2026-05-18 | 修正 URL 路径（stats → status, stats → switch） |
| 1.0 | 2026-05-18 | 初始版本 |

---

**文档维护**: 开发团队  
**最后更新**: 2026-05-18 10:36 GMT+8
