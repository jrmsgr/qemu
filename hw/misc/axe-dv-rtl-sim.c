
// clang-format off
#include "qemu/osdep.h" // Must be at the top
#include "hw/core/cpu.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "qemu/typedefs.h"
#include "system/system.h"
#include "trace.h"
#include "qemu/notify.h"
#include "qom/object.h"
#include "exec/memattrs.h"
#include "hw/misc/axe-dv-rtl-sim.h"
#include "multisim_client.h"
#include "trace/trace-hw_misc.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// clang-format on

OBJECT_DECLARE_SIMPLE_TYPE(AxeDvRtlSim, AXE_DV_RTL_SIM)

#define MULTISIM_NAME_MAX 1024
#define MULTISIM_CMD_SERVER_NAME "rw_cmd"
#define MULTISIM_RSP_SERVER_NAME "rw_rsp"
#define MULTISIM_EXIT_SERVER_NAME "exit"
#define MULTISIM_INTERRUPT_SERVER_NAME "interrupts"
#define MULTISIM_CMD_READ 0x1
#define MULTISIM_CMD_WRITE 0x0
#define MULTISIM_MEM_WRITE_SUCCESS 0x0
#define AXI_CMD_UI64_LEN 3
#define AXI_RSP_UI64_LEN 2
#define TO_BITS(n) (8*n)
#define AXI_CMD_BIT_LEN (64*AXI_CMD_UI64_LEN)
#define AXI_RSP_BIT_LEN (64*AXI_RSP_UI64_LEN)
#define AXI_OKAY 0
#define AXI_EXOKAY 1
#define AXI_DEC_ERR 3
#define EXIT_CMD_BIT_LEN (64*AXI_RSP_UI64_LEN)
// Max number of interrupt lines that can be instantiated
// SiFive's PLIC supports up to 520 but this makes tracing more complicated
#define MAX_IRQ_NUMBER 64

struct AxeDvRtlSim {
  SysBusDevice parent_obj;

  MemoryRegion iomem;
  char *name;
  char* multisim_dir;
  char* multisim_server_prefix;
  char* multisim_cmd_server;
  char* multisim_rsp_server;
  char* multisim_exit_server;
  char* multisim_interrupt_server;
  QemuThread irq_thread;
  uint64_t  size;
  Notifier  exit_notifier;
  qemu_irq* irqs;
  uint64_t  irq_number;
};

static inline MemTxResult axe_dv_rtl_sim_axi_resp_to_memtxresult(uint64_t code) {
    MemTxResult ret;
    switch (code) {
        case AXI_DEC_ERR:
            ret = MEMTX_DECODE_ERROR;
            break;
        case AXI_OKAY:
        case AXI_EXOKAY:
            ret = MEMTX_OK;
            break;
        default:
            ret = MEMTX_ERROR;
            break;
    }

    return ret;
}

static MemTxResult axe_dv_rtl_sim_read_with_attrs(void *opaque, hwaddr addr,
                                                  uint64_t *data, unsigned size,
                                                  MemTxAttrs attrs) {

  AxeDvRtlSim *s = AXE_DV_RTL_SIM(opaque);
  uint64_t cmd_payload[AXI_CMD_UI64_LEN] = {((uint64_t)TO_BITS(size) << 32) | MULTISIM_CMD_READ, addr, 0x0};
  uint64_t rsp_payload[AXI_CMD_UI64_LEN] = {0};
  int result;
  MemTxResult ret = MEMTX_OK;

  result = multisim_client_push(s->multisim_cmd_server, (data_handle_t)cmd_payload, AXI_CMD_BIT_LEN);
  if (result != MULTISIM_SUCCESS) {
      ret = MEMTX_ERROR;
      trace_axe_dv_rtl_sim_read(addr, size, *data, ret);
      return ret;
  }

  result = multisim_client_pull(s->multisim_rsp_server, (data_handle_t)rsp_payload, AXI_RSP_BIT_LEN);
  if (result != MULTISIM_SUCCESS) {
      ret = MEMTX_ERROR;
      trace_axe_dv_rtl_sim_read(addr, size, *data, ret);
      return ret;
  }

  *data = rsp_payload[1];
  ret = axe_dv_rtl_sim_axi_resp_to_memtxresult(rsp_payload[0]);

  trace_axe_dv_rtl_sim_read(addr, size, *data, ret);
  return ret;
}

static MemTxResult axe_dv_rtl_sim_write_with_attrs(void *opaque, hwaddr addr,
                                                   uint64_t data, unsigned size,
                                                   MemTxAttrs attrs) {
  AxeDvRtlSim *s = AXE_DV_RTL_SIM(opaque);
  uint64_t cmd_payload[AXI_CMD_UI64_LEN] = {((uint64_t)TO_BITS(size)<< 32) | MULTISIM_CMD_WRITE, addr, data};
  uint64_t rsp_payload[AXI_CMD_UI64_LEN] = {0};
  int result;
  MemTxResult ret = MEMTX_OK;

  result = multisim_client_push(s->multisim_cmd_server, (data_handle_t)cmd_payload, AXI_CMD_BIT_LEN);
  if (result != MULTISIM_SUCCESS) {
      ret = MEMTX_ERROR;
      trace_axe_dv_rtl_sim_read(addr, size, data, ret);
      return ret;
  }

  result = multisim_client_pull(s->multisim_rsp_server, (data_handle_t)rsp_payload, AXI_RSP_BIT_LEN);
  if (result != MULTISIM_SUCCESS) {
      ret = MEMTX_ERROR;
      trace_axe_dv_rtl_sim_read(addr, size, data, ret);
      return ret;
  }

  ret = axe_dv_rtl_sim_axi_resp_to_memtxresult(rsp_payload[0]);

  trace_axe_dv_rtl_sim_write(addr, size, data, ret);
  return ret;
}

static const MemoryRegionOps axe_dv_rtl_sim_ops = {
    .read_with_attrs = axe_dv_rtl_sim_read_with_attrs,
    .write_with_attrs = axe_dv_rtl_sim_write_with_attrs,
    .endianness = DEVICE_LITTLE_ENDIAN,
    // QEMU defaults to min=1 byte and max=4 bytes if no size is specified
    .impl.max_access_size = 8,
    .impl.min_access_size = 1
};

static void axe_dv_rtl_sim_exit_notifier(Notifier* notifier, void* data) {
    AxeDvRtlSim* s = container_of(notifier, AxeDvRtlSim, exit_notifier);
    const uint32_t exit_request = 0x1;
    // TODO: Check command length (might be incorrect)
    multisim_client_push(s->multisim_exit_server, (data_handle_t)&exit_request, EXIT_CMD_BIT_LEN);
    trace_axe_dv_rtl_sim_exit();
};

static void *axe_dv_rtl_sim_irq_thread(void* opaque) {
    AxeDvRtlSim* s = opaque;
    uint64_t irq_status = 0;
    while (1) {
        multisim_client_pull(s->multisim_interrupt_server, (data_handle_t)&irq_status, s->irq_number);
        trace_axe_dv_rtl_sim_irq_status(irq_status);
        /*
         * This runs in a dedicated thread, so the BQL must be held while
         * driving the IRQ line: qemu_irq_raise()/lower() propagates through
         * the PLIC down to cpu_interrupt(), which is not thread-safe.
         */
        bql_lock();
        for (uint64_t i = 0; i<s->irq_number; i++) {
            if (irq_status & ((uint64_t)1<<i)) {
                qemu_irq_raise(s->irqs[i]);
            } else {
                qemu_irq_lower(s->irqs[i]);
            }
        }
        bql_unlock();
    }
    return NULL;
}

static void axe_dv_rtl_sim_realize(DeviceState *dev, Error **errp) {
  AxeDvRtlSim *s = AXE_DV_RTL_SIM(dev);
  SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

  if (s->name == NULL) {
    s->name = g_strdup("axe-dv-rtl-sim");
  }

  memory_region_init_io(&s->iomem, OBJECT(s), &axe_dv_rtl_sim_ops, s, s->name,
                        s->size);

  s->multisim_dir = getcwd(NULL, 0);
  if (s->multisim_dir == NULL) {
      error_setg(errp, "Could not get the current dir\n");
      return;
  }

  if (s->multisim_server_prefix == NULL) {
      error_setg(errp, "multisim-server-prefix is not set\n");
      return;
  }

  if ((strlen(s->multisim_server_prefix)+strlen(MULTISIM_CMD_SERVER_NAME)) > MULTISIM_NAME_MAX) {
      error_setg(errp, "multisim-server-prefix is too long\n");
      return;
  }
  s->multisim_cmd_server = g_malloc0(MULTISIM_NAME_MAX);
  s->multisim_rsp_server = g_malloc0(MULTISIM_NAME_MAX);
  s->multisim_exit_server = g_malloc0(MULTISIM_NAME_MAX);
  s->multisim_interrupt_server = g_malloc0(MULTISIM_NAME_MAX);
  if ((sprintf(s->multisim_cmd_server, "%s_%s", s->multisim_server_prefix, MULTISIM_CMD_SERVER_NAME) < 0) ||
      (sprintf(s->multisim_rsp_server, "%s_%s", s->multisim_server_prefix, MULTISIM_RSP_SERVER_NAME) < 0) ||
      (sprintf(s->multisim_exit_server, "%s_%s", s->multisim_server_prefix, MULTISIM_EXIT_SERVER_NAME) < 0) ||
      (sprintf(s->multisim_interrupt_server, "%s_%s", s->multisim_server_prefix, MULTISIM_INTERRUPT_SERVER_NAME) < 0)) {
      error_setg(errp, "failed to create server names\n");
      return;
  }

  multisim_client_start(s->multisim_dir, s->multisim_cmd_server);
  multisim_client_start(s->multisim_dir, s->multisim_rsp_server);
  multisim_client_start(s->multisim_dir, s->multisim_exit_server);
  multisim_client_start(s->multisim_dir, s->multisim_interrupt_server);

  trace_axe_dv_rtl_sim_connection_done();

  sysbus_init_mmio(sbd, &s->iomem);

  s->exit_notifier.notify = axe_dv_rtl_sim_exit_notifier;
  qemu_add_exit_notifier(&s->exit_notifier);

  if (s->irq_number > 0) {
    if (s->irq_number > MAX_IRQ_NUMBER) {
        error_setg(errp, "too many IRQs requested: %lu. Max %d are supported\n", s->irq_number, MAX_IRQ_NUMBER);
        return;
    }
    s->irqs = g_malloc0(s->irq_number*sizeof(qemu_irq));
    for (uint64_t i=0; i<s->irq_number; i++) {
        sysbus_init_irq(sbd, &s->irqs[i]);
    }
    qdev_init_gpio_out(dev, s->irqs, s->irq_number);
    qemu_thread_create(&s->irq_thread, "axe-dv-rtl-sim-interrupt-thread", axe_dv_rtl_sim_irq_thread,
                       s, QEMU_THREAD_JOINABLE);
  }
}

static void axe_dv_rtl_sim_unrealize(DeviceState* dev) {
    AxeDvRtlSim *s = AXE_DV_RTL_SIM(dev);
    free(s->multisim_dir);
    g_free(s->multisim_cmd_server);
    g_free(s->multisim_rsp_server);
    g_free(s->multisim_exit_server);
    g_free(s->multisim_interrupt_server);
    if (s->irq_number > 0) {
        g_free(s->irqs);
    }
}

static const Property axe_dv_rtl_sim_properties[] = {
    DEFINE_PROP_STRING("name", AxeDvRtlSim, name),
    DEFINE_PROP_UINT64("size", AxeDvRtlSim, size, 0),
    DEFINE_PROP_STRING("multisim-server-prefix", AxeDvRtlSim, multisim_server_prefix),
    DEFINE_PROP_UINT64("irq-number", AxeDvRtlSim, irq_number, 0),
};

static void axe_dv_rtl_sim_class_init(ObjectClass *klass, const void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);

  dc->realize = axe_dv_rtl_sim_realize;
  dc->unrealize = axe_dv_rtl_sim_unrealize;
  device_class_set_props(dc, axe_dv_rtl_sim_properties);
  set_bit(DEVICE_CATEGORY_MISC, dc->categories);
  dc->desc = "RTL sim adapter by Axelera";
}

static const TypeInfo axe_dv_rtl_sim_info = {
    .name = TYPE_AXE_DV_RTL_SIM, 
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AxeDvRtlSim),
    .class_init = axe_dv_rtl_sim_class_init,
};

static void axe_dv_register_types(void) {
  type_register_static(&axe_dv_rtl_sim_info);
}

type_init(axe_dv_register_types)
