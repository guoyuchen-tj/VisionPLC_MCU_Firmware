# Stage B: communication check

The MCU runs the test from `D:/FD_Alogic/MCU_FPGA_CONNECT/main.c`.
The native MCU-to-Fabric APB CPU address is not confirmed by the local
documentation. This implementation uses the already validated AHB address
window to submit requests to a real APB master in the FPGA's 50 MHz domain.
It does not claim to validate the MCU's native APB bridge or CPU access-fault
handling. No FPGA synthesis or programming is performed by the verification.

## AHB mailbox

CPU base: `0x80000100`. Aligned 32-bit accesses only.

| Offset | Meaning |
| --- | --- |
| 0x00 | APB byte address, 20 bits |
| 0x04 | Write payload |
| 0x08 | Command: 1=read, 3=write |
| 0x0C | Completed read payload |
| 0x10 | Status: bit0 BUSY, bit1 DONE, bit2 captured PSLVERR |
| 0x14 | Write strobes, bits 3:0 |

Set address, payload and strobes, execute a barrier, then write command.
Poll until BUSY=0 and DONE=1 before accessing the next transaction.
Writes while busy are ignored. Read data is valid only when DONE=1 and
PSLVERR=0. A new accepted command clears the preceding DONE/error state.
Request/acknowledge toggles cross the clock domains; payloads remain stable
until the acknowledge returns. Both sides must be reset together, as in the
top-level PLL reset sequence.

## APB register bank

| Offset | Register | Access |
| --- | --- | --- |
| 0x00 | IP_ID=0x56504C43 | RO |
| 0x04 | VERSION=0x00010000 (1.0) | RO |
| 0x08 | SCRATCH, reset=0 | RW, per-byte write strobes |
| 0x0C | IRQ_ENABLE bit0, reset=0 | RW |
| 0x10 | IRQ_PENDING bit0 | RO / write-one-to-clear |
| 0x14 | IRQ_TEST: write bit0 to set pending; read current IRQ level | RW |

Unimplemented/unaligned addresses and writes to RO registers complete with
PSLVERR and no side effects. The mailbox reports the error as status instead
of raising an AHB error to the CPU, allowing subsequent requests to continue.

IRQ is a latched, maskable level. It is synchronized to `core_clk` and wired
to `clic_irq[0]`. The corresponding CLIC ID is not yet confirmed, so this
demo does not register/enable a CPU ISR. The test validates the IRQ source
via polling, not interrupt delivery. It clears and masks the source at the end.

## MCU acceptance

1. ID and version match.
2. Four byte-lane writes preserve unselected bytes.
3. Illegal read/write and RO write report PSLVERR; SCRATCH stays unchanged.
4. 10000 varying-pattern SCRATCH write/read pairs have no errors.
5. Masked pending, assertion and clear work; 100 IRQ source cycles pass.
6. UART prints `STAGE_B PASS`, then `VISIONPLC MCU READY` and remains alive for merged-video acceptance.

The old isolated AHB addition/display-2 regression is no longer part of the
merged-video firmware. Debugging must run the new ELF with hardware that
includes the bridge and register bank; old artifacts cannot run this test.

## Simulation

`tests/stage_b_tb.v` drives the actual AHB address/data phases at approximately
150 MHz and APB at 50 MHz. It checks 10000 pairs, byte lanes, address aliases,
alignment, RO protection, IRQ and reset. Add it to a separate simulation
library with `fpga_apb_regs.v` and `fpga_apb_bridge.v`, not to the TD design.

Example ModelSim commands (from a temporary simulation directory):

```tcl
vlib stage_b_work
vlog -sv -work stage_b_work D:/Anlogic/MCU_FPGA/fpga_apb_regs.v D:/Anlogic/MCU_FPGA/fpga_apb_bridge.v D:/Anlogic/MCU_FPGA/tests/stage_b_tb.v
vsim -c -onfinish exit -lib stage_b_work stage_b_tb -do "run -all"
```
