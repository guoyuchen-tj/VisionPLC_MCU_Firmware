# 队友任务书：PH1P35 内置 MCU 最小系统与工具链打通

> 项目：VisionPLC 智能视觉控制与机械执行系统  
> 目标板：HX1P35A  
> FPGA：PH1P35MDG324-3  
> 任务负责人：________  
> 开始日期：________  
> 计划完成日期：________

## 1. 一句话任务

请独立打通 **PH1P35 内置 RISC-V MCU 的“TD 硬件配置 → HPF → FutureDynasty Platform/BSP → C 程序编译 → JTAG 下载调试 → UART 输出”完整闭环**，并把能够复现的工程、生成文件、地址信息和操作记录交回主项目负责人。

本任务暂时不实现 VisionPLC 业务逻辑，不控制电机，不接传感器，也不修改视觉识别算法。

## 2. 最终必须交付什么

任务完成时必须交付以下内容，缺少任意关键项都不算完成。

### 2.1 软件与版本记录

- TangDynasty 版本号和安装路径。
- FutureDynasty 版本号和安装路径，目标版本为 `2026.2`。
- PH1P35 GCC 版本，预期为 `13.1.1`。
- 使用的 OpenOCD 配置名称，预期包含 `ph1p35.cfg`。
- JTAG 和 USB-UART 在 Windows 设备管理器中的识别结果。

### 2.2 可复现工程

- 一个独立的 TD 最小 MCU 工程。
- 该工程生成的 `.bit`。
- 该工程导出的 `.hpf`。
- 一个 FutureDynasty Platform 工程。
- Platform 自动生成的 BSP、`system.mss` 和相关驱动文件。
- 一个 FutureDynasty Application 工程。
- Application 生成的 `.elf`、`.map` 和链接脚本 `.ld`。
- 可直接编译的 `main.c`。

### 2.3 运行证据

- TD 综合、布局布线和 Bitstream 生成成功截图。
- HPF 导出成功截图。
- FutureDynasty Platform 创建成功截图。
- Application Build 为 `0 errors` 的截图或完整日志。
- GDB/OpenOCD 成功连接 MCU 的截图。
- 串口连续输出 Hello World 的截图。
- 至少完成 5 次“断电 → 上电 → 下载 → 运行”，并填写测试表。

### 2.4 必须记录的地址信息

- MCU Reset Vector。
- 链接脚本中的 RAM/ROM 起始地址和大小。
- HPF View 中显示的所有 MCU 可见地址空间。
- 如果已经加入 APB 测试模块，记录它的绝对 Base Address 和 Size。
- 地址信息必须附截图或生成文件作为证据，不能只口头填写。

## 3. 任务边界

### 3.1 你负责

- 安装、验证 FutureDynasty 2026.2。
- 新建独立最小工程，配置 PH1P35 MCU。
- 生成 bitstream 和 HPF。
- 创建 Platform、BSP 和 Application。
- 编译、下载、调试 Hello World。
- 找出并记录链接脚本、BSP、HPF 地址信息。
- 整理可复现操作步骤和问题记录。

### 3.2 暂时不要做

- 不修改现有目标识别算法。
- 不修改摄像头、DDR、DSI 显示链路。
- 不接入电机、电磁铁、限位开关等外设。
- 不设计完整 VisionPLC 寄存器表。
- 不直接在主工程生成的 `MCU.v`、`MCU_f*.v` 中手改参数。
- 未经主负责人确认，不覆盖主工程的 `.al`、`.bit`、IP 目录或约束文件。

### 3.3 工作方式

优先新建独立目录，例如：

```text
PH1P35_MCU_Bringup/
├─ td_mcu_min/
├─ fd_workspace/
├─ output/
├─ screenshots/
└─ bringup_log.md
```

不要直接拿视觉主工程做第一次 MCU 调试。先用最小工程排除摄像头、DDR、显示和时序收敛带来的干扰。

## 4. 固定技术参数

除非实际板卡资料明确冲突，否则按下表配置。

| 项目 | 配置值 |
|---|---|
| FPGA | PH1P35MDG324-3 |
| TD | 6.2.1 或与 PH1P35 MCU 指南匹配的 6.2.x |
| FutureDynasty | 2026.2 |
| 外部输入时钟 | 50 MHz，板卡管脚 `C4` |
| MCU Core Clock | 300 MHz |
| MCU Timer Clock | 100 MHz |
| MCU APB Clock | 50 MHz |
| AHB 分频 | 2，目标 150 MHz |
| Reset Vector | `0x20E00000` |
| UART0 | 115200、8N1、无流控 |
| UART0 RXD | `J1` |
| UART0 TXD | `H1` |
| 操作系统 | `standalone` |
| Processor | HPF 中识别出的 `rpu` |
| JTAG | 使用板载 PAD JTAG，不导出 Fabric JTAG |

注意：`0x20E00000` 是 Reset Vector，不是 APB 外设基址。

## 5. 阶段 A：工具链验收

### A1. 安装并启动 FutureDynasty

1. 安装或解压 `FD_2026.2.exe` / `FD_2026.2.zip`。
2. 建议使用纯英文、无空格路径，例如：

```text
D:\Anlogic\FutureDynasty_2026.2
```

3. 启动 FutureDynasty。
4. 检查 `File → New` 中是否存在：
   - `Platform Project`
   - `Application Project`
5. 记录 `Help → About` 中的版本号。

### A2. 验收标准

- FutureDynasty 正常启动。
- 版本为 `2026.2`。
- 能看到 Platform 和 Application 工程类型。
- 不提示缺少 Java、GCC、OpenOCD 或关键组件。

### A3. 进入下一阶段的判断

只有 A2 全部满足才能进入阶段 B。若失败，记录报错全文、安装路径和截图，不要反复重装而不留记录。

## 6. 阶段 B：建立最小 TD MCU 硬件工程

### B1. 创建工程

1. 在纯英文目录中新建 TD 工程。
2. 器件选择 `PH1P35MDG324-3`。
3. 输入时钟采用板载 50 MHz，并约束到 `C4`。
4. 创建 PLL 或使用可靠时钟模块，输出：
   - `core_clk = 300 MHz`
   - `timer_clk = 100 MHz`
   - `apb_clk = 50 MHz`
5. PLL `LOCK` 参与复位释放，复位必须同步处理。

### B2. 配置 MCU IP

配置 MCU IP 时至少满足：

- Reset Vector：`0x20E00000`
- Core Clock FREQ：`300 MHz`
- Timer Clock FREQ：`100 MHz`
- AHB Div：`2`
- UART0：启用，`115200`
- APB：启用
- AHB：本阶段可启用但不挂从设备
- MCU JTAG/Fabric JTAG 选项：**不要勾选**
- GPIO0～GPIO7：本阶段不需要可不导出

配置完成后重新生成 IP。不要在生成后的 Verilog 文件中把 `MCU_JTAG_SEL` 从 `1` 手动改成 `0`，必须由 IP 配置重新生成。

### B3. 顶层连接

必须连接：

- `core_clk`：300 MHz
- `timer_clk`：100 MHz
- `apb_clk`：50 MHz
- `core_reset`、`por_reset`、`apb_rst`
- `uart0_rx`、`uart0_tx`
- `nmi = 0`
- `clic_irq = 0`

未使用 AHB 时返回：

```verilog
hrdata = 32'h0000_0000;
hresp  = 2'b00;
hready = 1'b1;
```

APB 暂未挂实际从设备时返回：

```verilog
prdata  = 32'h0000_0000;
pready  = 1'b1;
pslverr = 1'b0;
```

禁止让 `pready`、`hready` 或返回数据悬空。

### B4. 引脚约束

至少确认：

```text
50 MHz 输入：C4
UART0 RXD：J1
UART0 TXD：H1
```

JTAG 使用开发板原有下载接口，不额外约束一组 Fabric JTAG 管脚。

### B5. 验收标准

- IP 重新生成成功。
- 生成文件中 Reset Vector 为 `0x20E00000`。
- `MCU_JTAG_SEL` 最终为 PAD JTAG 对应状态，预期为 `0`。
- APB 接口存在且已连接。
- 所有时钟和复位均有明确来源。
- 未使用总线返回值不悬空。
- TD Elaboration/Synthesis 无 Error。

### B6. 进入下一阶段的判断

综合 `0 errors` 且上述人工检查全部通过，才能进入阶段 C。

## 7. 阶段 C：生成 Bitstream 并导出 HPF

### C1. 操作

1. 运行 Synthesis。
2. 运行布局布线。
3. 检查时序报告，至少确认 MCU 三个时钟均被识别。
4. 生成 bitstream。
5. 用 TD 将 bitstream 下载到开发板。
6. 导出 Hardware Platform File，命名为：

```text
MCU_startup.hpf
```

### C2. 验收标准

- Synthesis、布局布线和 Bitstream 均为成功状态。
- TD 能识别板载 JTAG 并成功下载 bitstream。
- `MCU_startup.hpf` 存在且文件大小非 0。
- HPF 的修改时间晚于最后一次硬件修改时间。

### C3. 进入下一阶段的判断

必须同时拿到可下载的 `.bit` 和最新 `.hpf`，才能进入阶段 D。

## 8. 阶段 D：生成 Platform 和 BSP

### D1. 创建 Platform

1. 启动 FutureDynasty 2026.2。
2. 选择 `File → New → Platform Project`。
3. 选择刚导出的 `MCU_startup.hpf`。
4. OS 选择 `standalone`。
5. Processor 选择 HPF 中识别到的 `rpu`。
6. Platform 名称建议：

```text
ph1p35_mcu_platform
```

7. 完成创建并等待 BSP 生成。
8. 双击 `system.mss`，确认 BSP View 能正常打开。

### D2. 获取地址证据

1. 双击 Platform 工程根目录中的 `.hpf`。
2. 进入 HPF View。
3. 截图保存 Module Summary、Base Address 和 Size。
4. 把所有地址填写到 `bringup_log.md`。

此处得到的地址才是当前工程的真实地址。禁止把 Reset Vector 当成 APB Base Address。

### D3. 验收标准

- Platform 创建成功。
- `system.mss` 存在。
- BSP Sources 能正常生成。
- HPF View 能打开并显示地址空间。
- 能找到并记录处理器、存储器和总线相关地址。

### D4. 进入下一阶段的判断

Platform 和 BSP 均无错误，且地址空间已截图、记录，才能进入阶段 E。

## 9. 阶段 E：创建并编译 Hello World

### E1. 创建 Application

1. 选择 `File → New → Application Project`。
2. Target Hardware 选择 `ph1p35_mcu_platform`。
3. 选择 `HELLO_WORLD` 模板。
4. 工程名建议：

```text
ph1p35_hello_world
```

5. 编译项目。

### E2. 检查生成物

必须找到并保存：

- `.elf`
- `.map`
- `.ld`
- `main.c`
- BSP 头文件和驱动目录

打开 Link View，记录 RAM/ROM 的 Base Address 和 Size。再切换到 Source 查看链接脚本文本。

如果修改过链接脚本，必须执行：

```text
Clean Project → Build Project
```

### E3. 验收标准

- Build Console 显示 `0 errors`。
- `.elf` 确实生成。
- 可以打开链接脚本。
- 链接地址与 Reset Vector、实际存储器布局不存在明显冲突。

## 10. 阶段 F：JTAG 和 UART 联调

### F1. 硬件连接

1. 开发板正常供电。
2. 板载 JTAG USB 已连接电脑。
3. USB-UART 已连接电脑。
4. 在设备管理器记录 JTAG 和串口设备。
5. 串口工具设置为：

```text
115200 baud
8 data bits
1 stop bit
No parity
No flow control
```

### F2. 首次调试配置

推荐第一次采用分步流程：

1. 先用 TD 手动下载最新 `.bit`。
2. 在 FutureDynasty 中创建 GDB OpenOCD Debug 配置。
3. OpenOCD 配置使用 PH1P35 对应配置，通常为 `ph1p35.cfg`。
4. 调试端口通常为 `localhost:3333`。
5. 不启用 `Use Jtag Boot`。
6. `Program FPGA` 暂不勾选。
7. `Load symbols` 勾选。
8. `Load executable` 勾选。
9. 启动 Debug。
10. 如果默认停在 `main()`，按 Resume/F8 继续运行。

### F3. Hello World 内容

程序至少每秒打印一次递增计数，便于确认程序持续运行，而不是只看到启动残留信息。例如：

```c
int main(void)
{
    unsigned long count = 0;

    while (1) {
        printf("PH1P35 MCU alive: %lu\r\n", count++);
        /* 使用 BSP 提供的延时函数，或暂用可靠的软件延时。 */
    }
}
```

实际 `main.c` 应优先沿用官方模板的头文件、初始化和输出函数，不要为了套用上述片段破坏 BSP 初始化。

### F4. 验收标准

- OpenOCD 正常启动。
- GDB 能连接 MCU。
- 能停在 `main()`。
- Resume 后 UART 每秒输出递增数字。
- 连续运行 10 分钟无停止、乱码或异常复位。
- 完成 5 次断电重测，至少 5/5 成功。

## 11. 阶段 G：为主项目提取可复用结论

完成最小闭环后，整理以下信息交给主负责人：

1. 哪个 TD 版本确实能生成 PH1P35 MCU 工程。
2. MCU IP 每个关键选项的最终值和截图。
3. JTAG 选项的正确状态。
4. 三个时钟的实际来源与频率。
5. 复位连接和有效电平。
6. UART0 管脚和串口参数。
7. HPF 导出菜单的准确路径。
8. Platform/Application 创建过程。
9. BSP 和链接脚本的实际路径。
10. HPF View 中的地址空间截图。
11. Hello World 的下载、调试与启动方式。
12. 所有踩坑、报错全文和解决方式。

完成本阶段后，先停止，不要擅自把最小工程直接合入视觉主工程。由主负责人确认后再开展 APB 寄存器和主工程集成。

## 12. 测试记录表

| 次数 | 断电重启 | TD 下载 bit | OpenOCD 连接 | ELF 下载 | UART 递增输出 | 结果 |
|---:|---|---|---|---|---|---|
| 1 |  |  |  |  |  |  |
| 2 |  |  |  |  |  |  |
| 3 |  |  |  |  |  |  |
| 4 |  |  |  |  |  |  |
| 5 |  |  |  |  |  |  |

## 13. 问题记录模板

每遇到问题都按下面格式记录：

```text
问题编号：MCU-001
所处阶段：例如 D1 创建 Platform
操作时间：
软件版本：
操作步骤：
报错原文：
截图文件：
尝试过的方法：
最终原因：
解决方法：
是否可稳定复现：
```

不要只记录“打不开”“下载失败”“后来好了”。必须保存报错原文和解决条件。

## 14. 最终回报模板

```text
任务状态：完成 / 部分完成 / 阻塞

TD版本：
FD版本：
GCC版本：
OpenOCD配置：

Bitstream路径：
HPF路径：
Platform路径：
BSP路径：
Application路径：
ELF路径：
链接脚本路径：

Reset Vector：
ROM地址范围：
RAM地址范围：
APB地址范围：

UART测试：通过 / 未通过
JTAG测试：通过 / 未通过
5次断电测试：通过次数 __ / 5

当前遗留问题：
建议主工程集成方式：
```

## 15. 参考资料

- `使用资料/00_使用指南/PH1P35内MCU核使用指南.pdf`
- `使用资料/01_安路PH1P35官方资料/DS1300_PH1P_Datasheet (1).pdf`
- `使用资料/03_FutureDynasty资料/SWUG500_FD_User_Guide.pdf`
- `使用资料/03_FutureDynasty资料/FD_Release_Notes.pdf`

遇到教程和本任务书不一致时，以实际芯片 `PH1P35MDG324-3`、FutureDynasty 2026.2 和当前板卡原理图为准，并把差异记录下来后再与主负责人确认。
