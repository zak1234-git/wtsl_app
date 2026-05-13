目前测试发现海思问题如下：

平台：Hi2921+Hi2981

Host驱动版本：T215

1. T节点信道设置为549后，后台查询为0，设置其他值也一样；另外发现T节点设置essid设置后返回也是空的

  ```
  /sys/kernel/config/usb_gadget/g1 # iwpriv vap0 cfg get_channel
  vap0      cfg:[SUCC]channel = 0
  
  /sys/kernel/config/usb_gadget/g1 # iwpriv vap0 cfg "get_essid"
  vap0      cfg:[SUCC]ssid = 
  
  ```

2. 多个G节点如果使用**同一信道**，T节点扫描只能扫到一个

   

3. 同时开启多个G设备，不同信道，能查询到，但连接不上

  ```
  /home/wt # iwpriv vap0 cfg show_bss
  
  vap0      cfg:[SUCC]
  
  DOMAIN_NAME     CELL_ID  CHANNEL RSSI DOMAIN_CNT
  
  lry_gnode_02    9        2479    -12  1
  
  yh_Gnode_045    9        2291    -37  1
  
  lry_gnode_001   9        709     -9   1
  
  BSS SCANED NUM:3.
  
  /home/wt # iwpriv vap0 cfg "start_join 0"
  
  vap0      cfg:[SUCC]joined bss 0
  
  /home/wt # iwpriv vap0 cfg view_users
  
  vap0      cfg:[SUCC]
  
  vap_role[1], vap_state[4], user_num[0], scanning[0], roamming[0]
  
  uid mac_addr
  
  /home/wt # iwpriv vap0 cfg view_users
  
  vap0      cfg:[SUCC]
  
  vap_role[1], vap_state[4], user_num[0], scanning[0], roamming[0]
  
  uid mac_addr
  
  /home/wt # 
  ```

  

4. T节点扫描会出现空名字的情况

  ```
  	DOMAIN_NAME     CELL_ID  CHANNEL RSSI DOMAIN_CNT
  
  					9        2563    -68  1
  
  	yh_Gnode_045    9        2291    -47  1
  
  					9        2125    -69  1
  
  					9        1959    -72  1
  
  					9        1875    -72  1
  
  	lry_gnode_001   9        709     -9   1
  
  	lry_gnode_02    9        291     -12  1
  ```

5. 没有断开连接的接口

  1G多T连接后，想只断开其中一个T，需要断开接口，不影响其他T与G的连接

6. G与T切换后，不重启不能正常入网

  用load_gt.sh进行G/T切换，不重启不能正常入网，希望G/T切换能有支持不需要重启的方法

7. G节点修改属性需要down/up vap0的问题

  修改属性，down/up接口会断开连接的所有T，能否优化

8. 是否有接入控制接口，比如：输入正确的秘钥才能接入，或者其他的方法，目前版本接入层密钥口令，PSK等无效
9. 
10. 其他问题