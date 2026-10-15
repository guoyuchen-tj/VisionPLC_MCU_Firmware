# FPGA / MCU 兼容性

| 项目 | 当前值 |
|---|---|
| FPGA 仓库 | `YuxiangJia111/FPGA_ANLU_National_First_Prize` |
| FPGA 分支 | `feature/visionplc-s3-fpga` |
| FPGA 基准提交 | `85888f8` |
| APB IP ID | `0x56504C43` (`VPLC`) |
| APB 寄存器版本 | `0x00020000` |
| MCU AHB 邮箱基址 | `0x80000100` |
| UART | 115200, 8N1 |
| MCU IRQ | CLIC source 27（仍需以最终板级 CLIC 验收结果为准） |

MCU 启动时必须先校验 IP ID、寄存器版本和 APB 读写结果。任一检查失败时不得进入机械执行状态。

更新 APB 地址、位定义或中断语义时，必须同步修改两个仓库，并更新此兼容性表。
