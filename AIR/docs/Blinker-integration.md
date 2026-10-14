# Blinker 温湿度组件接入（2026-10-10）

此前 STM32 发送 `$ENV,10,20,30,0.015,28.6,72.5#`，这是原自定义手机 App 的协议。Blinker 数值组件需要 JSON 对象，用组件的数据键定位组件；`val` 为 JSON 数值，`uni` 为单位，整帧以实际 LF 换行（0x0A）结束。

当前默认环境为 `nucleo_f411re_uart6`，蓝牙改用 USART6：模块 RXD → PA11/CN10-14，TXD → PA12/CN10-12，无需改焊桥；ST-Link COM3 为 115200 调试日志。用户要求先断电改线并重新通电，再烧录。完整步骤见 [USART6 接线说明](Blinker-uart6-wiring.md)。旧 `nucleo_f411re` 环境仍保留原 USART2 接线与焊桥要求。

最新结果：用户完成改线后，USART6 固件烧录校验通过，用户反馈「有数据了」，确认手机接收路径已经可用。记录见 `logs/blinker-uart6-verification-20261010.json`。以下原 USART2 串口检查和模块型号分析保留为排查经过。

## 组件设置与发送格式

在点灯 Blinker 编辑界面，将温度、湿度两个数值组件的数据键分别设置为 `temp`、`humi`，大小写一致；显示标题可以自行设置。数值组件的小数显示设置应允许至少一位小数。

### 与教学截图的对应关系

教学截图中的 `BlinkerNumber Number_Temp("num-temp");` 包含两个不同的名称：`Number_Temp` 是程序中的对象变量名；括号里的 `"num-temp"` 是 APP 的组件键名。用户已选择沿用本项目的 `temp` / `humi`，所以对应的 Blinker 库写法是：

```cpp
// 教学对照示例；本项目实际在 src/air_bluetooth.cpp 中生成组件 JSON。
BlinkerNumber Number_Temp("temp");
BlinkerNumber Number_Humi("humi");

// 收到有效传感器数据后：
Number_Temp.unit("℃");
Number_Humi.unit("%");
Number_Temp.print(sample.reading.temperatureTenths / 10.0);
Number_Humi.print(sample.reading.humidityTenths / 10.0);
```

本项目直接发送与上述组件调用对应的 JSON，通过当前环境选定的串口传给蓝牙模块（默认 USART6，旧环境 USART2）。实际实现见 `formatBlinkerTelemetry()` 和 `AirBluetooth::poll()`，采集及调用入口在 `src/main.cpp`；上述示例用于理解教学内容。

手机 APP 操作步骤：

1. 打开设备页面，进入编辑模式，添加两个「数字组件」。
2. 分别打开组件设置，将「组件键名」设为下表中的值；显示标题与单位也按表设置。
3. 小数显示允许一位小数，保存后退出编辑模式，进入运行页面并连接设备。
4. 将 APP 的温湿度读数与监测屏对照，确认读数持续更新。若只有标题而无数值，先检查组件键名是否填写在正确字段中、大小写是否一致，再核对蓝牙连接与模块是否支持 Blinker 的接入方式。

| 数字组件 | 组件键名 | 显示标题 | 单位 |
| --- | --- | --- | --- |
| PM1.0 | `pm1` | PM1.0 | µg/m³ |
| PM2.5 | `pm25` | PM2.5 | µg/m³ |
| PM10 | `pm10` | PM10 | µg/m³ |
| 甲醛 | `hcho` | 甲醛 | mg/m³ |
| 温度 | `temp` | 温度 | ℃ |
| 湿度 | `humi` | 湿度 | % |

六项对应截图示例 `$ENV,10,20,30,0.015,28.6,72.5#` 的字段顺序。用户确认继续使用 Blinker，所以固件仍发送六项组件 JSON，APP 依次配置这六个数字组件的键名即可；无需改为 `$ENV` 或再次烧录。示例值仅解释字段含义，固件运行时使用传感器实时读数。PM 与甲醛的显示单位可在 APP 组件设置中填写，甲醛小数显示应允许三位。

### 蓝牙通道的验证范围

串口能收到组件 JSON，只能确认开发板输出正确。项目接线资料标注为 HC-06：经典 HC-06 使用蓝牙 SPP，而 Blinker 官方标准蓝牙接入使用 BLE，设备发现服务为 FFE0、通信服务为 FFE1。若实际模块是经典 HC-06，它不能直接满足这套 BLE 接入要求；仅修改组件键名或添加 BlinkerNumber 对象无法改变模块的蓝牙协议。模块若是兼容 BLE 的型号，则需要按其实际规格核对服务与串口设置。

依据：[Blinker 官方 BLE 接入文档](https://raw.githubusercontent.com/blinker-iot/blinker-doc/master/docs/001-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B/15-%E8%93%9D%E7%89%99BLE%E6%8E%A5%E5%85%A5.md)、[DSD TECH HC-06 产品说明及 Android SPP 工具](https://www.deshide.com/product-details_HC-06.html)。手机 APP 是否实际连接并显示数据，仍需通过设备页面核对。

2026-10-10 本次检查确认沿用 `temp` / `humi`，默认固件重新编译通过，并通过 COM3、9600 波特率只读采集到连续有效 JSON。源码及开发板程序无需为组件命名调整；本次未重新烧录。当前电脑的 native 单元测试在编译阶段因 PATH 中缺少 `gcc` / `g++` 而无法执行；既有测试记录保留。实际采集与检查结果见 `logs/blinker-temp-humi-check-20261010.json`，手机 APP 显示及实际模块的 BLE 兼容性尚未验证。

上述传感器示例在当前默认固件中发送为：

```json
{"pm1":{"val":10},"pm25":{"val":20},"pm10":{"val":30},"hcho":{"val":0.015},"temp":{"val":28.6,"uni":"\u2103"},"humi":{"val":72.5,"uni":"%"}}
```

JSON 后紧跟一个实际换行，不发送 `$ENV`、`#` 或 NUL。`\u2103` 是 JSON 对 ℃ 的编码，手机解析后显示 ℃。温度组件收到 28.6，湿度组件收到 72.5；这些是测试示例，固件运行时始终使用 PMS5003ST 的真实读数。

PM、甲醛四项仍包含在同一帧中，可以分别绑定 `pm1`、`pm25`、`pm10`、`hcho` 数值组件。PM 单位为 µg/m³，甲醛为 mg/m³。温湿度保留一位小数，甲醛保留三位小数；不引入浮点格式化依赖。

Blinker 发出 `{"get":"state"}` 加换行后，STM32 回复 `{"state":"connected"}` 加换行，并立即上报当前有效读数。尚未收到传感器有效帧、传感器报错或读数超过 10 秒未更新时，不发送虚假的零值。正常自动上报间隔仍为一秒。

## 保留的串口命令

命令仍支持 CR、LF 或 CRLF 结束，不区分大小写。默认构建中的回复全部使用 JSON 加换行，避免与 Blinker 的拆包规则冲突。

| 命令 | 当前回复/行为 |
| --- | --- |
| `PING` | `{"pong":true}` |
| `GET` | 当前组件 JSON；无有效读数则 `{"error":"WAITING"}`、`NO_DATA` 或 `SENSOR_ERROR` |
| `STREAM ON` | `{"stream":"on"}`，开启自动上报 |
| `STREAM OFF` | `{"stream":"off"}`，暂停自动上报；`GET` 和状态查询仍可读取 |
| `HELP` | `{"help":["PING","GET","STREAM ON","STREAM OFF","HELP"]}` |

屏幕、开机动画和传感器校验逻辑保留。蓝牙串口按环境选择：默认 USART6 为 9600，ST-Link USART2 日志为 115200；旧环境保持原接线与波特率。仍使用有界非阻塞发送队列，不安装完整 Blinker 库。未知 Blinker JSON 查询忽略，其他无效串口命令回复 JSON 错误。

## 构建与旧协议兼容

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native -e native_env
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_uart6
# 先完成改线并重新通电，再执行：
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re_uart6 -t upload
```

如需继续使用原自定义 App：构建/烧录 `nucleo_f411re_env`，它设置 `AIR_BLUETOOTH_BLINKER=0`，恢复原六字段 `$ENV,...#` 和原命令回复格式。当前默认为 `AIR_BLUETOOTH_BLINKER=1`。修改前源码与正式固件已保存到 `backups/before-blinker-20261010/`，历史 ENV 固件仍在 `firmware/air-hc06-env-20261010.*`。

此次测试包含题目中的完整示例、真实 PMS 帧到输出的映射、负温度、零值、超时、缓冲不足、队列短写和状态查询。构建、烧录及本机串口验证结果见 `logs/hc06-blinker-verification.json`。手机界面最终显示需要在重新连接后确认，串口输出验证不能单独证明蓝牙数据已到达手机。

协议依据：[官方 Number 组件实现（val/uni）](https://github.com/blinker-iot/blinker-library/blob/master/src/Functions/BlinkerNumber.h)、[官方 BLE 传输（换行结束）](https://github.com/blinker-iot/blinker-library/blob/master/src/Adapters/BlinkerBLE.h)、[官方状态查询实现](https://github.com/blinker-iot/blinker-library/blob/master/src/Blinker/BlinkerApi.h)。
