
// clang-format off
// Include order matters!!!
#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/empty_slot.h"
#include "qapi/error.h"
#include "trace.h"
#include "qom/object.h"
#include "exec/memattrs.h"
#include <stdio.h>
#include "hw/misc/axe-dv-rtl-sim.h"
// clang-format on

#define MULTISIM_SERVER_NAME "multisim"

OBJECT_DECLARE_SIMPLE_TYPE(AxeDvRtlSim, AXE_DV_RTL_SIM)

struct AxeDvRtlSim {
  SysBusDevice parent_obj;

  MemoryRegion iomem;
  char *name;
  char* server_file;
  uint64_t size;
};

static MemTxResult axe_dv_rtl_sim_read_with_attrs(void *opaque, hwaddr addr,
                                                  uint64_t *data, unsigned size,
                                                  MemTxAttrs attrs) {

  MemTxResult result = MEMTX_DECODE_ERROR;
  printf("Read at address 0x%lx with size 0x%x\n", addr, size);

  return result;
}

static MemTxResult axe_dv_rtl_sim_write_with_attrs(void *opaque, hwaddr addr,
                                                   uint64_t data, unsigned size,
                                                   MemTxAttrs attrs) {
  MemTxResult result = MEMTX_DECODE_ERROR;

  return result;
}

static const MemoryRegionOps axe_dv_rtl_sim_ops = {
    .read_with_attrs = axe_dv_rtl_sim_read_with_attrs,
    .write_with_attrs = axe_dv_rtl_sim_write_with_attrs,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void axe_dv_rtl_sim_realize(DeviceState *dev, Error **errp) {
  AxeDvRtlSim *s = AXE_DV_RTL_SIM(dev);

  if (s->name == NULL) {
    s->name = g_strdup("axe-dv-rtl-sim");
  }

  memory_region_init_io(&s->iomem, OBJECT(s), &axe_dv_rtl_sim_ops, s, s->name,
                        s->size);

  sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const Property axe_dv_rtl_sim_properties[] = {
    DEFINE_PROP_STRING("name", AxeDvRtlSim, name),
    DEFINE_PROP_UINT64("size", AxeDvRtlSim, size, 0),
    DEFINE_PROP_STRING("server-file", AxeDvRtlSim, server_file)
};

static void axe_dv_rtl_sim_class_init(ObjectClass *klass, const void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);

  dc->realize = axe_dv_rtl_sim_realize;
  device_class_set_props(dc, axe_dv_rtl_sim_properties);
  set_bit(DEVICE_CATEGORY_MISC, dc->categories);
  dc->desc = "RTL sim adapter by Axelera";
}

// clang-format off
static const TypeInfo axe_dv_rtl_sim_info = {
    .name = TYPE_AXE_DV_RTL_SIM, 
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AxeDvRtlSim),
    .class_init = axe_dv_rtl_sim_class_init,
};
// clang-format on

static void empty_slot_register_types(void) {
  type_register_static(&axe_dv_rtl_sim_info);
}

type_init(empty_slot_register_types)
