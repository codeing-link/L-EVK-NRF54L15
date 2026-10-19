# L-EVK-NRF54L15 使用文档

本工程使用原生 `l_evk_nrf54l15/nrf54l15/cpuapp` 自定义板级目标。LED、串口和传感器总线位于
应用仓库内的 `boards/lierda/l_evk_nrf54l15`，应用层 `app.overlay` 仅保留 MCUboot OTA 分区。

| 项目 | 配置 |
| --- | --- |
| LED0 / LED1 | P0.02 / P0.03，高电平点亮、同步每 1000 ms 翻转 |
| 串口 | UART30，TX=P0.01、RX=P0.00，115200 8N1，无流控 |
| IMU | LIS2DW12TR，TWIM20：SCL=P1.03、SDA=P1.06、100 kHz；三轴加速度串口日志 |
| PPG | GH3220，SPI21：SCK=P1.12、MOSI=P1.13、MISO=P1.10、CS=P1.11；仅底层初始化 |

## L-EVK 最小硬件配置

工程直接使用 Lierda 自定义板定义，不继承 nRF54L15 DK 的板载 UART、SPI NOR、LED、
按键、PWM、扩展排针或 IEEE 802.15.4 配置，因此不会探测未安装的 DK 外设，也不会占用
GH3220 使用的管脚。启用的产品外设是 UART30、两路 LED、TWIM20、SPI21 和 Bluetooth
LE Radio；MCUboot 与应用共用明确的内部 RRAM OTA 分区。

## 文档索引

- [传感器接口说明](SENSORS.md)：LIS2DW12TR 三轴日志、GH3220 底层接口、完整管脚定义与硬件验证。
- [构建与有线烧写](BUILD_FLASH.md)：首次全新构建、日常构建、版本号和 DAPLink 烧写。
- [Bluetooth LE OTA](BLE_OTA.md)：MCUmgr SMP 升级包、手机升级步骤、回滚和安全注意事项。

## 获取仓库与环境要求

应用仓库包含应用代码、构建脚本和自定义板文件，但**不包含** NCS、Zephyr、编译器、
`west` 或 Python 虚拟环境。请先完成 Nordic 官方 NCS 环境安装。本工程已在 NCS v3.4.1 验证。

为使 `build.sh` 自动定位 NCS 根目录，必须将仓库放在下面的精确路径：

```text
<ncs-workspace>/ncs/apps/L-EVK-NRF54L15
```

其中 `<ncs-workspace>/ncs` 必须是已初始化的 NCS 根目录，至少包含 `zephyr/`、`nrf/`
及其已安装的构建环境。克隆命令如下：

```bash
cd <ncs-workspace>/ncs
mkdir -p apps
git clone <GitHub-仓库地址> apps/L-EVK-NRF54L15
cd apps/L-EVK-NRF54L15
```

将 `<GitHub-仓库地址>` 替换为本工程的 GitHub Clone URL。若克隆到其他位置，
`build.sh` 将无法按相对路径发现 NCS 环境。

## 构建

在工程根目录执行：

```bash
./build.sh
```

默认目标为 `l_evk_nrf54l15/nrf54l15/cpuapp`。工程以 sysbuild 同时构建应用和
MCUboot；成功后常用产物为：

```text
build/mcuboot/zephyr/zephyr.hex               # MCUboot
build/L-EVK-NRF54L15/zephyr/zephyr.signed.hex # 首次有线烧写的应用
build/dfu_application.zip                      # 手机端 BLE OTA 包
build/L-EVK-NRF54L15/zephyr/zephyr.signed.bin # 单镜像 OTA 包
```

`zephyr-remote-build.sh` 和 `zephyr-remote-flash-daplink.sh` 是为项目维护者的 Ubuntu NCS
主机准备的辅助脚本，客户环境通常不需要使用它们。若确有远端构建需求，请先设置脚本
中的主机地址与远端工程路径，或使用 `UBUNTU_HOST`、`REMOTE_PROJECT` 覆盖默认值。

例如：

```bash
UBUNTU_HOST=<用户名>@<主机地址> \
REMOTE_PROJECT=<远端-ncs>/apps/L-EVK-NRF54L15 \
./zephyr-remote-build.sh
```

首次切换到当前硬件与 OTA 配置时，必须执行一次全新构建：

```bash
PRISTINE=always ./build.sh
```

后续日常更新可以直接执行 `./build.sh`。版本化构建与 OTA 发布规则见
[构建与有线烧写](BUILD_FLASH.md)。

## 首次烧写

必须先将 MCUboot 和已签名的初始应用映像写入设备：

```bash
PRISTINE=always ./build.sh && ./flash_daplink.sh
```

构建和 DAPLink 有线烧写均使用同一个默认 `build/` 目录；完整规则见
[构建与有线烧写](BUILD_FLASH.md)。

该脚本会先烧写 `mcuboot/zephyr/zephyr.hex`，再烧写
`L-EVK-NRF54L15/zephyr/zephyr.signed.hex`；两者缺一不可。

## 运行检查

串口以 115200、8N1 打开，预期日志包含：

```text
<inf> l_evk_blinky: L-EVK-NRF54L15 started
<inf> l_evk_imu: LIS2DW12TR ready: TWIM20 SCL=P1.03 SDA=P1.06, 100 kHz, ODR=25 Hz, +/-2 g
<inf> l_evk_smp_bt: BLE OTA advertising started: L-EVK-NRF54L15
```

此时 P0.02、P0.03 对应 LED 同步闪烁，串口每秒输出一次三轴加速度。
