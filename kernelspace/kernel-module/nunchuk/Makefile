obj-m := nunchuk.o

ARCH = arm
CROSS_COMPILE = arm-bootlin-linux-gnueabihf-

KDIR := /home/as/Desktop/linuxEmbeddedBBB/bootlin/linux-kernel/linux-kernel-bbb-labs/src/linux

all:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KDIR} M=$$PWD

clean:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KDIR} M=$$PWD clean

install:
	sudo cp nunchuk.ko /home/as/Desktop/linuxEmbeddedBBB/bbb-screen/nfsroot/root/