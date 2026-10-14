# logs/ 日志索引

本目录 46 个文件，是项目从点亮屏幕到蓝牙联调的**完整验证证据链**。
按前缀分成 9 组，下表说明每一组属于哪次迭代。

命名规律：`-build` 编译输出 · `-upload` ST-Link 烧录输出 · `-serial` COM3 串口采集 ·
`-verification.json` / `verification.json` **结构化验收记录**（含 SHA-256、Flash/RAM 占用、实机结论）。

---

## A. 基础验证 — 点亮屏幕（2026-10-03）｜4 个

| 文件 | 说明 |
| --- | --- |
| `build.log` (41 KB) | 首次编译输出 |
| `upload.log` (2.2 KB) | ST-Link 烧录输出 |
| `serial.log` (351 B) | 串口输出 |
| `verification.json` (1.1 KB) | ✅ 验收记录：文字、色条、计时器正常，用户确认 |

## B. PMS5003ST 传感器接入（2026-10-03）｜5 个

| 文件 | 说明 |
| --- | --- |
| `pms-build.log` | 编译输出 |
| `pms-upload.log` | 烧录输出 |
| `pms-serial.log` | 40 秒串口采集 |
| `pms-parser-tests.log` | 6 项解析测试输出 |
| `pms-verification.json` | ✅ 42 帧连续真实数据，校验/长度/半帧超时错误均为 0；Flash 27712 B |

## C. 开机动画（2026-10-05）｜4 个

| 文件 | 说明 |
| --- | --- |
| `boot-build.log` | 编译输出 |
| `boot-upload.log` | 烧录输出 |
| `boot-serial.log` | 串口采集 |
| `boot-verification.json` | ✅ 动画 3465 ms / 31 帧；Flash 44668 B |

## D. 启动白屏优化（2026-10-05）｜4 个

| 文件 | 说明 |
| --- | --- |
| `startup-build.log` | 编译输出 |
| `startup-upload.log` | 烧录输出 |
| `startup-serial.log` | 串口采集 |
| `startup-verification.json` | ✅ uptime 559 ms 开显示；动画 3482 ms；用户确认不再明显闪白 |

## E. HC-06 蓝牙接入主线（2026-10-10）｜6 个

| 文件 | 说明 |
| --- | --- |
| `hc06-build.log` (91 KB) | USART2 + Blinker 编译 |
| `hc06-host-tests.log` | 本机测试输出 |
| `hc06-native-tests.log` | native 环境单元测试 |
| `hc06-pms-regression.log` | PMS 解析回归测试 |
| `hc06-upload.log` | 烧录输出 |
| `hc06-verification.json` | ✅ `nucleo_f411re` 烧录 Verified OK；含焊桥要求记录 |

## F. HC-06 · Blinker 专版（2026-10-10）｜7 个

| 文件 | 说明 |
| --- | --- |
| `hc06-blinker-build.log` (135 KB) | 编译输出 |
| `hc06-blinker-native-tests.log` (14 KB) | native 单元测试 |
| `hc06-blinker-upload.log` | 烧录输出 |
| `hc06-blinker-serial.log` / `.json` / `.bin` | 串口采集（原始 + 结构化 + 二进制） |
| `hc06-blinker-verification.json` | ✅ 结构化验收，含 Blinker 协议定义与组件键名 |

## G. HC-06 · `$ENV` 旧协议版（2026-10-10）｜4 个

| 文件 | 说明 |
| --- | --- |
| `hc06-env-build.log` | 编译输出 |
| `hc06-env-native-tests.log` | native 单元测试 |
| `hc06-env-upload.log` | 烧录输出 |
| `hc06-env-verification.json` | ✅ `$ENV,10,20,30,0.015,28.6,72.5#` 协议验收 |

## H. HC-06 AT 地址探针实验（2026-10-10）｜6 个

为查清模块实际型号/地址做的一次性探针工程（源码在 `../tmp/hc06-at-probe/`）。

`hc06-address-probe-build.log` · `-upload.log` · `-read.log` · `-restore.log` · `.json` · `.bin`

## I. Blinker / USART6 · 当前默认方案（2026-10-10）｜6 个

| 文件 | 说明 |
| --- | --- |
| `blinker-uart6-build-20261010.log` (134 KB) | 编译输出 |
| `blinker-uart6-final-build-20261010.log` | 最终编译输出 |
| `blinker-uart6-upload-20261010.log` | 烧录输出 |
| `blinker-uart6-serial-20261010.json` | 串口采集 |
| `blinker-temp-humi-check-20261010.json` | 组件键名 `temp`/`humi` 与只读采集检查 |
| `blinker-uart6-verification-20261010.json` | ✅ **当前默认固件**的验收记录：USART6 PA11/PA12、烧录 Verified OK、用户确认"有数据了" |

---

## 相关文档

- 工程总体技术说明：[../README.md](../README.md)
- Blinker 接入：[../docs/Blinker-integration.md](../docs/Blinker-integration.md)
- USART6 接线：[../docs/Blinker-uart6-wiring.md](../docs/Blinker-uart6-wiring.md)
- HC-06 完整说明：[../docs/HC06-integration.md](../docs/HC06-integration.md)
- 固件存档对照：[../firmware/MANIFEST.md](../firmware/MANIFEST.md)
