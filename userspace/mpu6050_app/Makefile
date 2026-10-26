CC = arm-bootlin-linux-gnueabihf-gcc

SYSROOT_PATH = /home/as/Desktop/linuxEmbeddedBBB/bbb-screen/nfsroot
CFLAGS = --sysroot=$(SYSROOT_PATH)
LIBS = -liio

all:
	$(CC) $(CFLAGS) -o mpu_app main.c $(LIBS)

install:
	sudo cp mpu_app /home/as/Desktop/linuxEmbeddedBBB/bbb-screen/nfsroot/root