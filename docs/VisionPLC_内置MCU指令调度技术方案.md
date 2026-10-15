# VisionPLC：基于片内 MCU 的视觉指令调度与机械控制方案

> 项目平台：安路 PH1P35 / HX1P35A 开发板  
> 方案定位：单芯片异构视觉 PLC  
> 文档用途：团队方案同步、软硬件分工与后续实现基线  
> 更新日期：2026-10-10

---

## 1. 项目目标

在现有 FPGA 多目标视觉识别工程基础上，引入芯片内部的 RISC-V MCU 作为指令调度核心，构成完整的：

```text
视觉感知 → 任务理解 → 指令调度 → 实时运动 → 状态反馈
```

最终系统不局限于某一种识别题型，而是作为可编程的 VisionPLC 平台：摄像头识别多个目标及其位置，片内 MCU 根据场景规则生成动作序列，FPGA 运动控制逻辑驱动真实机械机构完成分拣、搬运、演奏、排序或其他任务。

项目答辩核心表述：

> 在一颗 PH1P35 芯片内，FPGA 逻辑完成并行实时视觉与硬实时运动控制，片内 RISC-V MCU 完成目标理解、任务编译和流程调度，形成视觉、决策、控制一体化的单芯片异构 VisionPLC。

---

## 2. 当前工程基础

### 2.1 已实现能力

- SC500 摄像头输入。
- ISP、DDR 图像缓存和 HDMI 显示。
- 最多 8 个目标的框选与坐标输出。
- 每个目标裁剪、28×28 下采样和 CNN 分类。
- 输出 8 路目标有效位、类别和目标框坐标。
- HDMI 可显示检测框、类别及调试信息。
- FPGA 已能与一个非触摸小屏通信，小屏可作为只读状态终端。
- 板载按键和拨码开关可作为确认、停止和模式选择输入。

主要视觉输出位于：

- `user_source/hdl_source/design_top_wrapper.v`
- `cnn_digit_valid[7:0]`
- `cnn_digits[31:0]`
- `cnn_bbox_x_min/x_max/y_min/y_max`

### 2.2 片内 MCU 现状

仓库内已有：

- `user_source/ip_source/MCU/MCU.v`
- `user_source/ip_source/MCU/MCU_f9f874759a41.v`
- `user_source/hdl_source/ps_wrapper.v`

当前 MCU IP 配置包括：

- RISC-V MCU 硬核。
- 300 MHz Core 配置。
- 100 MHz Timer 配置。
- 150 MHz AHB 配置。
- APB、AHB、JTAG、QSPI、UART0/1、8 路 MCU GPIO。
- UART0/1 波特率配置为 115200。
- 复位向量为 `0x20E00000`。
- 当前 I²C 未启用。

但是，当前顶层 `design_top_wrapper.v` 尚未实例化 MCU，TD 工程文件也没有正式纳入 MCU 相关源文件，因此目前只是“MCU IP 文件存在”，不是“MCU 已经运行”。

### 2.3 已知问题

1. `ps_wrapper.v` 将 `clic_irq[22:0]` 全部绑为 0，MCU暂时无法接收PL事件中断。
2. `ps_wrapper.v` 中 GPIO3、GPIO4 的三态使能引用信号可疑，不能原样用于机械输出。
3. 当前主 PLL 没有导出 300 MHz MCU Core 时钟。
4. 当前资料包未发现完整的 MCU 固件工程、SDK、启动文件和链接脚本。
5. 板载 UART TX 已用于 CNN 样本输出，MCU 调试串口需要独立引脚或增加模式复用。
6. 现有设计 ERAM、DSP和时序压力较大，新增功能应以寄存器、计数器和轻量状态机为主。

---

## 3. 总体架构

```text
                    ┌─────────────────────────┐
摄像头 ───────────►│ FPGA视觉流水线          │
                    │ ISP/框选/裁剪/CNN       │
                    └────────────┬────────────┘
                                 │ 8目标类别和坐标
                                 ▼
                    ┌─────────────────────────┐
                    │ vision_snapshot_cdc     │
                    │ 原子快照与跨时钟域握手  │
                    └────────────┬────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │ APB VisionPLC寄存器组   │
                    └───────┬─────────┬───────┘
                            │         │
                    APB     │         │ 状态邮箱
                            ▼         ▼
              ┌─────────────────┐  ┌───────────────┐
              │ 片内RISC-V MCU │  │ FPGA小屏驱动  │
              │ 稳定判定        │  │ 只读状态显示  │
              │ 坐标标定        │  └───────────────┘
              │ 路径规划        │
              │ 指令调度        │
              └────────┬────────┘
                       │ APB运动命令
                       ▼
              ┌────────────────────────┐
              │ FPGA实时运动控制器     │
              │ 加减速/STEP/PWM/限位   │
              └──────────┬─────────────┘
                         ▼
              电机驱动器/机械机构/传感器
```

### 3.1 职责划分

| 模块 | 主要职责 | 禁止承担的职责 |
|---|---|---|
| FPGA视觉逻辑 | 图像采集、目标检测、CNN分类、坐标输出 | 复杂任务规划 |
| 片内MCU | 目标稳定判定、坐标变换、目标排序、指令生成、流程管理、故障恢复 | 直接软件翻转STEP脉冲 |
| FPGA运动逻辑 | STEP/DIR、PWM、加减速、位置计数、限位处理 | 高层任务语义理解 |
| 安全逻辑 | 急停、心跳超时、最大导通时间、故障锁存 | 依赖MCU正常运行 |
| 小屏 | 当前任务、步骤、模式、故障和提示信息 | 触摸交互 |

核心原则：MCU 决定“做什么”，FPGA 决定“精确地怎么执行”，硬件安全链路决定“何时必须停机”。

---

## 4. 时钟与复位设计

### 4.1 时钟规划

建议使用 TD 的 PLL IP 生成器，在不改变现有视频时钟的前提下启用一个空闲输出：

| 时钟 | 建议频率 | 用途 |
|---|---:|---|
| `clk_mcu_core` | 300 MHz | MCU Core |
| `clk_100m` | 100 MHz | APB、Timer、运动控制、安全逻辑 |
| `clk_24m` | 约24 MHz | 摄像头控制及CNN相关现有逻辑 |
| `hdmi_pixel_clk` | 约74.25 MHz | 视频与视觉结果产生 |
| `hdmi_serial_clk` | 约371.25/375 MHz | HDMI串行发送 |

不要手工修改 PLL 生成文件，也不要占用 HDMI 正在使用的高速串行时钟。重新生成后必须重新检查全工程时序。

### 4.2 复位时序

1. 外部复位有效时，所有驱动输出处于禁用状态。
2. 等待 PLL Lock。
3. 各时钟域使用独立复位同步器，同步释放复位。
4. APB与运动控制逻辑先释放。
5. 延迟至少 1024 个 `clk_100m` 周期后释放 MCU Core/POR复位。
6. MCU完成自检和APB ID检查前，不允许使能电机。

复位默认值：

```text
STEP       = 0
DRIVER_EN  = 禁用
TOOL_OUT   = 0
CMD_VALID  = 0
FAULT      = 锁存或待检查
```

---

## 5. RTL模块规划

建议新增目录：

```text
user_source/hdl_source/mcu_subsystem/
├── mcu_subsystem.v
├── ps_wrapper_fixed.v
├── apb_visionplc_regs.v
├── vision_snapshot_cdc.v
├── motion_engine.v
├── stepper_axis.v
├── safety_guard.v
├── irq_controller.v
└── screen_mailbox.v
```

### 5.1 `mcu_subsystem.v`

负责：

- 实例化 `ps_wrapper_fixed`。
- 连接MCU Core、Timer、APB时钟和复位。
- APB地址译码。
- APB `PREADY/PSLVERR/PRDATA` 返回。
- CLIC中断汇总。
- MCU UART调试输出。

### 5.2 `ps_wrapper_fixed.v`

基于当前 `ps_wrapper.v` 修改：

```verilog
input wire [22:0] I_clic_irq;
input wire        I_nmi;
```

将当前固定连接：

```verilog
.nmi      (1'b0),
.clic_irq (23'd0)
```

改为实际事件输入。MCU GPIO暂时只用于点灯或早期调试，不用于电机和安全控制。

推荐中断分配：

| CLIC中断 | 事件 |
|---:|---|
| 0 | 新视觉快照 |
| 1 | 运动命令完成 |
| 2 | 运动/限位故障 |
| 3 | 小屏邮箱完成 |
| 4 | 工具动作完成 |

### 5.3 `vision_snapshot_cdc.v`

视觉结果来自视频时钟域，APB工作在100 MHz域，禁止直接跨域读取多位总线。

采用“冻结总线+请求/应答翻转”的多周期路径握手：

1. 一个完整识别批次完成后，视觉域把8个目标结果复制到源域影子寄存器。
2. 翻转 `vision_req_toggle`。
3. APB域通过两级同步器检测翻转。
4. 影子总线已稳定多个周期，APB域一次性复制完整快照。
5. APB域递增 `VIS_SEQ` 并产生 `IRQ_VISION`。
6. APB域翻转 `vision_ack_toggle`。
7. 源域收到ACK前禁止覆盖影子寄存器。

这样可避免类别与坐标来自不同帧的数据撕裂。

### 5.4 `motion_engine.v`

第一版采用“单条在途命令邮箱”，支持：

```text
NOP
HOME
MOVE_ABS
MOVE_REL
SET_TOOL
STOP_SOFT
STOP_EMERGENCY
CLEAR_FAULT
```

内部包含：

- X/Y双轴位置计数。
- STEP/DIR生成。
- 梯形加减速。
- 最高步频、最大加速度和最小脉宽限制。
- 单轴和双轴运动完成判断。
- 回零状态机。
- 软限位和硬限位。
- 工具PWM/开关输出。
- 超时与故障锁存。

MCU先写完全部参数，最后写提交寄存器。PL检测命令序号变化后，一次性锁存参数，防止执行到只写了一半的命令。

---

## 6. APB寄存器协议

以下均为APB从设备内部偏移。MCU侧绝对基地址必须根据厂商SDK和芯片内存映射确认后定义。

### 6.1 系统与中断区

| 偏移 | 名称 | 访问 | 说明 |
|---:|---|---|---|
| `0x000` | `IP_ID` | R | 固定为 `0x56504C43`，ASCII为VPLC |
| `0x004` | `IP_VERSION` | R | 主版本/次版本/修订号 |
| `0x008` | `CAPABILITY` | R | 轴数、最大目标数和工具能力 |
| `0x00C` | `BUILD_ID` | R | FPGA构建编号 |
| `0x010` | `IRQ_STATUS` | R/W1C | 中断状态，写1清除 |
| `0x014` | `IRQ_ENABLE` | R/W | 中断使能 |
| `0x018` | `SYS_STATUS` | R | 视觉、运动、屏幕、MCU状态 |
| `0x01C` | `FAULT_STATUS` | R | 系统故障汇总 |
| `0x020` | `HEARTBEAT` | W | MCU周期写入递增值 |
| `0x024` | `SYS_CONTROL` | R/W | ARM、停止、清故障 |
| `0x028` | `PL_UPTIME` | R | PL运行时间计数 |
| `0x030` | `SCRATCH` | R/W | MCU/APB联调寄存器 |

### 6.2 视觉快照区

| 偏移 | 名称 | 说明 |
|---:|---|---|
| `0x100` | `VIS_SEQ` | 每次原子快照递增 |
| `0x104` | `VIS_TIMESTAMP` | 快照时间戳 |
| `0x108` | `VIS_COUNT` | 有效目标数量 |
| `0x10C` | `VIS_FLAGS` | 批次完成、溢出、错误等 |
| `0x110～0x18F` | `TARGET[0..7]` | 每个目标16字节 |

目标槽位格式：

| 槽内偏移 | 内容 |
|---:|---|
| `+0x0` | `VALID / CLASS / SOURCE_SLOT / FLAGS` |
| `+0x4` | `X_MIN / X_MAX` |
| `+0x8` | `Y_MIN / Y_MAX` |
| `+0xC` | 预留给置信度、面积或跟踪ID |

MCU一致性读取：

```c
do {
    seq_before = REG_VIS_SEQ;
    read_all_targets();
    seq_after = REG_VIS_SEQ;
} while (seq_before != seq_after);
```

### 6.3 运动控制区

| 偏移 | 名称 | 说明 |
|---:|---|---|
| `0x200` | `MOTION_CONTROL` | 停止、清状态等 |
| `0x204` | `TARGET_X` | X目标步数，32位有符号 |
| `0x208` | `TARGET_Y` | Y目标步数，32位有符号 |
| `0x20C` | `MAX_SPEED` | 最大步频 |
| `0x210` | `ACCEL` | 加速度参数 |
| `0x214` | `TOOL_ARG` | 工具PWM、动作时间等 |
| `0x218` | `CMD_COMMIT` | 序号、操作码、标志；最后写入 |
| `0x21C` | `ACCEPTED_SEQ` | PL已接收的命令序号 |
| `0x220` | `DONE_SEQ` | PL已完成的命令序号 |
| `0x224` | `MOTION_STATUS` | Busy/Home/Tool/Fault |
| `0x228` | `CURRENT_X` | 当前X位置 |
| `0x22C` | `CURRENT_Y` | 当前Y位置 |
| `0x230` | `IO_INPUT` | 急停、限位、传感器 |
| `0x234` | `LIMIT_STATUS` | 各方向限位状态 |
| `0x238` | `FAULT_DETAIL` | 具体故障码 |

`CMD_COMMIT`建议格式：

```text
[31:16] CMD_SEQ
[15:8]  OPCODE
[7:0]   FLAGS
```

### 6.4 小屏邮箱区

| 偏移 | 名称 | 说明 |
|---:|---|---|
| `0x300` | `SCREEN_CONTROL` | 页面、清屏、刷新方式 |
| `0x304` | `SCREEN_STATUS` | Busy/Done/Error |
| `0x308～0x33C` | `SCREEN_PAYLOAD` | 数字、状态和短文本载荷 |
| `0x340` | `SCREEN_COMMIT` | 提交序号 |

MCU只写显示内容，现有FPGA小屏模块继续负责具体传输协议。

---

## 7. MCU固件方案

### 7.1 固件目录建议

```text
firmware/
├── main.c
├── startup/
├── linker/
├── bsp/
│   ├── apb_hal.c
│   ├── uart_hal.c
│   ├── timer_hal.c
│   └── irq_hal.c
├── services/
│   ├── vision_service.c
│   ├── planner.c
│   ├── motion_service.c
│   ├── screen_service.c
│   └── safety_service.c
└── app/
    ├── instruction.c
    ├── calibration.c
    └── state_machine.c
```

### 7.2 调度方式

第一版使用裸机事件驱动，不引入RTOS：

- Timer每1 ms产生系统节拍。
- 中断服务程序只清中断并设置事件标志。
- 主循环依次处理安全、视觉、运动、小屏和任务状态机。
- 禁止在ISR中执行排序、路径规划或小屏刷新。

```c
while (1) {
    safety_service();

    if (event_flags & EVT_NEW_VISION)
        vision_service();

    if (event_flags & EVT_MOTION_DONE)
        motion_service();

    app_state_machine();
    screen_service();
    heartbeat_service();
}
```

### 7.3 主状态机

```text
BOOT_SELFTEST
      ↓
SAFE_IDLE
      ↓
HOMING
      ↓
READY
      ↓
SNAPSHOT_LOCKED
      ↓
PLAN
      ↓
EXECUTE
      ↓
VERIFY
      ↓
COMPLETE
```

任意状态检测到急停、驱动故障、限位矛盾、APB错误或命令超时，转入 `FAULT`。

### 7.4 视觉稳定判定

建议判定条件：

- 连续3个完整识别批次。
- 有效目标数量一致。
- 按X中心排序后类别一致。
- 对应目标中心位移小于8像素。
- 无快照溢出或识别批次错误。

满足后冻结任务快照。机械执行期间不跟随每帧结果重规划，除非后续明确实现动态跟踪模式。

### 7.5 坐标标定

在MCU中使用 Q16.16 定点仿射变换：

```text
X机械 = a·u像素 + b·v像素 + c
Y机械 = d·u像素 + e·v像素 + f
```

使用4～6个标定点计算系数，系数存放于固件常量区或QSPI配置区。该计算不占用FPGA DSP。

### 7.6 内部指令集

MCU将稳定视觉结果编译为内部程序：

```text
HOME
MOVE_ABS(x, y)
MOVE_REL(dx, dy)
SET_SPEED(v, a)
TOOL_ON(parameter)
TOOL_OFF
WAIT_INPUT(sensor, timeout)
DELAY(ms)
END
```

示例：

```text
HOME
MOVE_ABS(target_0)
TOOL_ON
MOVE_ABS(bin_0)
TOOL_OFF
MOVE_ABS(target_1)
...
END
```

---

## 8. 安全设计

安全逻辑不能依赖 MCU 正常工作。

### 8.1 四层保护

1. 外部常闭急停直接切断驱动器 EN 或动力电源，同时把状态送入FPGA。
2. FPGA直接检测限位，并阻止继续向危险方向输出STEP。
3. MCU每50 ms更新一次 `HEARTBEAT`，PL超过200 ms未检测到变化时关闭驱动和工具输出。
4. 电磁铁、线圈和舵机设置硬件最大连续导通时间，超时强制关闭。

### 8.2 故障锁存

建议故障码包括：

```text
E_STOP
MCU_HEARTBEAT_TIMEOUT
X_LIMIT_CONFLICT
Y_LIMIT_CONFLICT
MOTION_TIMEOUT
DRIVER_FAULT
TOOL_TIMEOUT
APB_ERROR
VISION_OVERFLOW
```

清除故障必须同时满足：

- 急停已经释放。
- 限位状态合理。
- MCU心跳恢复。
- 用户执行一次明确的确认/复位操作。

---

## 9. 外部硬件建议

### 9.1 最小IO需求

| 方向 | 信号 |
|---|---|
| 输出 | `X_STEP`、`X_DIR` |
| 输出 | `Y_STEP`、`Y_DIR` |
| 输出 | `DRIVER_EN` |
| 输出 | `TOOL_EN`、`TOOL_PWM` |
| 输入 | `X_HOME`、`Y_HOME` |
| 输入 | `ESTOP` |
| 输入 | `DRIVER_FAULT` |
| 输入 | `TOOL_SENSOR` |

约12路IO即可实现基础双轴系统。具体扩展排针分配需要避开小屏已经占用的GPIO。

### 9.2 外设选型原则

- 步进驱动器可选TMC2209、A4988、DM542等具有STEP/DIR接口的模块。
- 输入侧增加施密特缓冲、光耦或数字隔离。
- FPGA仅提供3.3V控制信号。
- 电机、舵机和电磁铁使用独立5V/12V/24V电源。
- 禁止从开发板3.3V/5V排针直接给动力负载供电。
- 采用共地方式时要控制回流路径；条件允许时使用光耦隔离。
- 线圈、继电器和电磁铁必须带续流二极管或专用驱动器。

### 9.3 调试串口

现有板载UART TX正在输出CNN样本，推荐两种方案：

1. MCU UART0分配到扩展排针，外接USB转串口模块。
2. 增加调试MUX，由拨码开关选择“CNN样本”或“MCU日志”输出到板载UART。

第一种方案更适合联调，第二种方案适合最终演示减少外设。

---

## 10. 分阶段实施计划

### 阶段A：MCU最小启动

任务：

- 获取安路PH1P MCU SDK、交叉编译器、启动文件、链接脚本和下载调试流程。
- 将MCU IP和wrapper加入TD工程。
- 增加300 MHz Core时钟。
- MCU UART输出启动信息。

验收：

```text
上电后稳定输出：MCU BOOT / VERSION / RESET_CAUSE
```

### 阶段B：APB最小闭环

任务：

- 实现 `IP_ID`、`VERSION`、`SCRATCH`。
- MCU读取ID并循环读写SCRATCH。
- 接通CLIC中断。

验收：

- MCU读到 `0x56504C43`。
- 连续读写SCRATCH 10000次无错误。
- APB非法地址返回 `PSLVERR`，系统不死机。

### 阶段C：视觉快照

任务：

- 实现 `vision_snapshot_cdc`。
- 接入8目标类别和坐标。
- MCU完成读取、排序和三批次稳定判定。

验收：

- 连续运行10000个快照无数据撕裂。
- 类别、坐标与HDMI显示一致。
- 改变目标后不会误用旧快照。

### 阶段D：运动控制

任务：

- 实现单轴STEP/DIR和加减速。
- 再扩展至双轴、回零、限位和工具控制。
- 先用示波器/逻辑分析仪验证，不接电机。

验收：

- STEP频率、脉宽和加减速符合配置。
- 指令序号只执行一次。
- 急停、限位、心跳超时均可独立停止输出。

### 阶段E：机械与整机闭环

任务：

- 单轴低速空载。
- 双轴回零与定位。
- 接入末端工具和传感器。
- 完成像素到机械坐标标定。
- 接入小屏状态显示。

验收：

- 重复定位稳定。
- 漏抓、超时、急停均有明确故障码。
- 更换目标后能生成不同指令序列。
- 连续运行30分钟无失控、卡死或线圈异常发热。

### 阶段F：比赛展示优化

任务：

- HDMI展示视觉证据、目标编号、稳定状态和当前执行对象。
- 小屏展示任务状态、当前指令、进度和故障提示。
- 准备自动模式、单步模式和安全演示模式。
- 加入故障注入演示，如遮挡、急停、目标移动和执行失败重试。

---

## 11. 推荐团队分工

| 角色 | 工作内容 | 主要交付物 |
|---|---|---|
| FPGA视觉 | 输出稳定批次信号，整理8目标结果接口 | 视觉快照源接口、仿真数据 |
| FPGA总线 | MCU接入、APB寄存器、CLIC中断、CDC | `mcu_subsystem`、寄存器文档 |
| FPGA运动 | STEP/DIR、加减速、回零、限位、安全 | `motion_engine`、波形与仿真 |
| MCU固件 | BSP、状态机、目标排序、任务规划 | 可编译固件、串口日志 |
| 机械电气 | 机构、电机驱动、传感器、供电、急停 | 接线图、BOM、安全测试 |
| UI/答辩 | HDMI叠加、小屏页面、演示流程 | 页面定义、答辩脚本、演示视频 |

建议由一人统一维护APB寄存器定义，RTL和固件共同引用同一份地址/位定义，避免两边各写一套后产生偏差。

---

## 12. 开工前必须确认的事项

- [ ] PH1P MCU SDK和RISC-V交叉编译器是否已经取得。
- [ ] MCU固件从 `0x20E00000` 的加载、固化和启动方式。
- [ ] JTAG是否能同时调试FPGA和MCU，具体选择方式是什么。
- [ ] MCU侧APB绝对基地址与缓存/访问属性。
- [ ] 300 MHz Core时钟及100 MHz Timer/APB时钟的官方接法。
- [ ] 当前小屏使用的接口协议和占用GPIO。
- [ ] J1/J2可用GPIO到芯片封装管脚的最终约束表。
- [ ] 最终机械形态、轴数、负载、行程和目标速度。
- [ ] 电机驱动器输入是否兼容3.3V逻辑。

其中前五项是MCU接入的关键前置条件。若SDK或启动方式没有确认，不应直接进入完整视觉和机械联调。

---

## 13. 近期最小行动项

当前只做以下最小闭环：

```text
MCU启动
  ↓
UART打印启动信息
  ↓
读取APB IP_ID
  ↓
循环读写SCRATCH
  ↓
触发一次PL→MCU中断
```

该闭环通过后，再依次增加视觉快照、运动脉冲和机械外设。第一阶段不要同时接入屏幕、视觉、双轴电机和工具，否则出现问题时无法快速判断属于时钟、固件、APB、CDC还是机械电气。

---

## 14. 最终技术亮点

1. **单芯片异构架构**：RISC-V MCU与FPGA逻辑位于同一芯片内。
2. **视觉程序动态生成**：多目标类别和坐标被编译为可执行机械指令，而非简单亮灯。
3. **软硬件职责清晰**：MCU负责灵活决策，FPGA负责确定性实时控制。
4. **原子视觉快照**：通过跨时钟域握手确保多目标数据一致性。
5. **版本化APB协议**：便于软硬件独立开发、调试和后续扩展。
6. **失效安全设计**：急停、限位、心跳和工具超时均可脱离MCU独立生效。
7. **算法过程透明**：HDMI显示视觉证据，小屏显示任务状态，机械机构展示真实执行结果。

这套架构既可承载数字分拣、XY搬运、机械演奏等比赛演示，也可在后续更换视觉模型和机械末端，而不需要推翻MCU调度与运动控制框架。
