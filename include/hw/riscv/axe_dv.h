#ifndef HW_RISCV_SPIKE
#define HW_RISCV_SPIKE

#include "hw/riscv/spike.h"

#define AXE_DV_CPUS_MAX SPIKE_CPUS_MAX
#define AXE_DV_SOCKETS_MAX SPIKE_SOCKETS_MAX

#define TYPE_AXE_DV_MACHINE MACHINE_TYPE_NAME("axe_dv")
typedef SpikeState AxeDvState;
DECLARE_INSTANCE_CHECKER(AxeDvState, AXE_DV_MACHINE,
                         TYPE_AXE_DV_MACHINE)

enum {
    AXE_DV_MROM,
    AXE_DV_HTIF,
    AXE_DV_CLINT,
    AXE_DV_PLIC,
    AXE_DV_DRAM,
    AXE_DV_AXE_DV_RTL_SIM
};

/* PLIC layout (matches the SiFive PLIC used by the virt machine) */
#define AXE_DV_PLIC_NUM_PRIORITIES 7
#define AXE_DV_PLIC_PRIORITY_BASE  0x00
#define AXE_DV_PLIC_PENDING_BASE   0x1000
#define AXE_DV_PLIC_ENABLE_BASE    0x2000
#define AXE_DV_PLIC_ENABLE_STRIDE  0x80
#define AXE_DV_PLIC_CONTEXT_BASE   0x200000
#define AXE_DV_PLIC_CONTEXT_STRIDE 0x1000

/* PLIC interrupt sources. Source 0 is reserved by the PLIC spec. */
#define AXE_DV_RTL_SIM_IRQ   1
#define AXE_DV_PLIC_NUM_SOURCES (AXE_DV_RTL_SIM_IRQ + 1)

#endif
