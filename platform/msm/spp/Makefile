#
# Makefile for the spp driver
#

#csrspp-objs	:= csrspp_tty.o
obj-m += csrspp_tty.o

all:
	@echo "---------------------------"
	@echo "CC  : $(CC)"
	@echo "LD  : $(LD)"
	@echo "KDIR: $(KDIR)"
	@echo "---------------------------"
	make CC=$(CC) LD=$(LD) -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` clean

