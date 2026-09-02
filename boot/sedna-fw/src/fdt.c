/* SPDX-License-Identifier: MIT */
/*
 * Minimal flattened-device-tree scan: pulls the UART, CLINT, memory range and virtio block
 * device windows out of the blob the board hands over. This walks our own board's tree, so it
 * assumes two address and two size cells throughout, which the root and soc nodes declare.
 */

#include "fw.h"

#define FDT_MAGIC 0xD00DFEED
#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

struct fdt_header {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
};

/* What the props seen so far say about the node currently being walked. */
struct node_state {
    u64 reg;
    int has_reg;
    int is_uart;
    int is_clint;
    int is_virtio;
    int is_memory;
    u64 size;
};

static void compatible_matches(const char *list, const u32 len, struct node_state *node) {
    for (u32 i = 0; i < len;) {
        const char *entry = list + i;
        if (strcmp(entry, "virtio,mmio") == 0) {
            node->is_virtio = 1;
        } else if (strcmp(entry, "ns16550a") == 0 || strcmp(entry, "ns16550") == 0) {
            node->is_uart = 1;
        } else if (strcmp(entry, "riscv,clint0") == 0) {
            node->is_clint = 1;
        }
        while (i < len && list[i] != 0) {
            i++;
        }
        i++;
    }
}

static void commit(struct node_state *node) {
    if (!node->has_reg) {
        node->is_uart = node->is_clint = node->is_virtio = node->is_memory = 0;
        return;
    }
    if (node->is_memory && board.ram_size == 0) {
        board.ram_base = node->reg;
        board.ram_size = node->size;
    } else if (node->is_uart && board.uart == 0) {
        board.uart = node->reg;
    } else if (node->is_clint && board.clint == 0) {
        board.clint = node->reg;
    } else if (node->is_virtio && board.blk_count < MAX_BLOCK_DEVICES && blk_present(node->reg)) {
        board.blk[board.blk_count++] = node->reg;
    }
    node->has_reg = 0;
    node->is_uart = node->is_clint = node->is_virtio = node->is_memory = 0;
}

int fdt_scan(void *dtb) {
    const struct fdt_header *header = dtb;
    if (be32(&header->magic) != FDT_MAGIC) {
        return -1;
    }

    board.dtb = dtb;
    board.dtb_size = be32(&header->totalsize);

    const u8 *structure = (const u8 *) dtb + be32(&header->off_dt_struct);
    const char *strings = (const char *) dtb + be32(&header->off_dt_strings);

    struct node_state node = {0};
    u32 offset = 0;
    for (;;) {
        const u32 token = be32(structure + offset);
        offset += 4;
        switch (token) {
            case FDT_BEGIN_NODE: {
                commit(&node); /* the parent's props are complete once a child begins */
                const char *name = (const char *) (structure + offset);
                u32 length = 0;
                while (name[length] != 0) {
                    length++;
                }
                offset += (length + 4) & ~3u;
                break;
            }
            case FDT_END_NODE:
                commit(&node);
                break;
            case FDT_PROP: {
                const u32 length = be32(structure + offset);
                const u32 name_offset = be32(structure + offset + 4);
                const u8 *value = structure + offset + 8;
                offset += 8 + ((length + 3) & ~3u);

                const char *name = strings + name_offset;
                if (strcmp(name, "reg") == 0 && length >= 16) {
                    node.reg = be64(value);
                    node.size = be64(value + 8);
                    node.has_reg = 1;
                } else if (strcmp(name, "compatible") == 0) {
                    compatible_matches((const char *) value, length, &node);
                } else if (strcmp(name, "device_type") == 0 && length >= 7
                    && memcmp(value, "memory", 7) == 0) {
                    node.is_memory = 1;
                }
                break;
            }
            case FDT_NOP:
                break;
            case FDT_END:
            default:
                return board.ram_size != 0 ? 0 : -1;
        }
    }
}
