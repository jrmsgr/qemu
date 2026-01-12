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
    AXE_DV_DRAM,
    AXE_DV_AXE_DV_RTL_SIM
};

#endif
