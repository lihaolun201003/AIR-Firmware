# Blinker USART6 接线（NUCLEO-F411RE）

APP 截图确认温度组件键名为 `temp`、实时数据已启用，设备页面显示已连接但数值为 0。用户随后确认模块接在 D0/D1、焊桥未改或不确定，选择不焊接改线方案，并要求先改线再烧录。

2026-10-10 完成：用户确认改线后，`nucleo_f411re_uart6` 固件通过 ST-Link 烧录，返回 `Verified OK` 并复位；用户随后反馈「有数据了」。这确认改线后的手机接收路径可用。完整构建、烧录与本机检查记录见 `logs/blinker-uart6-verification-20261010.json`；两个组件的具体读数未由代理逐一查看。固件继续在同一 JSON 中发送 `temp` 与 `humi`。

NUCLEO-F411RE 的默认 SB62/SB63 为断开，主控 PA2/PA3 与 D1/D0 排针之间不连通。手机与模块连接，不代表模块收到主控数据。新环境 `nucleo_f411re_uart6` 使用直接引出的 USART6，绕过这段排针路由；模块的实际 BLE 型号与手机接收仍需现场确认。

## 断电后改两根信号线

先拔掉 USB，并断开其他供电。

| 蓝牙模块引脚 | 原位置 | 新位置 |
| --- | --- | --- |
| RXD（模块接收） | D1 / PA2 | **PA11，CN10-14（主控 USART6 TX）** |
| TXD（模块发送） | D0 / PA3 | **PA12，CN10-12（主控 USART6 RX）** |
| VCC | 原供电位置 | 保持原接法 |
| GND | 原共地位置 | 保持原接法 |

CN10 是开发板标为 CN10 的双排 ST morpho 排针；12、14 是这个连接器的针脚编号，不是 Arduino 的 D12、D14。CN10 的偶数针脚在同一列；12 与 14 相邻，14 比 12 多两号。优先按板子对应的官方引脚图核对 PA11、PA12，不要仅凭方向猜位置。

本方案保持默认焊桥，屏幕与 PMS5003ST 接线不动。改完重新接上 USB，确认板子供电，然后烧录配套环境：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_uart6 -t upload
```

蓝牙串口保持 9600、8N1。不要用旧 `nucleo_f411re` 的固件配这套新接线。APP 温度数字组件键名为 `temp`，湿度数字组件键名为 `humi`；保存并退出编辑状态，再连接设备观察与监测屏一致的实时读数。

## 调试与验证

USART2 留给 ST-Link VCP。COM3 调试日志为 115200，可查看传感器读数与有效帧计数；这些日志仍不能独立证明手机收到蓝牙数据。最终应以 APP 温湿度持续更新为准。

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM3 -b 115200
```

备份：`backups/before-uart6-20261010-181321/` 保存修改前源码、配置、说明及原 USART2 固件。若恢复旧固件，也要恢复对应接线与焊桥设置。

依据：[ST 官方 UM1724，USART 路由与 NUCLEO-F411RE 引脚图](https://www.st.com/resource/en/user_manual/dm00105823-nucleo-f401re-user-manual-stmicroelectronics.pdf)，以及本机 STM32duino `F411R(C-E)T/PeripheralPins.c` 的 USART6 TX=PA11、RX=PA12 映射。
