# iperf 测试 

```
make CONFIG_CHIP_NAME=BL602 CONFIG_LINK_ROM=1 -j 
```

PC服务端UDP测试
```
iperf.exe -s -u -i 1
```

```
ipu 192.168.8.101 (UDP测试，192.168.8.101是PC的IP地址)
```

PC服务端TCP测试
```
iperf -s -i 1
```

```
ipc 192.168.8.101 (TCP测试，192.168.8.101 是 PC 的 IP 地址)
```

BL端作为服务端
```
ips 
```
PC端作为连接端
```
iperf.exe -c 192.168.8.100 -t 360 -i 1 (192.168.8.100是模组的IP地址)
$iperf.exe -u -c 192.168.8.100 -t 360 -i 1 (192.168.8.100 是模组的IP地址)
```

