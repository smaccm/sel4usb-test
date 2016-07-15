#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

lib-dirs:=libs

# The main target we want to generate
all: app-images
	-cp images/storage-image-${ARCH}-${PLAT} images/capdl-loader-experimental-image-${ARCH}-${PLAT}
	-cp images/kernel-ia32-pc99 /srv/tftp/
	-cp images/storage-image-ia32-${PLAT} /srv/tftp/

-include .config

include tools/common/project.mk

.PHONY: tags
tags:
	@find apps kernel projects \( -name "*.h" -o -name "*.c" \) > list.txt
	@ctags-exuberant -L list.txt
	@rm list.txt
