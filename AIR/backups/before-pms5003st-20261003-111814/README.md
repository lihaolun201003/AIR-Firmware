# AIR 屏幕点亮程序

硬件：NUCLEO-F411RE + 2.8 英寸 ILI9341 SPI TFT。基于现有 PlatformIO / Arduino 工程。

## 接线

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

触摸和 SD 卡不参与本程序。RESET 指屏幕的 RESET 引脚，接开发板 D7。板载用户 LED 与 PA5/SCK 共用，程序不将它设为普通 GPIO 或闪灯。

屏幕模块 VCC 支持 3.3–5V，但信号逻辑为 3.3V。用户提供的屏幕 PDF 第 4 页 DC 高低电平文字与 ILI9341 芯片手册不一致；本程序由驱动库按芯片协议处理：低电平命令，高电平参数/像素。

## 预期画面

复位后依次全屏红、绿、蓝、白、黑，各保持 400 ms。随后横屏显示 `AIR LCD TEST`、板卡和屏幕型号、六色彩条及引脚说明；底部 `RUN` 每秒递增，右下角圆点明暗交替。按开发板黑色 RESET 按钮可以重播。

ST-Link 虚拟串口以 115200、8N1 输出启动信息和每秒心跳。MISO 未接，程序不会读取或自动确认 LCD 响应；串口心跳证明 MCU 正在运行，画面仍以实际观察为准。

## 编译、烧录

在当前目录使用 PlatformIO 的 Build / Upload，或 PowerShell：

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e nucleo_f411re -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -b 115200
```

生成文件：`.pio/build/nucleo_f411re/firmware.bin` 和 `firmware.elf`。
修改前的源码、配置和已有编译产物保存在 `backups/before-lcd-*`。
其中 `device-flash.bin` 是烧录前从板上读取的完整 512 KiB Flash 备份。

当前 STM32duino 3.0 已移除旧版 `wiring_private.h`，而 Adafruit ILI9341 1.6.4 仍包含该头文件，但不使用其中的私有接口。项目中的 `include/wiring_private.h` 提供兼容头，不修改下载的库。

`scripts/windows_paths.py` 将 Windows 下链接器的 map 输出改为工程相对路径，避免 GNU Arm 工具链对中文绝对路径报 `Illegal byte sequence`。工程可以保留在现有中文目录。

驱动依赖：[Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341) 1.6.4，使用完整初始化和 RGB565 像素格式。SPI 请求频率 8 MHz，实际时钟由 STM32 分频器选择不高于该值的频率。若跳线较长导致花屏，可将 `src/main.cpp` 中 `TFT_SPI_HZ` 调低到 `4000000`。

若背光亮但无图像，断电后检查 CS、DC、RESET、SCK、MOSI 与上表一致，以及共地；若连背光也不亮，先检查 VCC、GND 和 LED 接线。

## 本次验证结果

2026-10-03：编译通过；ST-Link 烧录成功并返回 `Verified OK`；COM3 收到完整启动信息及连续秒级心跳；用户已确认文字、彩条和计时均正常。

构建占用：Flash 23552 字节，静态 RAM 1484 字节。`logs/` 保存编译、烧录、串口记录和固件 SHA-256 校验值。
