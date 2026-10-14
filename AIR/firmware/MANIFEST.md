# firmware/ 固件存档对照表

本目录保存每次里程碑的可烧录产物。**所有 SHA-256 已实际核算**，与 `logs/` 里的验证记录一一对应。

> 生成说明：本表由整理时逐个文件计算 SHA-256 后编写，映射依据是
> `firmware/SHA256SUMS-*.txt`、`logs/*-verification.json` 里记录的哈希，
> 以及 `.pio/build/*/firmware.bin` 的实际比对。

## 对照表

| 固件文件（.bin / .elf） | 大小 (B) | .bin SHA-256（前 24 位） | 对应 PlatformIO 环境 | 通信方式 | 验证记录 |
| --- | --- | --- | --- | --- | --- |
| `air-blinker-uart6-20261010` ⭐ | 48,600 / 95,028 | `46f820447f180d37c6e09040` | `nucleo_f411re_uart6` | Blinker JSON，USART6 PA11/PA12 | `logs/blinker-uart6-verification-20261010.json` |
| `air-hc06-blinker-20261010` | 47,552 / 94,088 | `207608b54bb156bf0d75676a` | `nucleo_f411re` | Blinker JSON，USART2 PA2/PA3 | `logs/hc06-blinker-verification.json` |
| `air-hc06-env-20261010` | 47,144 / 93,948 | `db4ac66be32e08aa9345157c` | `nucleo_f411re_env` | `$ENV,...#` 六字段，USART2 | `logs/hc06-env-verification.json` |
| `air-hc06-20261010` | 47,768 / 93,964 | `5d701b1024d58472428b9a81` | `nucleo_f411re` | Blinker JSON，USART2（同环境早期修订） | `logs/hc06-verification.json` |
| `air-debug-20261010` | 45,808 / 92,788 | `805b5cedbad901537548d1af` | `nucleo_f411re_debug` | 无蓝牙，ST-Link VCP 115200 调试 | `logs/hc06-verification.json` |
| `air-debug-env-20261010` | 45,808 / 92,788 | `805b5cedbad901537548d1af` | ⚠️ 见下 | — | — |

⭐ = **当前默认固件**，配套 USART6 接线（模块 TXD → PA12 / CN10-12，RXD → PA11 / CN10-14）。

## ⚠️ 一处重复

`air-debug-env-20261010.bin/.elf` 与 `air-debug-20261010.bin/.elf` **字节完全相同**
（SHA-256 均为 `805b5ced…` / `1b0224ef…`）。

也就是说 `-env` 那两个文件是多余的副本。整理时**予以保留、未删除**，你可以自行决定是否移除。

## 校验方法

```powershell
cd 02_固件工程\AIR\firmware
Get-ChildItem *.bin | Get-FileHash -Algorithm SHA256
```

两个 `SHA256SUMS-*.txt` 是项目原有的校验文件，内容未改动。
