/* SPDX-License-Identifier: MIT */
/*
 * The resident SBI runtime: base, TIME and RFENCE, which is everything a single-hart kernel with
 * syscon poweroff needs. Everything else probes as unsupported. Timers forward the CLINT's
 * M-timer interrupt as a supervisor timer interrupt via mip.STIP, which this board leaves
 * writable from M-mode.
 */

#include "fw.h"

#define EID_BASE 0x10
#define EID_TIME 0x54494D45
#define EID_RFNC 0x52464E43

#define SBI_SUCCESS 0
#define SBI_ERR_NOT_SUPPORTED (-2l)

#define SBI_SPEC_VERSION ((1 << 24) | 0) /* v1.0 */
#define SBI_IMPL_ID 0x53 /* unregistered; "S" for Sedna */
#define SBI_IMPL_VERSION 1

#define MCAUSE_INTERRUPT (1ul << 63)
#define MCAUSE_M_TIMER (MCAUSE_INTERRUPT | 7)
#define MCAUSE_ECALL_S 9

#define MIP_STIP (1ul << 5)

/* Register indices in the trap frame start.S builds. */
#define FRAME_A0 8
#define FRAME_A7 15

void sbi_init(void) {
    mmio_write64(board.clint + CLINT_MTIMECMP, ~0ul);
    csr_write(mie, 0);
}

static u64 handle_base(const u64 function, const u64 arg0, long *error) {
    switch (function) {
        case 0:
            return SBI_SPEC_VERSION;
        case 1:
            return SBI_IMPL_ID;
        case 2:
            return SBI_IMPL_VERSION;
        case 3:
            return arg0 == EID_BASE || arg0 == EID_TIME || arg0 == EID_RFNC ? 1 : 0;
        case 4:
            return csr_read(mvendorid);
        case 5:
            return csr_read(marchid);
        case 6:
            return csr_read(mimpid);
        default:
            *error = SBI_ERR_NOT_SUPPORTED;
            return 0;
    }
}

static void handle_ecall(u64 *frame) {
    const u64 extension = frame[FRAME_A7];
    const u64 function = frame[FRAME_A0 + 6];
    const u64 arg0 = frame[FRAME_A0];
    long error = SBI_SUCCESS;
    u64 value = 0;

    switch (extension) {
        case EID_BASE:
            value = handle_base(function, arg0, &error);
            break;
        case EID_TIME:
            if (function == 0) {
                mmio_write64(board.clint + CLINT_MTIMECMP, arg0);
                csr_clear(mip, MIP_STIP);
                csr_set(mie, MIE_MTIE);
            } else {
                error = SBI_ERR_NOT_SUPPORTED;
            }
            break;
        case EID_RFNC:
            switch (function) {
                case 0:
                    asm volatile("fence.i" ::: "memory");
                    break;
                case 1:
                case 2:
                    asm volatile("sfence.vma" ::: "memory");
                    break;
                default:
                    error = SBI_ERR_NOT_SUPPORTED;
                    break;
            }
            break;
        default:
            error = SBI_ERR_NOT_SUPPORTED;
            break;
    }

    frame[FRAME_A0] = (u64) error;
    frame[FRAME_A0 + 1] = value;
}

void handle_trap(u64 *frame) {
    const u64 cause = csr_read(mcause);
    if (cause == MCAUSE_M_TIMER) {
        /* Forward to S; MTIE stays off until the next set_timer or it would fire forever. */
        csr_clear(mie, MIE_MTIE);
        csr_set(mip, MIP_STIP);
        return;
    }
    if (cause == MCAUSE_ECALL_S) {
        handle_ecall(frame);
        csr_write(mepc, csr_read(mepc) + 4); /* ecall is never compressed */
        return;
    }

    uart_puts("\r\nsedna-fw: unexpected trap, mcause=");
    uart_puthex(cause);
    uart_puts(" mepc=");
    uart_puthex(csr_read(mepc));
    uart_puts("\r\n");
    for (;;) {
        asm volatile("wfi");
    }
}
