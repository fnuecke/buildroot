/* SPDX-License-Identifier: MIT */

#include "fw.h"

/* The RISC-V Linux Image header, at the start of the loaded file. */
#define IMAGE_TEXT_OFFSET 8
#define IMAGE_SIZE_OFFSET 16
#define IMAGE_MAGIC2_OFFSET 56
#define IMAGE_MAGIC2 0x05435352

#define MSTATUS_MPP_S (1ul << 11)
#define MSTATUS_MPP_MASK (3ul << 11)

#define MEDELEG_S 0xB1FF /* all standard exceptions except environment calls from S and M */
#define MIDELEG_S 0x222 /* SSIP, STIP, SEIP */
#define MCOUNTEREN_ALL 0x7

#define RESCAN_INTERVAL 10000000ul /* timer ticks between passes over the block devices */

#define NOT_A_KERNEL 1
#define NO_MEMORY 2
#define NO_DISK 4

struct board_info board;

/* The firmware rescans every second, a message is only printed the first time. */
static void report_once(const int problem) {
    static int reported;
    if (reported & problem) {
        return;
    }
    reported |= problem;
    uart_puts(problem == NOT_A_KERNEL ? "sedna-fw: /boot/Image is not a kernel for this machine\r\n"
        : problem == NO_MEMORY ? "sedna-fw: not enough memory to load the kernel\r\n"
        : "sedna-fw: no bootable disk\r\n");
}

extern u8 _fw_stack_top[]; /* the end of the resident firmware region */

void enter_supervisor(u64 entry, void *dtb);

static int try_boot(const u64 base) {
    if (blk_init(base) != 0) {
        return -1;
    }
    if (ext2_open(base) != 0) {
        blk_reset(base);
        return -1;
    }

    /* The header contains the kernel target location, so it is read before loading. */
    const long file_size = ext2_find("boot", "Image");
    const u8 *header = file_size >= (long) (IMAGE_MAGIC2_OFFSET + 4) ? ext2_first_block() : 0;
    if (header == 0) {
        blk_reset(base);
        return -1;
    }
    const u64 text_offset = *(const u64 *) (header + IMAGE_TEXT_OFFSET);
    const u64 image_size = *(const u64 *) (header + IMAGE_SIZE_OFFSET);
    const u64 load = board.ram_base + text_offset;
    if (*(const u32 *) (header + IMAGE_MAGIC2_OFFSET) != IMAGE_MAGIC2
        || (text_offset & (PAGE_SIZE - 1)) != 0 || load < (u64) _fw_stack_top) {
        report_once(NOT_A_KERNEL);
        blk_reset(base);
        return -1;
    }
    /* Keep clear of the scratch buffers and the device tree above them. The file lacks bss;
     * image_size is the full size once running. */
    if (load >= board.scratch || image_size > board.scratch - load) {
        report_once(NO_MEMORY);
        blk_reset(base);
        return -1;
    }
    const long size = ext2_load((void *) load, board.scratch - load);
    if (size == EXT2_TOO_LARGE) {
        report_once(NO_MEMORY);
    }
    if (size < 0) {
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
    board.scratch = (((u64) board.dtb) & ~(PAGE_SIZE - 1)) - SCRATCH_SIZE;
    if (board.scratch < (u64) _fw_stack_top) {
        report_once(NO_MEMORY); /* no room for the scratch buffers below the device tree */
        for (;;) {
            nap();
        }
    }

    /* Rescan forever: on this board, block device media can appear while we wait. */
    for (;;) {
        for (int i = 0; i < board.blk_count; i++) {
            try_boot(board.blk[i]);
        }
        report_once(NO_DISK);
        nap();
    }
}
