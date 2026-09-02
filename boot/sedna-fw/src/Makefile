# SPDX-License-Identifier: MIT

CROSS_COMPILE ?= riscv64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
NM := $(CROSS_COMPILE)nm

CFLAGS := -Os -Wall -Wextra -ffreestanding -nostdlib -fno-stack-protector -fno-pic \
	-mcmodel=medany -mno-relax -march=rv64imac_zicsr_zifencei -mabi=lp64
LDFLAGS := -static -nostdlib -Wl,--build-id=none -T fw.ld

OBJS := start.o fw.o fdt.o virtio.o ext2.o uart.o sbi.o lib.o

all: firmware.bin firmware.properties

firmware.bin: firmware.elf
	$(OBJCOPY) -O binary $< $@

firmware.properties: firmware.elf
	printf 'regionSize=%d\n' 0x$$($(NM) $< | awk '$$3 == "_fw_region_size" { print $$1 }') > $@

firmware.elf: $(OBJS) fw.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c fw.h Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) firmware.elf firmware.bin firmware.properties

.PHONY: all clean
