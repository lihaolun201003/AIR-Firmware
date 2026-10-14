# AIR 空气监测屏

NUCLEO-F411RE + ILI9341 2.8 英寸 SPI 屏 + PMS5003ST。基于 PlatformIO / STM32duino 3.0。

## 接线

### PMS5003ST

| 传感器接口 | NUCLEO-F411RE |
| --- | --- |
| VCC（1） | 5V |
| GND（2） | GND |
| TXD（5） | D2 / PA10，USART1 RX |
| RXD（4） | 不接，使用默认主动上报 |
| SET（3）、RESET（6） | 未使用时悬空，传感器内部上拉 |
| NC（7、8） | 不接 |

这是本次用户确认的三线接法。传感器供电为 5V，串口逻辑为 3.3V。程序不向传感器发送命令；若传感器曾被设置为被动模式或休眠，先将传感器断电再上电，恢复默认主动上报。

STM32duino 的串口初始化需要 TX 引脚，所以程序显式使用 `Uart sensor(PA10, PA9)`，PA9 / D8 留空，不需要加线。显式选脚也避免默认 USART1 TX 引脚占用屏幕的 PB6。

### ILI9341 屏幕

| LCD 接口 | NUCLEO-F411RE |
| --- | --- |
| VCC | 5V |
| GND | GND |
| CS | D10 / PB6 |
| RESET | D7 / PA8 |
| DC | D4 / PB5 |
| SDI (MOSI) | D11 / PA7 |
| SCK | D13 / PA5 |
| LED | 3V3，背光常亮 |
| SDO (MISO) | 不接 |

屏幕、传感器、开发板共地。触摸、SD 卡不参与程序。PA5 同时接板载用户 LED，不能再将它配置成普通闪灯 GPIO。

## 显示与接收

主界面显示环境浓度 PM2.5、PM1.0、PM10（µg/m³），温度（°C），相对湿度（%）和 HCHO 甲醛（mg/m³）。PM 使用大气环境字段；CF=1 字段和六档粒子计数也被解析并保存在 `PmsReading` 中。

- USART1 接收格式为 9600、8N1；PC 调试串口仍为 ST-Link COM3、115200、8N1。
- 按 PMS5003ST 的 40 字节帧解析，要求帧头 `42 4D`、长度 `00 24`，并验证前 38 字节之和等于末尾的 16 位校验和。
- 温度是有符号 16 位数 / 10，湿度 / 10，甲醛 / 1000 转为 mg/m³。
- 仅完整且校验通过的帧才更新数据。噪声、错误长度、坏校验和及中途丢字节会重新寻找帧头；半帧停顿超过 250 ms 则丢弃。
- 接收采用中断缓冲，缓冲区为 256 字节；屏幕最多每 500 ms 局部更新。
- 无数据时显示 `--` 和 `WAITING`，首次收到有效帧后显示 30 秒 `WARMUP`，随后显示 `LIVE`。
- 超过 10 秒没有有效帧时显示 `NO DATA` 并清空读数；收到传感器错误码时显示 `SENSOR ERROR` 并清空读数。重新收到正常数据后自动恢复。
- 底部 `Frames` 是校验通过的帧计数，`Age` 是最后一帧距今的秒数，`Bad` 是长度或校验错误次数。

30 秒等待用于颗粒物风扇稳定；厂家另要求甲醛探头首次上电至少 48 小时以充分极化。HCHO 显示的是传感器上报值。

## 工程文件

- `src/main.cpp`：引脚、串口接收、屏幕界面及调试输出。
- `include/pms5003st.h`：独立的 40 字节解析器。
- `test/test_pms_parser/test_main.cpp`：在开发板上运行的 6 项解析测试。
- `docs/PMS5003ST-manual-v2.6.pdf`：Plantower 厂家协议手册，附录 I 为帧格式。
- `logs/pms-build.log`、`pms-upload.log`、`pms-serial.log`、`pms-parser-tests.log`：本次验证记录。
- `backups/before-pms5003st-*`：本次修改前已正常工作的屏幕测试源码、配置与固件。

## 编译、烧录和测试

在此目录执行：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM3 -b 115200
```

固件在 `.pio/build/nucleo_f411re/firmware.bin` 和 `firmware.elf`。按开发板 RESET 可重新启动。

解析测试会临时烧录测试程序，执行后应重新烧录正式应用：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e nucleo_f411re -f test_pms_parser --test-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re -t upload
```

项目保留了已验证的两个环境兼容处理：`include/wiring_private.h` 供 Adafruit ILI9341 1.6.4 与 STM32duino 3.0 配合编译；`scripts/windows_paths.py` 使用相对 map 路径，支持当前中文工程目录。

## 本次验证

2026-10-03：6 项解析测试均在 NUCLEO-F411RE 上通过；正式程序编译通过，ST-Link 烧录返回 `Verified OK`；40 秒采集到 42 帧连续真实 PMS5003ST 数据，长度、校验及半帧超时错误均为 0。前两帧原始字节经过独立 Python 解码，与固件输出的六项读数完全一致。详细验证及固件 SHA-256 见 `logs/pms-verification.json`。

程序占用 Flash 27712 字节、静态 RAM 2268 字节。用户已确认所有读数显示正常且持续更新。

协议来源：[Plantower PMS5003ST V2.6 厂家手册镜像](https://raw.githubusercontent.com/Arduinolibrary/DFRobot_SEN0233_Air_Quality_Monitor/master/PMS5003ST%20series%20data%20manua_English_V2.6%20.pdf)。
