##
## Makefile for Miosix embedded OS
##

## Path to kernel/config directories (edited by init_project_out_of_git_repo.pl)
KPATH := miosix
CONFPATH := $(KPATH)
MAKEFILE_VERSION := 1.14
include $(KPATH)/Makefile.kcommon

##
## List here your source files (both .s, .c and .cpp)
##
SRC := main.cpp

##
## List here additional include directories (in the form -Iinclude_dir)
##
INCLUDE_DIRS :=

##
## List here additional static libraries with relative path
##
LIBS :=

##
## List here subdirectories which contains makefiles
##
SUBDIRS +=

##
## Attach a romfs filesystem image after the kernel
##
ROMFS_DIR :=

all: $(if $(ROMFS_DIR), image, main)

main: $(OBJ) all-recursive
	$(ECHO) "[LD  ] main.elf"
	$(Q)$(CXX) $(LFLAGS) -o main.elf $(OBJ) $(LINK_LIBS)
	$(ECHO) "[CP  ] main.hex"
	$(Q)$(CP) -O ihex   main.elf main.hex
	$(ECHO) "[CP  ] main.bin"
	$(Q)$(CP) -O binary main.elf main.bin
	$(Q)$(SZ) main.elf

clean: clean-recursive
	$(Q)rm -f $(OBJ) $(OBJ:.o=.d) main.elf main.hex main.bin main.map

-include $(OBJ:.o=.d)
