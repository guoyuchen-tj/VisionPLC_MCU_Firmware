# VisionPLC MCU S2：综合、烧录与验收说明

> 日期：2026-10-13  
> 状态：代码接入完成；TD综合、布局布线和板上烧录由用户执行

## 1. 本次已经完成的修改

1. 将独立工程验证过的AHB邮箱→APB桥移入视频工程：
   - `user_source/hdl_source/mcu_subsystem/fpga_apb_bridge.v`
   - `user_source/hdl_source/mcu_subsystem/fpga_apb_regs.v`
2. 在`design_top_wrapper.v`中：
   - 接出MCU的AHB时钟、地址、数据和控制信号；
   - 增加AHB域复位同步；
   - 用`0x80000100`邮箱窗口连接FPGA内部20位APB空间；
   - 将APB测试IRQ同步到MCU `clic_irq[0]`。
3. 在`camera_to_dsi_display.al`中登记上述两份RTL源文件。
4. 将`MCU_FPGA_CONNECT/main.c`改为合并工程S2验收固件：
   - 保留IP ID、版本、字节写选通、非法地址、10000次SCRATCH和100次IRQ源测试；
   - 删除视频工程不存在的旧“AHB加法器/数码管2”回归；
   - 成功后打印`VISIONPLC MCU READY`并保持运行。

## 2. 用户综合步骤

1. 在TD中重新打开`td_project/camera_to_dsi_display.al`。
2. 在源文件列表确认出现：
   - `mcu_subsystem/fpga_apb_regs.v`
   - `mcu_subsystem/fpga_apb_bridge.v`
3. 运行综合与布局布线。
4. 检查没有未定义模块、端口宽度或多驱动错误。
5. 检查setup/hold报告；最终验收目标为setup和hold WNS都不小于0。
6. 生成并烧录新的`camera_to_dsi_display.bit`。

## 3. MCU固件步骤

1. 将项目中的`FPGA_MCU/application/MCU_FPGA_CONNECT/main.c`同步到FutureDynasty当前`FPGA-MCU`工程。
2. 在FD中执行Clean Project，再执行Build Project，防止继续下载旧ELF。
3. 确认新ELF生成时间晚于本次源码修改时间。
4. 保持UART0为115200、8N1、无流控。
5. 启动Debug，命中`main()`后打开串口，再点击Resume。

## 4. 正确串口结果

日志应依次包含：

```text
Hello World From Anlogic! BSP version:...
APB IP_ID: 0x56504c43 PASS
APB byte strobes PASS
APB PSLVERR read/write PASS, legal access recovered
APB SCRATCH 10000/10000, errors=0 PASS
APB IRQ source 100/100 PASS (polling, not CLIC ISR)
STAGE_B PASS
VISIONPLC MCU READY
```

不得再出现：

```text
FPGA addition timeout
MCU -> FPGA: ..., addition PASS
display 2
```

如果仍出现这些旧日志，说明FD下载的是旧ELF。

## 5. S2通过标准

- 视频、摄像头识别、HDMI和小屏功能与接入前一致。
- UART完整出现`STAGE_B PASS`和`VISIONPLC MCU READY`。
- 10000次SCRATCH读写为零错误。
- 非法地址测试后仍可继续访问合法寄存器。
- APB IRQ源100次置位/清除通过；本阶段只验证IRQ源，不宣称CLIC ISR已经完成。
- 连续运行10分钟无复位、无串口异常、视频不退化。
- 保存UART截图、视频画面和TD时序报告。

只有以上全部通过，才进入S3：MCU心跳、200ms硬件看门狗、安全ARM/FAULT寄存器和真实CLIC中断。
