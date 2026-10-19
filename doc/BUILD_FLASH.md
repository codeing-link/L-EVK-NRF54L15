# 构建与有线烧写

本工程使用 MCUboot sysbuild。一次构建会同时生成 MCUboot 和可有线烧写的签名应用。
为避免烧错镜像，项目统一只使用工程根目录下的 `build/`。

## 首次切换到当前版本

从旧配置、非 sysbuild 工程或旧分区布局首次切换到当前版本时，必须清理缓存并完整烧写：

```bash
cd ncs/apps/L-EVK-NRF54L15
PRISTINE=always ./build.sh && ./flash_daplink.sh
```

这会重新生成并烧写同一套 MCUboot 和签名应用，避免旧 Kconfig、DTS 或分区缓存残留。

## 日常更新

每次代码更新后执行：

```bash
./build.sh && ./flash_daplink.sh
```

`build.sh` 会更新 `build/`，`flash_daplink.sh` 也只烧写 `build/` 中的 MCUboot 与已签名
应用，因此两个镜像始终来自同一次构建，不会混淆目录。

## 发布 OTA 新版本

每次 OTA 发布都应递增 MCUboot 镜像版本。以下命令全新构建版本 `1.0.1+0`，仍然更新
工程默认的 `build/`：

```bash
PRISTINE=always ./build.sh -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.1+0\"
```

用于 nRF Device Manager BLE OTA 上传的文件为：

```text
build/dfu_application.zip
```

命令行 MCUmgr 或单镜像客户端可使用：

```text
build/L-EVK-NRF54L15/zephyr/zephyr.signed.bin
```

若还要把该发布版本通过有线方式完整安装，再执行 `./flash_daplink.sh`。有线脚本会打印
两个实际烧写的 `build/` 镜像路径。

只有需要单独归档发布产物时才建议使用独立目录，例如：

```bash
BUILD_DIR="$PWD/build-release-v1.0.1" PRISTINE=always ./build.sh -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.1+0\"
```

`flash_daplink.sh` 为避免误烧，固定只读取默认 `build/`；独立发布目录主要用于保存和
分发 OTA 包。

## 找不到开发板

若 DAPLink/PyOCD 找不到目标，构建没有问题，但烧写主机没有检测到调试器。请在执行
`flash_daplink.sh` 的同一台主机上检查：

```bash
pyocd list
```

若存在多个调试器，请断开无关设备，或按 PyOCD 的序列号选择目标。也可将目标型号和频率
显式传给脚本：

```bash
PYOCD_TARGET=nrf54l PYOCD_FREQUENCY=1000000 ./flash_daplink.sh
```

检查 USB 数据线和目标供电。使用外置 J-Link 时，确认 SWDIO、SWCLK、GND、VTref 均已连接。

## 产物用途

常用产物如下：

| 文件 | 用途 |
| --- | --- |
| `build/mcuboot/zephyr/zephyr.hex` | MCUboot 引导程序 |
| `build/L-EVK-NRF54L15/zephyr/zephyr.signed.hex` | 有线烧写应用镜像 |
| `build/dfu_application.zip` | nRF Device Manager 推荐的 BLE OTA 包 |
| `build/L-EVK-NRF54L15/zephyr/zephyr.signed.bin` | 命令行/单镜像客户端 OTA 包 |
