# AIR Firmware

基于 NUCLEO-F411RE 的空气监测固件，使用 PMS5003ST、ILI9341 TFT 显示屏和 HC-06 蓝牙模块。

工程位于 [AIR/](AIR/)，使用 PlatformIO / STM32duino。默认环境为 nucleo_f411re_uart6。

~~~sh
cd AIR
pio run -e nucleo_f411re_uart6
pio test -e native
~~~

- [工程说明与接线](AIR/README.md)
- [USART6 蓝牙接线](AIR/docs/Blinker-uart6-wiring.md)
- [Blinker 协议与组件设置](AIR/docs/Blinker-integration.md)
- [源码](AIR/src/) · [头文件](AIR/include/) · [测试](AIR/test/)
- [已保存固件](AIR/firmware/) · [验证记录](AIR/logs/) · [历史备份](AIR/backups/)

本仓库保留原“02_固件工程”目录结构；PlatformIO 构建缓存按工程现有 .gitignore 排除。
