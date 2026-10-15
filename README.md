# L-EVK-NRF54L15

Standalone Zephyr project for the custom L-EVK-NRF54L15 board. It does not
inherit Nordic DK board definitions.

- LED0: P0.02
- LED1: P0.03
- UART30 TX: P0.00
- UART30 RX: P0.01
- UART: 115200 8N1, no RTS/CTS

Build on the remote NCS host:

```sh
bash ./zephyr-remote-build.sh
```

Flash via the remote DAPLink probe:

```sh
bash ./zephyr-remote-flash-daplink.sh
```

Set `PYOCD_FREQUENCY=100000` when a lower SWD clock is needed.

The sysbuild image is generated at `build/L-EVK-NRF54L15/zephyr/zephyr.bin`.
