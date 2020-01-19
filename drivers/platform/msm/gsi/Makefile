obj-$(CONFIG_GSI) += gsi.o gsidbg.o

ifneq ($(CONFIG_DEBUG_FS),y)
gsidbg-$(CONFIG_GSI) += gsi_dbg.o

CFLAGS_gsi.o += -DCONFIG_DEBUG_FS
CFLAGS_gsi_dbg.o += -DCONFIG_DEBUG_FS
CFLAGS_gsidbg.o += -DCONFIG_DEBUG_FS
else
gsidbg-$(CONFIG_DEBUG_FS) += gsi_dbg.o
endif

obj-$(CONFIG_IPA_EMULATION) += gsi_emulation.o
