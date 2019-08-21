#
# Makefile for the spp tty driver
#

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m := csrspp-tty.o
csrspp-tty-objs := csrspp_tty.o

M ?= $(shell pwd)

all:
	@echo "KDIR: $(KERNEL_SRC)"
	make -C $(KERNEL_SRC) M=$(M) modules

modules_install:
	make -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	make -C $(KERNEL_SRC) M=$(M) clean

