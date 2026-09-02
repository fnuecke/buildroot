/* SPDX-License-Identifier: MIT */
/* Polled 16550 output for progress and errors. Silently does nothing without a console. */

#include "fw.h"

#define LSR 5
#define LSR_THRE 0x20

void uart_putc(const char c) {
    if (board.uart == 0) {
        return;
    }
    while ((*(volatile u8 *) (board.uart + LSR) & LSR_THRE) == 0) {
    }
    *(volatile u8 *) board.uart = (u8) c;
}

void uart_puts(const char *s) {
    for (; *s; s++) {
        uart_putc(*s);
    }
}

void uart_puthex(const u64 value) {
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc("0123456789abcdef"[(value >> shift) & 0xF]);
    }
}
