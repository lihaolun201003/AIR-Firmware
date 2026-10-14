# HC-06 蓝牙迭代说明（2026-10-10）

本文记录原 `$ENV,...#` 版本。当前默认固件已适配 Blinker JSON 数值组件，使用方式见 [Blinker 接入说明](Blinker-integration.md)。需要本文的旧协议时，构建/烧录环境改为 `nucleo_f411re_env`；以下历史验证记录与旧固件副本仍保留。

结论：NUCLEO-F411RE 可以同时运行 ILI9341、PMS5003ST 和 HC-06，按所给接线实现了固件。必须先确认 USART2 焊桥：PA2/PA3 原来用于 ST-Link 虚拟串口，不能作为两个独立串口同时使用。

目前完成源码迭代、本机测试和两种固件构建。2026-10-10 用户确认硬件就绪及手机 App 的六字段顺序后，最新 `$ENV` 固件已通过 ST-Link 写入，返回 `Verified OK`，开发板自动复位。焊桥状态由用户确认就绪，未由代理实物测量；手机实际接收尚未验证。工程没有 PCB 原理图或布局文件，本次交付为固件及开发板接线说明。

## 接口分配与可行性

| 外设 | MCU 接口 | 引脚 | 本次处理 |
| --- | --- | --- | --- |
| ILI9341 | SPI1 + GPIO | MOSI PA7、SCK PA5、CS PB6、DC PB5、RESET PA8 | 保留原接线、显示和开机动画 |
| PMS5003ST | USART1，9600 8N1 | RX PA10，TX PA9 留空 | 保留主动接收和 40 字节校验 |
| HC-06 | USART2，9600 8N1 | RX PA3、TX PA2 | 每秒 `$ENV` 上报和命令回复 |
| ST-Link | SWD | PA13、PA14 | 保留下载和断点调试 |

原工程显式指定 `Uart sensor(PA10, PA9)`，避免 USART1 默认 TX 占用屏幕 PB6。蓝牙复用框架已有的 `Serial` 实例，在 `begin()` 前显式设置 PA3/PA2，不创建第二个 USART2 实例。初始化、动画和主循环中的旧日志仅在调试构建中启用。

芯片容量为 512 KiB Flash、128 KiB RAM，资源充足。最新 `$ENV` 固件资源占用见 `logs/hc06-env-verification.json`。UART 9600 8N1 的理论载荷约 960 字节/秒，每秒一个、小于 64 字节的数据包适合目前六项读数。

## HC-06 接线及焊桥

| HC-06 接口 | NUCLEO-F411RE | 说明 |
| --- | --- | --- |
| VCC | 5V | 仅适用于明确支持 5V 输入、带稳压器的底板 |
| GND | GND | 所有模块共地 |
| TXD | PA3 / D0 / CN10-37 | 蓝牙发送 → MCU 接收 |
| RXD | PA2 / D1 / CN10-35 | MCU 发送 → 蓝牙接收 |

STM32 与 HC-06 数据脚使用 3.3V 逻辑，可直接连接。底板支持 5V 供电不代表 UART 引脚应接 5V；裸模块的供电要求也不同。屏幕、PMS5003ST 的接线保持用户给出的方案。

ST 官方 UM1724 的 USART communication 一节及 Table 10 要求：

| 焊桥 | HC-06 接 PA2/PA3 | 原 ST-Link VCP 调试 |
| --- | --- | --- |
| SB13、SB14 | OFF，断开 | ON，接通 |
| SB62、SB63 | ON，接通 | OFF，断开 |

先断开 USB 和其他电源，再检查 PCB 焊桥标号，并用万用表确认两端是否短接。出厂状态通常连接 ST-Link VCP。所给引脚图能确定排针位置，无法确定实物焊桥是否已改；换用 CN10 排针也不会绕过焊桥。

这些是 PCB 焊桥，不是 CN2 的 SWD 跳帽。切换 UART 后，ST-Link 下载和断点调试仍可使用。不要把 ST-Link UART TX 与 HC-06 TX 同时连接到 MCU PA3。

供电应覆盖开发板、传感器、屏幕背光和蓝牙底板的总负载；屏幕及 HC-06 底板的具体型号尚未给出，供电余量需在实物上确认。

依据：[ST UM1724](https://www.st.com/resource/en/user_manual/um1724-stlinkv21-in-circuit-debuggerprogrammer-for-stm8-and-stm32-stmicroelectronics.pdf)，[DSD TECH HC-06 底板规格](https://www.deshide.com/product-details_HC-06.html)，[ITEAD HC-06 裸模块规格](https://itead.cc/product/serial-port-bluetooth-module-slave-hc-06/)。

## 安卓接收

HC-06 使用经典蓝牙 SPP，手机端使用支持 SPP 的串口终端。它不是 BLE 外设。DSD TECH 的 HC-06 产品说明指出其不支持 iPhone/iPad 的普通串口连接；用户选择的安卓适合当前方案。

1. 确认焊桥和接线完成，再上电并烧录蓝牙固件。
2. 在安卓系统蓝牙设置中找到模块当前名称并配对。厂家默认配对码为 `1234`，已被修改的模块使用实际密码。
3. 在 SPP 串口终端中选择已配对模块并连接。MCU 与 HC-06 的 UART 配置为 9600、8N1。
4. 默认每秒收到一个 `$ENV,...#` 数据包。发送 `PING` 并附换行，应收到 `$PONG#`；发送 `GET` 并附换行，应收到当前有效读数或 `$ERR,状态#`。

如果模块以前改过 UART 波特率，可通过构建标志 `-DAIR_BLUETOOTH_BAUD=实际波特率` 匹配。固件不会发送 AT 命令或修改模块名称、密码、波特率。

## 手机 App 数据协议：$ENV

用户已确认六项数据的顺序为 PM1.0、PM2.5、PM10、甲醛、温度、湿度。自动上报与 `GET` 的数据帧均采用纯 ASCII 文本，以 `$ENV,` 开头，以 `#` 结束。线上不发送 CR、LF 或 NUL；一次蓝牙读取可能包含半包或多个包，App 应累积接收并按 `#` 拆包，寻找 `$ENV,` 前缀。

截图协议示例已作为完整测试用例验证；实际数字来自传感器读数：

```text
$ENV,18,35,48,0.023,25.6,61.2#
```

| 位置 | 字段 | 单位和格式 |
| --- | --- | --- |
| 1 | PM1.0 | µg/m³，大气环境字段，整数 |
| 2 | PM2.5 | µg/m³，大气环境字段，整数 |
| 3 | PM10 | µg/m³，大气环境字段，整数 |
| 4 | HCHO | mg/m³，传感器原始值除以 1000，三位小数 |
| 5 | 温度 | °C，原始值除以 10，一位小数，支持负数 |
| 6 | 相对湿度 | %，原始值除以 10，一位小数 |

中间严格为六个数值，以英文逗号分隔，不携带字段名称、单位、空格或 JSON。首次正常接收、断流恢复或错误恢复后，屏幕保留 30 秒预热提示，预热期间也上报校验通过的读数。连续 10 秒没有有效帧、尚未收到帧或传感器报错时，停止自动发送 `$ENV`，恢复正常数据后自动继续。协议未提供状态字段，App 应用接收超时提示无数据；固件不会发送假零值或过期读数来填充六个字段。

| 命令，后接 CR、LF 或 CRLF | 回复 |
| --- | --- |
| `PING` | `$PONG#` |
| `GET` | `$ENV,...#`；无有效数据时为 `$ERR,WAITING#`、`$ERR,NO_DATA#` 或 `$ERR,SENSOR_ERROR#` |
| `STREAM ON` | `$ACK,STREAM,ON#` |
| `STREAM OFF` | `$ACK,STREAM,OFF#` |
| `HELP` | `$HELP,PING,GET,STREAM ON,STREAM OFF,HELP#` |

`STREAM OFF` 停止自动上报，传感器接收和屏幕更新持续运行，`GET` 仍可查询。重启恢复自动上报。诊断回复也以 `#` 结束，仅在收到命令时发送；App 可忽略非 `$ENV,` 帧。命令不区分大小写，允许首尾空格，最长 32 字符；超长或含非文本字节的命令丢弃至换行，返回 `$ERR,COMMAND_TOO_LONG#` 或 `$ERR,INVALID_COMMAND#`。半条命令停顿超过 1 秒后丢弃。未知命令返回 `$ERR,UNKNOWN_COMMAND#`。命令不会转发给 PMS 或作为 HC-06 AT 指令处理。

发送使用 1 KiB 环形队列，按 UART 可用空间分批写入。队列满时丢弃整个新包，保留已有完整帧；大量查询可能丢失回复，应等上一个回复完成再发下一条。暂停前已进入队列或 UART 缓冲的数据会发送完。每次服务最多接收 64 字节并限定发送量，动画绘制期间也服务两个串口。

四线 HC-06 未接 STATE，固件无法检测手机是否实际连接。屏幕 `Bluetooth: streaming/paused` 只表示自动上报开关。安卓端应设置接收超时，处理蓝牙链路断开；无手机连接时 MCU 仍向 HC-06 UART 发数据，软件队列不代表无线送达。

## 构建、测试和恢复

在项目根目录执行：

```powershell
# 本机测试，不烧录开发板；需要 PATH 中可用的 g++。
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native

# PA2/PA3 蓝牙固件。
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re

# 焊桥和接线确认后，通过 ST-Link 烧录。
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re -t upload
```

最新固件在 `.pio/build/nucleo_f411re/firmware.bin` 和 `firmware.elf`，另存副本在 `firmware/air-hc06-env-20261010.bin` 和 `firmware/air-hc06-env-20261010.elf`。较早的 `air-hc06-20261010` 文件为 JSON 版本。蓝牙接法下，ST-Link COM3 不再与目标 UART 相连，但系统可能仍显示该设备。接收端使用手机 SPP，或 Windows 配对后的蓝牙 COM 端口。

恢复 ST-Link 日志时，断电、断开 HC-06 数据线并恢复上表的调试焊桥，再执行：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_debug -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM3 -b 115200
```

开发板 PMS 测试可在调试接线下运行，测试固件会替换正式程序，结束后应恢复应用：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e nucleo_f411re_debug -f test_pms_parser --test-port COM3
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_debug -t upload
```

初版 JSON 源码和固件保存在 `backups/before-env-protocol-20261010/`，更早版本在 `backups/before-hc06-20261010/`。最新构建记录为 `logs/hc06-env-build.log`，测试为 `logs/hc06-env-native-tests.log`，烧录为 `logs/hc06-env-upload.log`，验证详情及文件散列为 `logs/hc06-env-verification.json`。

蓝牙版和调试版均构建成功，14 项蓝牙测试及 6 项 PMS 回归测试通过，覆盖截图示例、字段顺序、单位换算、负温度、无效数据静默、预热/超时边界、计时回绕、命令分片、发送拥塞和 PMS 到 `$ENV` 的完整路径。最新蓝牙固件烧录校验通过；新固件的屏幕、传感器和手机实际接收仍需实物联调确认。
