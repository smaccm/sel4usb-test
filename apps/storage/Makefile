#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := storage.bin

ENTRY_POINT := _sel4_start

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
ASMFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.S))
OFILES   := archive.o

# Libraries required to build the target
LIBS = c sel4 sel4muslcsys sel4vka sel4allocman \
       platsupport sel4platsupport sel4vspace elf \
       sel4utils sel4simple utils  sel4simple-default cpio \
       usbdrivers sel4dma sel4sync pci

#CFLAGS += -Werror

include $(SEL4_COMMON)/common.mk

archive.o: ${COMPONENTS}
	$(Q)mkdir -p $(dir $@)
	${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive $^


