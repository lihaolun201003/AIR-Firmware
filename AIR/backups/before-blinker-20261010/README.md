# AIR 空气监测屏

NUCLEO-F411RE + ILI9341 2.8 英寸 SPI 屏 + PMS5003ST + HC-06。基于 PlatformIO / STM32duino 3.0。

## HC-06 蓝牙迭代

2026-10-10 加入 HC-06：TXD → PA3，RXD → PA2，支持 5V 的底板 VCC → 5V，GND → GND。默认构建 `nucleo_f411re` 使用 USART2、9600 8N1，按手机 App 协议每秒发送 `$ENV,PM1.0,PM2.5,PM10,HCHO,温度,湿度#`。甲醛单位 mg/m³，温度 °C，湿度 %；仅发送有效传感器读数。支持 `PING`、`GET`、`STREAM ON`、`STREAM OFF`、`HELP`（命令后附换行），保留开机动画和屏幕局部更新。

**使用上述接线前必须确认焊桥：SB13/SB14 断开，SB62/SB63 接通。** PA2/PA3 原用于 ST-Link 调试串口，蓝牙接法下 COM3 不再提供固件日志，ST-Link 下载和断点调试仍可使用。调试构建 `nucleo_f411re_debug` 保留原 115200 串口日志，需要断开 HC-06 并恢复调试焊桥。

接线位置、焊桥确认、安卓使用、App 数据格式和测试方法见 [HC-06 完整说明](docs/HC06-integration.md)。用户已确认硬件就绪及六字段顺序。最新 `$ENV` 固件已通过 ST-Link 烧录，返回 `Verified OK` 并自动复位，20 项本机测试通过。详情见 `logs/hc06-env-verification.json`，手机端实际接收仍需联调确认。初版 JSON 源码和固件备份在 `backups/before-env-protocol-20261010/`，更早版本备份在 `backups/before-hc06-20261010/`。

## 开机动画

2026-10-05 增加用户提供的角色图：白底、角色从左侧短距离滑入并轻轻起伏，右侧显示 AIR MONITOR、跳动圆点及加载条。约 3 秒后自动进入空气监测页。上电或按开发板黑色 RESET 按钮都会播放。

启动白屏优化：`BootDisplay` 在初始化后保持显示关闭，先写入包含角色、文字及加载条的完整首帧，再发送 `DISPON`。第一帧已有可见角色，避免先出现空白底图；硬件复位改为低电平 10 ms、释放后等待 120 ms，退出休眠后保留 120 ms 等待，移除了额外的开机延时。初始化配置基于 Adafruit ILI9341 1.6.4，保留原有电源和 Gamma 参数，未修改依赖库。时序参考 [ILI9341 数据手册](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)。

当前 LED 直连 3V3，背光上电就亮。显示关闭命令并不等于关闭背光，因此软件不能保证消除上电及复位期间的所有白光；如需完全隐藏该阶段，需要可控背光电路，在首帧准备好后再打开背光。此次修改不改变接线。先前版本保存在 `backups/before-startup-flash-fix-20261005-134112`。

动画期间持续处理 PMS5003ST 和蓝牙串口，进入主界面立即显示已收到的最新真实读数。传感器未连接时动画也会按时结束，主界面按原逻辑显示等待数据。

- `assets/boot-character-original.png`：用户上传的原始图片，完整保留。
- `assets/boot-preview.gif`：最初版本的动画布局预览（当前入场距离已缩短）；`boot-preview.png` 为放大静帧。
- `src/boot_animation.cpp`：动画时序和分条绘制。
- `src/boot_display.cpp`、`include/boot_display.h`：延迟开启显示的初始化驱动。
- `include/boot_character_data.h`：149×208 像素、4 位灰度的固件图像，占用 15496 字节 Flash。
- `scripts/generate_boot_asset.py`：把原图等比导出为固件数组，运行 `python scripts/generate_boot_asset.py` 可重新生成，需要 Pillow。

动画使用 5 KiB 分条缓冲区。修改前版本保存在 `backups/before-boot-animation-*`，本次构建、烧录和串口日志以 `logs/boot-` 开头。

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

- USART1 接收格式为 9600、8N1；默认构建将 USART2 分配给 HC-06、9600、8N1。恢复调试接线后，`nucleo_f411re_debug` 使用 ST-Link COM3、115200、8N1。
- 按 PMS5003ST 的 40 字节帧解析，要求帧头 `42 4D`、长度 `00 24`，并验证前 38 字节之和等于末尾的 16 位校验和。
- 温度是有符号 16 位数 / 10，湿度 / 10，甲醛 / 1000 转为 mg/m³。
- 仅完整且校验通过的帧才更新数据。噪声、错误长度、坏校验和及中途丢字节会重新寻找帧头；半帧停顿超过 250 ms 则丢弃。
- 接收采用中断缓冲，缓冲区为 256 字节；屏幕最多每 500 ms 局部更新。
- 无数据时显示 `--` 和 `WAITING`，首次收到有效帧后显示 30 秒 `WARMUP`，随后显示 `LIVE`。
- 超过 10 秒没有有效帧时显示 `NO DATA` 并清空读数；收到传感器错误码时显示 `SENSOR ERROR` 并清空读数。重新收到正常数据后自动恢复。
- 底部 `Frames` 是校验通过的帧计数，`Age` 是最后一帧距今的秒数，`Bad` 是长度或校验错误次数。

30 秒等待用于颗粒物风扇稳定；厂家另要求甲醛探头首次上电至少 48 小时以充分极化。HCHO 显示的是传感器上报值。

## 工程文件

- `src/main.cpp`：引脚、两个串口的服务、屏幕界面及调试输出。
- `src/air_bluetooth.cpp`、`include/air_bluetooth.h`：`$ENV` 数据上报、命令解析和发送缓冲。
- `include/air_config.h`：蓝牙开关和模块 UART 波特率。
- `test/test_air_bluetooth/test_main.cpp`：14 项本机蓝牙协议测试，包括手机 App 的完整示例。
- `include/pms5003st.h`：独立的 40 字节解析器。
- `test/test_pms_parser/test_main.cpp`：可在本机及开发板上运行的 6 项解析测试。
- `docs/PMS5003ST-manual-v2.6.pdf`：Plantower 厂家协议手册，附录 I 为帧格式。
- `logs/pms-build.log`、`pms-upload.log`、`pms-serial.log`、`pms-parser-tests.log`：本次验证记录。
- `backups/before-pms5003st-*`：本次修改前已正常工作的屏幕测试源码、配置与固件。

## 编译、烧录和测试

在此目录执行；烧录蓝牙构建前先确认上文的焊桥及接线：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re -t upload
```

固件在 `.pio/build/nucleo_f411re/firmware.bin` 和 `firmware.elf`。按开发板 RESET 可重新启动。蓝牙数据在安卓 SPP 串口终端中查看，详细步骤见 [HC-06 说明](docs/HC06-integration.md)。

本机测试不烧录开发板，需要 PATH 中可用的 `g++`：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native
```

恢复调试焊桥并断开 HC-06 后，可使用原串口日志。开发板解析测试会临时替换应用，结束后应恢复正式程序：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e nucleo_f411re_debug -f test_pms_parser --test-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_debug -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM3 -b 115200
```

项目保留了已验证的两个环境兼容处理：`include/wiring_private.h` 供 Adafruit ILI9341 1.6.4 与 STM32duino 3.0 配合编译；`scripts/windows_paths.py` 使用相对 map 路径，支持当前中文工程目录。

## 验证记录

2026-10-03：6 项解析测试均在 NUCLEO-F411RE 上通过；正式程序编译通过，ST-Link 烧录返回 `Verified OK`；40 秒采集到 42 帧连续真实 PMS5003ST 数据，长度、校验及半帧超时错误均为 0。前两帧原始字节经过独立 Python 解码，与固件输出的六项读数完全一致。详细验证及固件 SHA-256 见 `logs/pms-verification.json`。

2026-10-03 传感器版本占用 Flash 27712 字节、静态 RAM 2268 字节。用户已确认所有读数显示正常且持续更新。

2026-10-05 开机动画版本编译通过，ST-Link 烧录校验通过；占用 Flash 44668 字节、静态 RAM 7388 字节。实测动画用时 3465 ms，绘制 31 帧；动画期间收到 5 帧传感器数据，自动进入监测页后继续收到 17 帧，未发生校验、长度或半帧超时错误。用户已确认动画正常，随后数据正常更新。完整记录见 `logs/boot-verification.json`。

2026-10-05 启动白屏优化版本编译通过，ST-Link 返回 `Verified OK`；Flash 45200 字节、静态 RAM 7392 字节。本次启动在 uptime 559 ms 时发送首帧开启显示命令，动画用时 3482 ms，绘制 31 帧。动画期间收到 4 帧真实传感器数据，进入监测页后继续收到 17 帧，校验、长度和半帧超时错误均为 0。用户按 RESET 实屏确认：不再明显闪白，动画和数据正常。该反馈验证复位启动效果，不代表已测试所有冷上电条件。构建、烧录、串口及验证记录以 `logs/startup-` 开头。

协议来源：[Plantower PMS5003ST V2.6 厂家手册镜像](https://raw.githubusercontent.com/Arduinolibrary/DFRobot_SEN0233_Air_Quality_Monitor/master/PMS5003ST%20series%20data%20manua_English_V2.6%20.pdf)。
