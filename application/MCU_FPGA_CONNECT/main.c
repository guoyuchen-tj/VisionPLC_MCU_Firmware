#include <stdio.h>
#include "al_core.h"
#include "al_intr.h"
#include "al_systimer.h"

#define FPGA_POLL_LIMIT       1000000U
#define VISIONPLC_CLIC_IRQ    27U

/* Confirmed CPU AHB mailbox -> FPGA 50 MHz APB register bank. */
#define APB_MAILBOX_BASE      0x80000100UL

#define APB_IP_ID             0x000U
#define APB_IP_VERSION        0x004U
#define APB_CAPABILITY        0x008U
#define APB_BUILD_ID          0x00CU
#define APB_IRQ_STATUS        0x010U
#define APB_IRQ_ENABLE        0x014U
#define APB_SYS_STATUS        0x018U
#define APB_FAULT_STATUS      0x01CU
#define APB_HEARTBEAT         0x020U
#define APB_SYS_CONTROL       0x024U
#define APB_PL_UPTIME         0x028U
#define APB_IRQ_TEST          0x02CU
#define APB_SCRATCH           0x030U
#define APB_ILLEGAL           0x034U

#define IRQ_VISION            (1U << 0)
#define IRQ_MOTION_DONE       (1U << 1)
#define IRQ_FAULT             (1U << 2)
#define IRQ_SCREEN_DONE       (1U << 3)
#define IRQ_TEST              (1U << 4)
#define IRQ_ALL               0x1FU

#define SYS_CONTROL_ARM       (1U << 0)
#define SYS_CONTROL_CLEAR     (1U << 1)
#define SYS_CONTROL_DISARM    (1U << 2)

#define SYS_STATUS_ARMED      (1U << 0)
#define SYS_STATUS_SAFE       (1U << 1)

static volatile AL_U32 g_IrqWake;
static volatile AL_U32 g_IrqCount;
static volatile AL_U32 g_EventFlags;

/* Returns 0 for success, 1 for PSLVERR, and -1 for mailbox timeout. */
static AL_S32 FpgaApbTransfer(AL_U32 Address, AL_U32 Write,
                              AL_U32 Strobes, AL_U32 *Data)
{
    AL_U32 Status;
    AL_U32 Index;

    AL_REG32_WRITE(APB_MAILBOX_BASE + 0x00U, Address);
    AL_REG32_WRITE(APB_MAILBOX_BASE + 0x04U, Write ? *Data : 0U);
    AL_REG32_WRITE(APB_MAILBOX_BASE + 0x14U, Strobes);
    DSB();
    AL_REG32_WRITE(APB_MAILBOX_BASE + 0x08U, Write ? 3U : 1U);
    DSB();

    for (Index = 0; Index < FPGA_POLL_LIMIT; ++Index) {
        Status = AL_PL_REG32_READ(APB_MAILBOX_BASE + 0x10U);
        if ((Status & 3U) == 2U) {
            if (Status & 4U)
                return 1;
            if (!Write)
                *Data = AL_PL_REG32_READ(APB_MAILBOX_BASE + 0x0CU);
            return 0;
        }
    }
    return -1;
}

static AL_S32 FpgaRead(AL_U32 Address, AL_U32 *Data)
{
    return FpgaApbTransfer(Address, 0U, 0U, Data);
}

static AL_S32 FpgaWrite(AL_U32 Address, AL_U32 Data)
{
    return FpgaApbTransfer(Address, 1U, 15U, &Data);
}

static AL_S32 FpgaS2Regression(AL_VOID)
{
    AL_U32 Data = 0U;
    AL_U32 Expected;
    AL_U32 Seed = 0x13579BDFU;
    AL_U32 Index;

    if (FpgaRead(APB_IP_ID, &Data) != 0 || Data != 0x56504C43U)
        return -1;
    AL_LOG(AL_LOG_LEVEL_NOTICE, "APB IP_ID: 0x%08x PASS\r\n",
           (unsigned int)Data);

    if (FpgaRead(APB_IP_VERSION, &Data) != 0 || Data != 0x00020000U)
        return -1;

    if (FpgaWrite(APB_SCRATCH, 0U) != 0)
        return -1;
    Expected = 0U;
    for (Index = 0; Index < 4U; ++Index) {
        Data = 0xA5A5A5A5U;
        Expected |= 0xA5U << (Index * 8U);
        if (FpgaApbTransfer(APB_SCRATCH, 1U, 1U << Index, &Data) != 0 ||
            FpgaRead(APB_SCRATCH, &Data) != 0 || Data != Expected)
            return -1;
    }
    AL_LOG(AL_LOG_LEVEL_NOTICE, "APB byte strobes PASS\r\n");

    if (FpgaRead(APB_ILLEGAL, &Data) != 1)
        return -1;
    Data = 0xBAD0BAD0U;
    if (FpgaApbTransfer(APB_ILLEGAL, 1U, 15U, &Data) != 1 ||
        FpgaApbTransfer(APB_IP_ID, 1U, 15U, &Data) != 1 ||
        FpgaRead(APB_SCRATCH, &Data) != 0 || Data != Expected)
        return -1;
    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "APB PSLVERR read/write PASS, legal access recovered\r\n");

    for (Index = 0; Index < 10000U; ++Index) {
        Seed ^= Seed << 13;
        Seed ^= Seed >> 17;
        Seed ^= Seed << 5;
        Expected = Seed;
        switch (Index & 7U) {
        case 0U: Expected = 0U; break;
        case 1U: Expected = 0xFFFFFFFFU; break;
        case 2U: Expected = 0xAAAAAAAAU; break;
        case 3U: Expected = 0x55555555U; break;
        case 4U: Expected = 1U << ((Index >> 3) & 31U); break;
        case 5U: Expected = ~(1U << ((Index >> 3) & 31U)); break;
        default: break;
        }
        if (FpgaWrite(APB_SCRATCH, Expected) != 0 ||
            FpgaRead(APB_SCRATCH, &Data) != 0 || Data != Expected)
            return -1;
    }
    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "APB SCRATCH 10000/10000, errors=0 PASS\r\n");

    /* MCU debug reset does not reset the FPGA register bank.  Clear any
       latched S3 event from an earlier run before testing the S2 IRQ source. */
    if (FpgaWrite(APB_IRQ_ENABLE, 0U) != 0 ||
        FpgaWrite(APB_IRQ_STATUS, IRQ_ALL) != 0 ||
        FpgaRead(APB_IRQ_STATUS, &Data) != 0 || Data != 0U) {
        AL_LOG(AL_LOG_LEVEL_NOTICE,
               "APB IRQ pre-clear FAIL, status=0x%08x\r\n",
               (unsigned int)Data);
        return -1;
    }

    if (FpgaWrite(APB_IRQ_TEST, IRQ_TEST) != 0 ||
        FpgaRead(APB_IRQ_STATUS, &Data) != 0 || Data != IRQ_TEST) {
        AL_LOG(AL_LOG_LEVEL_NOTICE,
               "APB IRQ inject FAIL, status=0x%08x\r\n",
               (unsigned int)Data);
        return -1;
    }

    if (FpgaWrite(APB_IRQ_STATUS, IRQ_TEST) != 0 ||
        FpgaRead(APB_IRQ_STATUS, &Data) != 0 || Data != 0U) {
        AL_LOG(AL_LOG_LEVEL_NOTICE,
               "APB IRQ W1C FAIL, status=0x%08x\r\n",
               (unsigned int)Data);
        return -1;
    }

    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "APB IRQ source polling PASS\r\n");
    AL_LOG(AL_LOG_LEVEL_NOTICE, "STAGE_B PASS\r\n");
    return 0;
}

static AL_VOID FpgaIrqHandler(AL_VOID *Param)
{
    (AL_VOID)Param;
    g_IrqWake = 1U;
    g_IrqCount++;
    /* The PL line is level-sensitive. Main clears W1C before re-enabling it. */
    (AL_VOID)AlIntr_SetInterrupt(VISIONPLC_CLIC_IRQ, AL_FUNC_DISABLE);
}

static AL_S32 FpgaIrqService(AL_VOID)
{
    AL_U32 Status;

    if (FpgaRead(APB_IRQ_STATUS, &Status) != 0)
        return -1;
    if (Status != 0U && FpgaWrite(APB_IRQ_STATUS, Status) != 0)
        return -1;

    g_EventFlags |= Status;
    g_IrqWake = 0U;
    DSB();
    return AlIntr_SetInterrupt(VISIONPLC_CLIC_IRQ, AL_FUNC_ENABLE);
}

static AL_S32 WaitForFpgaIrq(AL_U32 Expected, AL_U32 PreviousCount,
                             AL_U32 TimeoutMs)
{
    AL_U64 Start = AlSys_GetTimerTickCount();
    AL_U64 Limit = Start +
                   (AlSys_GetTimerFreq() * TimeoutMs) / 1000U;

    while (g_IrqCount == PreviousCount &&
           AlSys_GetTimerTickCount() < Limit) {
    }
    if (g_IrqCount != PreviousCount && FpgaIrqService() == 0 &&
        (g_EventFlags & Expected) == Expected) {
        g_EventFlags &= ~Expected;
        return 0;
    }
    return -1;
}

static AL_S32 FpgaS3SafetyTest(AL_VOID)
{
    AL_INTR_AttrStrct Attr = {
        .VectorMode = NON_VECTOR_INTERRUPT,
        .TrigMode = LEVEL_HIGH_TRIGGER,
        .Priority = 0
    };
    const AL_U32 Sources[5] = {
        IRQ_VISION, IRQ_MOTION_DONE, IRQ_FAULT, IRQ_SCREEN_DONE, IRQ_TEST
    };
    AL_U32 Data;
    AL_U32 Heartbeat = 1U;
    AL_U32 PreviousCount;
    AL_U32 StartMs;
    AL_U32 EndMs;
    AL_U32 Index;

    g_IrqWake = 0U;
    g_IrqCount = 0U;
    g_EventFlags = 0U;
    if (FpgaWrite(APB_IRQ_STATUS, IRQ_ALL) != 0 ||
        FpgaWrite(APB_IRQ_ENABLE, IRQ_ALL) != 0)
        return -1;

    if (AlIntr_RegHandler(VISIONPLC_CLIC_IRQ, &Attr,
                          FpgaIrqHandler, AL_NULL) != 0)
        return -1;
    (AL_VOID)AlIntr_SetLocalInterrupt(AL_FUNC_ENABLE);

    for (Index = 0; Index < 5U; ++Index) {
        PreviousCount = g_IrqCount;
        if (FpgaWrite(APB_IRQ_TEST, Sources[Index]) != 0 ||
            WaitForFpgaIrq(Sources[Index], PreviousCount, 100U) != 0)
            return -1;
    }
    if (g_IrqCount != 5U)
        return -1;
    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "S3 CLIC IRQ 5/5 PASS, CLIC_ID=%u\r\n",
           (unsigned int)VISIONPLC_CLIC_IRQ);

    if (FpgaWrite(APB_SYS_CONTROL, SYS_CONTROL_CLEAR) != 0 ||
        FpgaWrite(APB_HEARTBEAT, Heartbeat) != 0 ||
        FpgaWrite(APB_SYS_CONTROL, SYS_CONTROL_ARM) != 0)
        return -1;

    for (Index = 0; Index < 5U; ++Index) {
        AlSys_MDelay(50U);
        if (FpgaWrite(APB_HEARTBEAT, ++Heartbeat) != 0 ||
            FpgaRead(APB_SYS_STATUS, &Data) != 0 ||
            (Data & (SYS_STATUS_ARMED | SYS_STATUS_SAFE)) !=
            (SYS_STATUS_ARMED | SYS_STATUS_SAFE))
            return -1;
    }
    AL_LOG(AL_LOG_LEVEL_NOTICE, "S3 heartbeat 50ms keepalive PASS\r\n");

    if (FpgaRead(APB_PL_UPTIME, &StartMs) != 0)
        return -1;
    PreviousCount = g_IrqCount;
    if (WaitForFpgaIrq(IRQ_FAULT, PreviousCount, 300U) != 0 ||
        FpgaRead(APB_PL_UPTIME, &EndMs) != 0 ||
        FpgaRead(APB_FAULT_STATUS, &Data) != 0 || Data != 1U ||
        FpgaRead(APB_SYS_STATUS, &Data) != 0 ||
        (Data & SYS_STATUS_SAFE) != 0U)
        return -1;
    if ((EndMs - StartMs) < 180U || (EndMs - StartMs) > 220U)
        return -1;
    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "S3 MCU_TIMEOUT %ums PASS, safe output OFF\r\n",
           (unsigned int)(EndMs - StartMs));

    if (FpgaWrite(APB_SYS_CONTROL, SYS_CONTROL_ARM) != 0 ||
        FpgaRead(APB_SYS_STATUS, &Data) != 0 ||
        (Data & SYS_STATUS_SAFE) != 0U)
        return -1;
    if (FpgaWrite(APB_SYS_CONTROL, SYS_CONTROL_CLEAR) != 0 ||
        FpgaWrite(APB_HEARTBEAT, ++Heartbeat) != 0 ||
        FpgaWrite(APB_SYS_CONTROL, SYS_CONTROL_ARM) != 0 ||
        FpgaRead(APB_SYS_STATUS, &Data) != 0 ||
        (Data & SYS_STATUS_SAFE) == 0U)
        return -1;

    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "S3 explicit CLEAR + HEARTBEAT + ARM recovery PASS\r\n");
    AL_LOG(AL_LOG_LEVEL_NOTICE, "STAGE_S3 PASS\r\n");
    return 0;
}

AL_S32 main()
{
    AL_U32 Heartbeat = 100U;
    AL_U32 Events;
    AL_U32 Fault;
    AL_U64 LastHeartbeat;
    AL_U64 HeartbeatPeriod;

    AL_LOG(AL_LOG_LEVEL_NOTICE,
           "Hello World From Anlogic! BSP version:%s.%d.%d\r\n",
           AL_BSP_NAME, AL_BSP_VERSION_MAJOR, AL_BSP_VERSION_MINOR);

    /* MMIO is kept uncached even when XIP enables D-cache. */
    AlCache_DisableDCache();
    DSB();

    if (FpgaS2Regression() != 0) {
        AL_LOG(AL_LOG_LEVEL_NOTICE, "STAGE_B FAIL\r\n");
        return -1;
    }
    if (FpgaS3SafetyTest() != 0) {
        AL_LOG(AL_LOG_LEVEL_NOTICE, "STAGE_S3 FAIL\r\n");
        return -1;
    }

    AL_LOG(AL_LOG_LEVEL_NOTICE, "VISIONPLC MCU READY\r\n");
    HeartbeatPeriod = AlSys_GetTimerFreq() / 20U;
    LastHeartbeat = AlSys_GetTimerTickCount();

    for (;;) {
        if (g_IrqWake) {
            if (FpgaIrqService() != 0) {
                AL_LOG(AL_LOG_LEVEL_NOTICE, "S3 IRQ SERVICE FAIL\r\n");
                return -1;
            }
        }

        Events = g_EventFlags;
        if (Events != 0U) {
            g_EventFlags &= ~Events;
            if (Events & IRQ_FAULT) {
                Fault = 0U;
                (AL_VOID)FpgaRead(APB_FAULT_STATUS, &Fault);
                AL_LOG(AL_LOG_LEVEL_NOTICE,
                       "S3 FAULT LATCHED: 0x%08x; reset required for demo recovery\r\n",
                       (unsigned int)Fault);
            }
        }

        if ((AlSys_GetTimerTickCount() - LastHeartbeat) >= HeartbeatPeriod) {
            if (FpgaWrite(APB_HEARTBEAT, ++Heartbeat) != 0) {
                AL_LOG(AL_LOG_LEVEL_NOTICE, "S3 HEARTBEAT WRITE FAIL\r\n");
                return -1;
            }
            LastHeartbeat += HeartbeatPeriod;
        }
    }
}
