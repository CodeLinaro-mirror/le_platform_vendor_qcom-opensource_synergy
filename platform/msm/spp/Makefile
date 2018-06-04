#
# Makefile for the spp tty driver
#

obj-m := csrspp-tty.o
csrspp-tty-objs := csrspp_tty.o

all:
	@echo "KDIR: $(KDIR)"
	make -C $(KDIR) M=`pwd` modules

modules_install:
	make -C $(KDIR) M=`pwd` modules_install

clean:
	make -C $(KDIR) M=`pwd` clean

