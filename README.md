# L-EVK-NRF54L15

L-EVK-NRF54L15 是基于 Nordic nRF54L15 的 Zephyr/NCS 应用工程。应用仓库同时保存
自定义 board、LED、UART、LIS2DW12TR、GH3220、MCUboot 和 Bluetooth LE OTA 配置，
不需要修改 NCS 自带的 `zephyr/boards`。

本工程已在 NCS v3.4.1 环境完成 sysbuild 编译验证。

## 硬件配置

| 功能 | 外设与引脚 | 配置 |
| --- | --- | --- |
| LED0 | P0.02 | 高电平点亮 |
| LED1 | P0.03 | 高电平点亮 |
| 调试串口 | UART30：TX=P0.01、RX=P0.00 | 115200 8N1，无流控 |
| LIS2DW12TR | I2C20：SCL=P1.03、SDA=P1.06 | 地址 `0x19`，100 kHz |
| IMU INT | P1.04 | 高有效 |
| GH3220 | SPI21：SCK=P1.12、MOSI=P1.13、MISO=P1.10 | 板级 SPI 总线 |
| PPG CS | P1.11 | 低有效 |
| PPG RESET | P1.09 | 低有效 |
| PPG INT | P1.14 | 高有效 |

P1.03 同时是 NFC2 管脚，因此 board 会禁用 NFCT，将该管脚交给 I2C20 使用。

## 仓库与 board 结构

```text
L-EVK-NRF54L15/
├── boards/
│   └── lierda/
│       └── l_evk_nrf54l15/
│           ├── board.yml
│           ├── board.cmake
│           ├── Kconfig.defconfig
│           ├── Kconfig.l_evk_nrf54l15
│           ├── l_evk_nrf54l15-pinctrl.dtsi
│           ├── l_evk_nrf54l15_common.dtsi
│           ├── l_evk_nrf54l15_nrf54l15_cpuapp.dts
│           ├── l_evk_nrf54l15_nrf54l15_cpuapp.yaml
│           └── l_evk_nrf54l15_nrf54l15_cpuapp_defconfig
├── dts/
├── src/
├── sysbuild/
├── CMakeLists.txt
├── build.sh
└── prj.conf
```

Zephyr 板目标为：

```text
l_evk_nrf54l15/nrf54l15/cpuapp
```

文件夹和目标名使用 Zephyr 规范要求的小写下划线形式，`board.yml` 中的显示名称仍为
`L-EVK-NRF54L15`。`CMakeLists.txt` 和 `build.sh` 都将本仓库加入 `BOARD_ROOT`，因此
应用、sysbuild 和 MCUboot 都能找到仓库内的 board。

## 首次获取工程

本仓库不包含 NCS、Zephyr、工具链或 Python 虚拟环境。先安装并初始化 NCS，然后将
应用克隆到 NCS 的 `apps` 目录：

```bash
cd <ncs-workspace>/ncs
mkdir -p apps
git clone <仓库地址> apps/L-EVK-NRF54L15
cd apps/L-EVK-NRF54L15
PRISTINE=always ./build.sh
```

不要再把 board 复制到 `ncs/zephyr/boards`。board 已经随应用仓库一起下载、版本化和
构建，这样不会污染 Nordic/Zephyr 的上游仓库，也不会在 NCS 更新时丢失。

## 更新仓库代码

更新前先检查本地修改：

```bash
cd <ncs-workspace>/ncs/apps/L-EVK-NRF54L15
git status
git pull --ff-only
```

如果更新包含 `boards/`、`app.overlay`、`prj.conf`、`sysbuild/` 或分区修改，执行：

```bash
PRISTINE=always ./build.sh
```

如果仅修改普通 `.c`/`.h` 应用源码，可直接增量构建：

```bash
./build.sh
```

`git pull` 更新本应用及其 board；`west update` 更新 NCS manifest 中管理的 SDK 模块，
两者用途不同。只有切换 NCS 版本或 manifest 发生变化时才需要在 NCS 工作区执行
`west update`。

## 修改和制作 board

硬件定义应放在 `boards/lierda/l_evk_nrf54l15`，不要放回应用 `app.overlay`：

- `l_evk_nrf54l15-pinctrl.dtsi`：UART、I2C、SPI 的引脚复用和休眠状态。
- `l_evk_nrf54l15_common.dtsi`：LED、传感器 GPIO、外设状态、时钟和供电配置。
- `l_evk_nrf54l15_nrf54l15_cpuapp.dts`：SoC 入口、模型、compatible 和内存选择。
- `board.yml`：板名、厂商、SoC 和烧写 runner 信息。
- `*_defconfig`、`Kconfig.*`：板级默认 Kconfig 和 SoC 选择。
- `board.cmake`：J-Link、nrfutil 等烧写 runner 配置。

制作新版本 board 时建议复制整个 board 目录后同时修改目录名、`board.yml`、Kconfig
符号、DTS/defconfig 文件名前缀以及 Devicetree compatible。修改任何板级文件后必须使用
`PRISTINE=always` 重新生成 Devicetree 和 Kconfig。

board 与应用必须在同一个提交或同一个发布标签中更新。例如改变硬件引脚时，应在一次
提交中同时更新 board、驱动代码和本文档，避免应用版本与硬件定义不匹配。

## 构建产物

```text
build/mcuboot/zephyr/zephyr.hex
build/L-EVK-NRF54L15/zephyr/zephyr.hex
build/L-EVK-NRF54L15/zephyr/zephyr.signed.hex
build/L-EVK-NRF54L15/zephyr/zephyr.signed.bin
build/dfu_application.zip
```

首次有线烧写必须同时写入 MCUboot 和已签名应用。具体步骤见：

- [完整构建与运行说明](doc/README.md)
- [构建和烧写](doc/BUILD_FLASH.md)
- [Bluetooth LE OTA](doc/BLE_OTA.md)
- [传感器接口](doc/SENSORS.md)

## 发布注意事项

MCUboot 默认签名密钥只能用于开发调试。正式发布前必须替换为受控的产品私钥，并妥善
保存私钥、升级包版本和对应的发布标签。
