# VisionPLC S3：中断、心跳与硬件安全验收

> 状态：2026-10-15板上验收通过；CLIC 5/5，心跳50 ms，看门狗实测197 ms

## 本阶段目标

在不连接电机和电磁铁的条件下，验证以下链路：

```text
FPGA事件 → clic_irq[0] → MCU CLIC ISR → 主循环W1C清除
MCU每50ms更新心跳 → FPGA独立200ms监控 → 超时锁存并关闭safe_enable
```

`safe_enable`只作为后续执行器安全使能，不再连接屏幕背光。板测发现用安全信号开关`O_screen_pwm`会导致屏幕重新同步、闪烁和分割画面，因此背光固定保持开启；安全状态通过APB、UART和后续专用LED/执行器使能观察。

## 已实现内容

- APB S3寄存器表：ID、版本、能力、构建号、IRQ状态/使能、系统状态、故障、心跳、控制、运行时间、调试IRQ和SCRATCH。
- 5个IRQ位：VISION、MOTION_DONE、FAULT、SCREEN_DONE、TEST。
- `IRQ_STATUS`写1清除，IRQ输出为电平触发。
- MCU ISR只置事件标志并暂时屏蔽该CLIC源；APB读取和W1C在主循环完成，避免邮箱重入。
- MCU每50ms写入变化的32位心跳值。
- FPGA在50MHz域独立计数10,000,000周期，即200ms。
- 超时后锁存`MCU_TIMEOUT`、撤销ARM并关闭`safe_enable`。
- 故障后直接ARM无效，必须依次执行CLEAR、写入新心跳、ARM。
- 上电仍先执行S2的一万次APB回归，再执行S3测试。

## 文件

- FPGA寄存器：`user_source/hdl_source/mcu_subsystem/fpga_apb_regs.v`
- AHB/APB桥：`user_source/hdl_source/mcu_subsystem/fpga_apb_bridge.v`
- 视频工程顶层：`user_source/hdl_source/design_top_wrapper.v`
- MCU固件：`FPGA_MCU/application/MCU_FPGA_CONNECT/main.c`
- 自检testbench：`FPGA_MCU/MCU_FPGA/tests/stage_s3_tb.v`

## 寄存器偏移

| 偏移 | 名称 | 访问 | 关键语义 |
|---:|---|---|---|
| `0x000` | `IP_ID` | R | `0x56504C43` |
| `0x004` | `IP_VERSION` | R | `0x00020000` |
| `0x010` | `IRQ_STATUS` | R/W1C | 位0~4为五类事件 |
| `0x014` | `IRQ_ENABLE` | R/W | 事件使能 |
| `0x018` | `SYS_STATUS` | R | bit0 ARM，bit1 safe，bit2见过心跳，bit3 IRQ |
| `0x01C` | `FAULT_STATUS` | R | bit0 MCU_TIMEOUT |
| `0x020` | `HEARTBEAT` | R/W | 只有数值变化才刷新看门狗 |
| `0x024` | `SYS_CONTROL` | R/W | bit0 ARM，bit1 CLEAR，bit2 DISARM |
| `0x028` | `PL_UPTIME` | R | FPGA毫秒计数 |
| `0x02C` | `IRQ_TEST` | W | 写入位掩码注入测试事件 |
| `0x030` | `SCRATCH` | R/W | APB回归测试 |

## 上板步骤

1. 在TD中重新综合、布局布线并生成包含S3 RTL的新bit文件。
2. 下载新bit，确认视频通路仍正常。启动阶段背光会暂时关闭，这是安全默认态。
3. 将最新`main.c`替换进FutureDynasty应用工程并Build。
4. 串口保持`115200-8-N-1`，先打开串口，再Debug并在`main()`断点处按F8。
5. 等待S2和S3自检完成。S3会主动制造一次约200ms的心跳超时，背光短暂熄灭后通过显式恢复重新亮起。

## 预期串口关键日志

```text
APB IP_ID: 0x56504c43 PASS
APB SCRATCH 10000/10000, errors=0 PASS
STAGE_B PASS
S3 CLIC IRQ 5/5 PASS, CLIC_ID=27
S3 heartbeat 50ms keepalive PASS
S3 MCU_TIMEOUT 200ms PASS, safe output OFF
S3 explicit CLEAR + HEARTBEAT + ARM recovery PASS
STAGE_S3 PASS
VISIONPLC MCU READY
```

## 人工失效注入

在出现`VISIONPLC MCU READY`后：

1. 用FD暂停MCU，保持暂停至少300ms。
2. 观察屏幕背光应在约200ms后由FPGA自动熄灭。
3. 恢复MCU，串口应输出`S3 FAULT LATCHED`；持续心跳不能自动重新使能背光。
4. 重新Debug/Reset，启动自检执行CLEAR、新心跳和ARM后背光恢复。

## 判定准则

只有以下条件全部满足，S3才算通过：

- UART同时出现`STAGE_B PASS`、`STAGE_S3 PASS`和`VISIONPLC MCU READY`。
- 五种测试事件共进入五次ISR，没有中断风暴。
- 正常50ms心跳期间背光持续开启。
- MCU暂停后背光在`200ms ± 20ms`内关闭。
- 恢复CPU执行不会自动清故障或自动ARM。
- Reset期间背光保持关闭；重新完成显式恢复序列后才开启。
- 视频识别与显示功能没有退化。

若日志停在`STAGE_B PASS`之后并打印`STAGE_S3 FAIL`，优先保留完整UART日志；首个待核验项是本芯片生成配置中`clic_irq[0]`是否对应SDK中断号27。
