/* SPDX-License-Identifier: MIT */

#include "fw.h"

/* The RISC-V Linux Image header, at the start of the loaded file. */
#define IMAGE_TEXT_OFFSET 8
#define IMAGE_MAGIC2_OFFSET 56
#define IMAGE_MAGIC2 0x05435352

#define MSTATUS_MPP_S (1ul << 11)
#define MSTATUS_MPP_MASK (3ul << 11)

#define MEDELEG_S 0xB1FF /* all standard exceptions except environment calls from S and M */
#define MIDELEG_S 0x222 /* SSIP, STIP, SEIP */
#define MCOUNTEREN_ALL 0x7

#define RESCAN_INTERVAL 10000000ul /* timer ticks between passes over the block devices */

#define NO_MEMORY "sedna-fw: not enough memory to load the kernel\r\n"

struct board_info board;

void enter_supervisor(u64 entry, void *dtb);

static int try_boot(const u64 base) {
    if (blk_init(base) != 0) {
        return -1;
    }
    if (ext2_open(base) != 0) {
        blk_reset(base);
        return -1;
    }

    const u64 load = board.ram_base + KERNEL_OFFSET;
    if ((u64) board.dtb <= load) {
        uart_puts(NO_MEMORY);
        blk_reset(base);
        return -1;
    }
    /* Keep clear of the device tree at the top of memory. */
    const u64 room = (u64) board.dtb - load;
    const long size = ext2_load(base, "boot", "Image", (void *) load, room);
    if (size == EXT2_TOO_LARGE) {
        uart_puts(NO_MEMORY);
        blk_reset(base);
        return -1;
    }
    if (size < (long) (IMAGE_MAGIC2_OFFSET + 4)) {
        blk_reset(base);
        return -1;
    }

    if (*(const u32 *) (load + IMAGE_MAGIC2_OFFSET) != IMAGE_MAGIC2
        || *(const u64 *) (load + IMAGE_TEXT_OFFSET) != KERNEL_OFFSET) {
        uart_puts("sedna-fw: /boot/Image is not a kernel for this machine\r\n");
        blk_reset(base);
        return -1;
    }

    blk_reset(base);

    sbi_init();
    csr_write(medeleg, MEDELEG_S);
    csr_write(mideleg, MIDELEG_S);
    csr_write(mcounteren, MCOUNTEREN_ALL);
    csr_clear(mstatus, MSTATUS_MPP_MASK);
    csr_set(mstatus, MSTATUS_MPP_S);

    asm volatile("fence.i" ::: "memory");
    enter_supervisor(load, board.dtb);
    return -1; /* unreachable */
}

/*
 * Sleeps until the timer fires. mstatus.MIE is still clear, so no trap is taken and execution
 * resumes right here; without this an unbootable machine would spin at full emulated speed.
 */
static void nap(void) {
    mmio_write64(board.clint + CLINT_MTIMECMP, mmio_read64(board.clint + CLINT_MTIME) + RESCAN_INTERVAL);
    csr_set(mie, MIE_MTIE);
    asm volatile("wfi");
    csr_clear(mie, MIE_MTIE);
    mmio_write64(board.clint + CLINT_MTIMECMP, ~0ul);
}

void fw_main(void *dtb) {
    if (fdt_scan(dtb) != 0) {
        for (;;) {
            asm volatile("wfi");
        }
    }

    /* Rescan forever: on this board, block device media can appear while we wait. */
    int warned = 0;
    for (;;) {
        for (int i = 0; i < board.blk_count; i++) {
            try_boot(board.blk[i]);
        }
        if (!warned) {
            uart_puts("sedna-fw: no bootable disk\r\n");
            warned = 1;
        }
        nap();
    }
}
