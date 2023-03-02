##
## Makefile for Miosix embedded OS
##
MAKEFILE_VERSION := 1.10
## Path to kernel directory (edited by init_project_out_of_git_repo.pl)
KPATH := miosix
## Path to config directory (edited by init_project_out_of_git_repo.pl)
CONFPATH := $(KPATH)
include $(CONFPATH)/config/Makefile.inc

##
## List here subdirectories which contains makefiles
##
SUBDIRS := $(KPATH)

##
## List here your source files (both .s, .c and .cpp)
##
TINYUSB_SRC := \
    tinyusb/src/host/hub.c \
    tinyusb/src/host/usbh.c \
    tinyusb/src/portable/nordic/nrf5x/dcd_nrf5x.c \
    tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c \
    tinyusb/src/portable/valentyusb/eptri/dcd_eptri.c \
    tinyusb/src/portable/ehci/ehci.c \
    tinyusb/src/portable/bridgetek/ft9xx/dcd_ft9xx.c \
    tinyusb/src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
    tinyusb/src/portable/nxp/khci/dcd_khci.c \
    tinyusb/src/portable/nxp/khci/hcd_khci.c \
    tinyusb/src/portable/nxp/lpc17_40/dcd_lpc17_40.c \
    tinyusb/src/portable/nxp/lpc17_40/hcd_lpc17_40.c \
    tinyusb/src/portable/nxp/transdimension/hcd_transdimension.c \
    tinyusb/src/portable/nxp/transdimension/dcd_transdimension.c \
    tinyusb/src/portable/ohci/ohci.c \
    tinyusb/src/portable/wch/ch32v307/dcd_usbhs.c \
    tinyusb/src/portable/template/dcd_template.c \
    tinyusb/src/portable/mentor/musb/hcd_musb.c \
    tinyusb/src/portable/mentor/musb/dcd_musb.c \
    tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
    tinyusb/src/portable/dialog/da146xx/dcd_da146xx.c \
    tinyusb/src/portable/chipidea/ci_hs/dcd_ci_hs.c \
    tinyusb/src/portable/chipidea/ci_hs/hcd_ci_hs.c \
    tinyusb/src/portable/sunxi/dcd_sunxi_musb.c \
    tinyusb/src/portable/microchip/samx7x/dcd_samx7x.c \
    tinyusb/src/portable/microchip/samg/dcd_samg.c \
    tinyusb/src/portable/microchip/pic/dcd_pic.c \
    tinyusb/src/portable/microchip/pic32mz/dcd_pic32mz.c \
    tinyusb/src/portable/microchip/samd/dcd_samd.c \
    tinyusb/src/portable/nuvoton/nuc121/dcd_nuc121.c \
    tinyusb/src/portable/nuvoton/nuc120/dcd_nuc120.c \
    tinyusb/src/portable/nuvoton/nuc505/dcd_nuc505.c \
    tinyusb/src/portable/renesas/usba/dcd_usba.c \
    tinyusb/src/portable/renesas/usba/hcd_usba.c \
    tinyusb/src/portable/espressif/esp32sx/dcd_esp32sx.c \
    tinyusb/src/portable/raspberrypi/rp2040/rp2040_usb.c \
    tinyusb/src/portable/raspberrypi/rp2040/dcd_rp2040.c \
    tinyusb/src/portable/raspberrypi/rp2040/hcd_rp2040.c \
    tinyusb/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c \
    tinyusb/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c \
    tinyusb/src/portable/sony/cxd56/dcd_cxd56.c \
    tinyusb/src/portable/mindmotion/mm32/dcd_mm32f327x_otg.c \
    tinyusb/src/portable/ti/msp430x5xx/dcd_msp430x5xx.c \
    tinyusb/src/common/tusb_fifo.c \
    tinyusb/src/class/cdc/cdc_rndis_host.c \
    tinyusb/src/class/cdc/cdc_host.c \
    tinyusb/src/class/cdc/cdc_device.c \
    tinyusb/src/class/video/video_device.c \
    tinyusb/src/class/net/ncm_device.c \
    tinyusb/src/class/net/ecm_rndis_device.c \
    tinyusb/src/class/dfu/dfu_rt_device.c \
    tinyusb/src/class/dfu/dfu_device.c \
    tinyusb/src/class/midi/midi_device.c \
    tinyusb/src/class/usbtmc/usbtmc_device.c \
    tinyusb/src/class/bth/bth_device.c \
    tinyusb/src/class/audio/audio_device.c \
    tinyusb/src/class/msc/msc_host.c \
    tinyusb/src/class/msc/msc_device.c \
    tinyusb/src/class/hid/hid_device.c \
    tinyusb/src/class/hid/hid_host.c \
    tinyusb/src/class/vendor/vendor_host.c \
    tinyusb/src/class/vendor/vendor_device.c \
    tinyusb/src/device/usbd_control.c \
    tinyusb/src/device/usbd.c \
    tinyusb/src/tusb.c

SRC := main.cpp $(TINYUSB_SRC)

##
## List here additional static libraries with relative path
##
LIBS :=

##
## List here additional include directories (in the form -Iinclude_dir)
##
INCLUDE_DIRS := -Itinyusb/src -Itusb_cmsis

##############################################################################
## You should not need to modify anything below                             ##
##############################################################################

ifeq ("$(VERBOSE)","1")
Q := 
ECHO := @true
else
Q := @
ECHO := @echo
endif

## Replaces both "foo.cpp"-->"foo.o" and "foo.c"-->"foo.o"
OBJ := $(addsuffix .o, $(basename $(SRC)))

## Includes the miosix base directory for C/C++
## Always include CONFPATH first, as it overrides the config file location
CXXFLAGS := $(CXXFLAGS_BASE) -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC)  \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)      \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
CFLAGS   := $(CFLAGS_BASE)   -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC)  \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)      \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
AFLAGS   := $(AFLAGS_BASE)
LFLAGS   := $(LFLAGS_BASE)
DFLAGS   := -MMD -MP

## libmiosix.a is among stdlibs because needs to be within start/end group
STDLIBS  := -lmiosix -lstdc++ -lc -lm -lgcc -latomic
LINK_LIBS := $(LIBS) -L$(KPATH) -Wl,--start-group $(STDLIBS) -Wl,--end-group

all: all-recursive main

clean: clean-recursive clean-topdir

program:
	$(PROGRAM_CMDLINE)

all-recursive:
	$(foreach i,$(SUBDIRS),$(MAKE) -C $(i)                               \
	  KPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(KPATH))       \
	  CONFPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(CONFPATH)) \
	  || exit 1;)

clean-recursive:
	$(foreach i,$(SUBDIRS),$(MAKE) -C $(i)                               \
	  KPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(KPATH))       \
	  CONFPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(CONFPATH)) \
	  clean || exit 1;)

clean-topdir:
	-rm -f $(OBJ) main.elf main.hex main.bin main.map $(OBJ:.o=.d)

main: main.elf
	$(ECHO) "[CP  ] main.hex"
	$(Q)$(CP) -O ihex   main.elf main.hex
	$(ECHO) "[CP  ] main.bin"
	$(Q)$(CP) -O binary main.elf main.bin
	$(Q)$(SZ) main.elf

main.elf: $(OBJ) all-recursive
	$(ECHO) "[LD  ] main.elf"
	$(Q)$(CXX) $(LFLAGS) -o main.elf $(OBJ) $(KPATH)/$(BOOT_FILE) $(LINK_LIBS)

%.o: %.s
	$(ECHO) "[AS  ] $<"
	$(Q)$(AS)  $(AFLAGS) $< -o $@

%.o : %.c
	$(ECHO) "[CC  ] $<"
	$(Q)$(CC)  $(DFLAGS) $(CFLAGS) $< -o $@

%.o : %.cpp
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

#pull in dependecy info for existing .o files
-include $(OBJ:.o=.d)
