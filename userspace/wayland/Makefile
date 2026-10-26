CC = arm-bootlin-linux-gnueabihf-gcc
SYSROOT = /home/as/Desktop/linuxEmbeddedBBB/bbb-screen/nfsroot
INSTALL_DIR = $(SYSROOT)/root

CFLAGS ?= -Wall -Wextra -O2 --sysroot=$(SYSROOT)
LDFLAGS ?= --sysroot=$(SYSROOT) -lwayland-client -liio -lpthread -lm -lrt

WAYLAND_SCANNER ?= wayland-scanner
XDG_SHELL_XML ?= /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

TARGET = wayland_app
SRCS = wayland_app.c xdg-shell-protocol.c
OBJS = $(SRCS:.c=.o)
GEN_HEADERS = xdg-shell-client-protocol.h
GEN_SOURCES = xdg-shell-protocol.c

all: $(TARGET)

$(GEN_HEADERS): $(XDG_SHELL_XML)
	$(WAYLAND_SCANNER) client-header $< $@

$(GEN_SOURCES): $(XDG_SHELL_XML)
	$(WAYLAND_SCANNER) private-code $< $@

%.o: %.c $(GEN_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(GEN_SOURCES) $(GEN_HEADERS) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_DIR)/

clean:
	rm -f $(TARGET) $(OBJS) $(GEN_HEADERS) $(GEN_SOURCES)

.PHONY: all clean install
