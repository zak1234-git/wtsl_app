# iptables 可用命令清单（精简内核环境）

> 设备：iptables v1.8.2 (legacy)  
> 内核已裁剪，缺少大多数 netfilter 扩展模块（如 state、multiport、LOG 等）。  
> 本清单仅包含当前环境 **确认可用** 的功能，并提供具体命令示例。  
> 编写 API 时请严格遵循这些限制，并使用 `-w` 锁。

---

## 1. 查看规则
```bash
# 列出 filter 表所有链（默认表）
iptables -L

# 以数字形式显示地址/端口，避免DNS解析
iptables -L -n

# 显示详细信息（接口名、包/字节计数）
iptables -L -n -v

# 显示精确计数器值（不做单位换算，如用字节数）
iptables -L -n -v -x

# 显示行号（用于按编号删除/替换）
iptables -L -n --line-numbers

# 以 iptables-save 格式输出（推荐脚本/API使用，易解析）
iptables -S

# 查看指定表（如 nat 表）
iptables -t nat -L -n -v
iptables -t nat -S
```

------

## 2. 链管理

```
# 创建自定义链
iptables -N MY_CHAIN

# 重命名自定义链
iptables -E MY_CHAIN NEW_CHAIN

# 清空自定义链中的规则（删除前必须为空）
iptables -F NEW_CHAIN

# 删除自定义链（链为空且无引用）
iptables -X NEW_CHAIN

# 清空内置链所有规则（不影响默认策略）
iptables -F INPUT
iptables -F

# 重置所有链的计数器
iptables -Z
iptables -Z INPUT

# 设置内置链的默认策略（只能为 ACCEPT 或 DROP）
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT
```



------

## 3. 规则增、删、改、查

```
# 追加规则到链末尾
iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 插入规则到第1条位置（最前面）
iptables -I INPUT 1 -s 192.168.1.100 -j DROP

# 插入规则到第3条位置
iptables -I INPUT 3 -p icmp -j ACCEPT

# 替换第2条规则
iptables -R INPUT 2 -p tcp --dport 80 -j ACCEPT

# 按匹配条件删除规则（条件必须完全一致）
iptables -D INPUT -s 192.168.1.100 -j DROP

# 按行号删除规则（先通过 -L --line-numbers 查看编号）
iptables -D INPUT 3

# 检查规则是否存在（返回0表示存在）
iptables -C INPUT -p tcp --dport 22 -j ACCEPT
echo $?   # 输出 0 或 1
```



------

## 4. 可用匹配条件（无需 `-m` 扩展模块）

这些匹配直接由内核基础网络栈支持，**无需加载任何额外模块**，均可正常使用。

### 4.1 协议、地址、接口

```
# 协议 -p（tcp, udp, icmp, all）
iptables -A INPUT -p tcp -j ACCEPT

# 源地址 -s（支持掩码）
iptables -A INPUT -s 10.0.0.0/8 -j DROP

# 目标地址 -d
iptables -A OUTPUT -d 172.16.0.1 -j REJECT

# 入口接口 -i（用于 INPUT/FORWARD/PREROUTING 链）
iptables -A INPUT -i eth0 -j ACCEPT

# 出口接口 -o（用于 OUTPUT/FORWARD/POSTROUTING 链）
iptables -A OUTPUT -o wlan0 -j ACCEPT

# 接口通配符 +（如 eth+ 匹配 eth0、eth1 等）
iptables -A INPUT -i eth+ -j ACCEPT
```



### 4.2 取反（!）

```
# 不来自 10.0.0.0/8 的流量
iptables -A INPUT ! -s 10.0.0.0/8 -j DROP

# 不是 22 端口的 TCP 包
iptables -A INPUT -p tcp ! --dport 22 -j DROP

# 不是从 eth0 进入的流量
iptables -A INPUT ! -i eth0 -j DROP
```



### 4.3 TCP/UDP 端口（需先指定 -p tcp 或 -p udp）

```
# 单个端口
iptables -A INPUT -p tcp --dport 443 -j ACCEPT
iptables -A INPUT -p udp --sport 53 -j ACCEPT

# 端口范围
iptables -A INPUT -p tcp --dport 1024:65535 -j ACCEPT
iptables -A OUTPUT -p udp --dport 8000:9000 -j DROP
```



### 4.4 ICMP 类型（需先指定 -p icmp）

```
# 接受 ping 请求（echo-request）
iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT

# 禁止 ping 请求
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP

# 接受 ICMP 目标不可达
iptables -A INPUT -p icmp --icmp-type destination-unreachable -j ACCEPT
```



### 4.5 分片包匹配（-f）

```
# 丢弃非首片的后续分片（防御分片攻击）
iptables -A INPUT -f -j DROP
```



------

## 5. 可用目标（-j / -g）

### 5.1 内建目标（直接可用）

```
# 接受
iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 丢弃
iptables -A INPUT -s 10.0.0.0/8 -j DROP

# 从自定义链返回
iptables -A MY_CHAIN -d 1.2.3.4 -j RETURN
```



### 5.2 REJECT

```
# 如果可用，拒绝流量并返回 ICMP 端口不可达
iptables -A INPUT -s 192.168.1.0/24 -j REJECT --reject-with icmp-port-unreachable
```



### 5.3 跳转到自定义链（-j）与无返回跳转（-g）

```
# 创建两个自定义链
iptables -N CHAIN_A
iptables -N CHAIN_B

# 向 CHAIN_A 添加规则：如果源 IP 匹配则 DROP，否则返回
iptables -A CHAIN_A -s 10.99.0.0/16 -j DROP
iptables -A CHAIN_A -j RETURN

# 从 INPUT 跳转到 CHAIN_A（执行完会返回）
iptables -A INPUT -p tcp -j CHAIN_A

# 使用 -g 无返回跳转：匹配的包直接进入 CHAIN_B，不返回原链
iptables -A INPUT -p udp -g CHAIN_B
iptables -A CHAIN_B -j DROP   # CHAIN_B 末尾直接 DROP，不会再回到 INPUT
```



------

## 6. 表操作（-t）

```
# 操作 filter 表（默认，通常可省略 -t filter）
iptables -t filter -A INPUT -s 1.2.3.4 -j DROP

# 操作 nat 表（需确认 SNAT/DNAT 等目标是否可用）
iptables -t nat -L -n -v

# 操作 mangle 表（基本 mangle 表存在，但可能缺少扩展目标）
iptables -t mangle -L -n -v
```



------

## 7. 并发锁（强烈推荐在 API 中使用）

```
# 添加规则时，最多等待 5 秒以获取 xtables 锁
iptables -w 5 -A INPUT -p tcp --dport 443 -j ACCEPT

# 调整轮询间隔为 100 毫秒（100000 微秒）
iptables -w 5 -W 100000 -A INPUT -p tcp --dport 80 -j ACCEPT
```



------

## 8. 规则保存与恢复

```
# 将当前所有表规则保存到文件
iptables-save > /etc/iptables/rules.v4

# 从文件恢复规则（会覆盖当前规则）
iptables-restore < /etc/iptables/rules.v4

# 在恢复前检查文件语法是否正确
iptables-restore -t < rules-file
```