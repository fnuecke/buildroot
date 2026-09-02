/* SPDX-License-Identifier: MIT */
/*
 * Minimal M-mode firmware for the Sedna RISC-V board.
 *
 * Two jobs: load /boot/Image from the first bootable virtio block device into memory and enter it
 * in S-mode, then stay resident as the SBI runtime (base, TIME, RFENCE) the kernel needs. Every
 * address comes from the device tree the board passes in a1; nothing but the link address is
 * baked in.
 */
#ifndef FW_H
#define FW_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

#define KERNEL_OFFSET 0x200000ul /* where the kernel Image expects to sit, relative to RAM base */
#define SCRATCH_OFFSET 0x100000ul /* boot-time metadata buffers, free memory once the kernel runs */

#define MAX_BLOCK_DEVICES 8

#define CLINT_MTIMECMP 0x4000
#define CLINT_MTIME 0xBFF8
#define MIE_MTIE (1ul << 7)

struct board_info {
    void *dtb;
    u64 dtb_size;
    u64 uart;
    u64 clint;
    u64 ram_base;
    u64 ram_size;
    u64 blk[MAX_BLOCK_DEVICES];
    int blk_count;
};

extern struct board_info board;

/* fdt.c */
int fdt_scan(void *dtb);

/* uart.c */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(u64 value);

/* virtio.c */
int blk_present(u64 base);
int blk_init(u64 base);
int blk_read(u64 base, u64 sector, void *dst, u32 len);
void blk_reset(u64 base);

/* ext2.c */
#define EXT2_TOO_LARGE (-2l) /* the file does not fit the space the caller offered */
int ext2_open(u64 base);
long ext2_load(u64 base, const char *dir, const char *name, void *dst, u64 max);

/* sbi.c */
void sbi_init(void);

/* lib.c */
void *memcpy(void *dst, const void *src, u64 n);
void *memset(void *dst, int value, u64 n);
int memcmp(const void *a, const void *b, u64 n);
int strcmp(const char *a, const char *b);

static inline u32 be32(const void *p) {
    const u8 *b = p;
    return ((u32) b[0] << 24) | ((u32) b[1] << 16) | ((u32) b[2] << 8) | b[3];
}

static inline u64 be64(const void *p) {
    return ((u64) be32(p) << 32) | be32((const u8 *) p + 4);
}

static inline u32 mmio_read32(u64 address) {
    return *(volatile u32 *) address;
}

static inline u64 mmio_read64(u64 address) {
    return *(volatile u64 *) address;
}

static inline void mmio_write32(u64 address, u32 value) {
    *(volatile u32 *) address = value;
}

static inline void mmio_write64(u64 address, u64 value) {
    *(volatile u64 *) address = value;
}

#define csr_read(csr) ({ u64 v; asm volatile("csrr %0, " #csr : "=r"(v)); v; })
#define csr_write(csr, value) asm volatile("csrw " #csr ", %0" :: "r"(value))
#define csr_set(csr, mask) asm volatile("csrs " #csr ", %0" :: "r"(mask))
#define csr_clear(csr, mask) asm volatile("csrc " #csr ", %0" :: "r"(mask))

#endif
