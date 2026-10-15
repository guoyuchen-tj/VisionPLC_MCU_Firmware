# VisionPLC MCU Firmware

本仓库保存 VisionPLC 在安路 PH1P35 内置 RISC-V MCU 上运行的固件、FutureDynasty 工程、BSP/SDK、链接脚本，以及独立 MCU/APB 验证工程。

## 目录

- `application/MCU_FPGA_CONNECT/`：可在 FutureDynasty 中导入的 MCU 应用，入口为 `main.c`。
- `platform/MCU_FPGA/`：PH1P35 RPU 平台、BSP、驱动与链接环境。
- `MCU_FPGA/`：最小 MCU 硬件参考工程和 APB 验证源码；生成的 Runs、bit 和日志不纳入版本控制。
- `docs/`：工具链、烧录和 S2/S3 验收说明。

## 当前能力

- UART 115200 8N1 启动日志。
- FPGA APB ID、版本、PSLVERR、SCRATCH 和 IRQ 验证。
- S3 中断心跳、安全使能、超时停机与显式故障恢复。
- 与 FPGA 视频主工程分仓管理，通过寄存器映射版本绑定。

## FutureDynasty 导入

1. 先导入 `platform/MCU_FPGA` 平台工程。
2. 如果 FD 提示原始绝对路径不存在，在 BSP Viewer 中把 HPF 重新指向当前仓库的 `MCU_FPGA/hx1p35_mcu_top.hpf`，然后重新生成 BSP Sources。
3. 导入 `application/MCU_FPGA_CONNECT`。
4. 将应用的 Referenced BSP 指向刚导入的 `MCU_FPGA`。
5. Clean/Build 后，通过 OpenOCD 下载 ELF，并用 115200 8N1 查看串口。

`.fd/project.json` 中保留了已经上板验证过的原始工程参数；换电脑后只需要在 FD 中重新绑定工作区路径。

## 兼容性

本仓库当前固件与 FPGA 仓库 `YuxiangJia111/FPGA_ANLU_National_First_Prize` 的 `feature/visionplc-s3-fpga` 分支配套。详细版本约束见 `COMPATIBILITY.md`。
