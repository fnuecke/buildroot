/* SPDX-License-Identifier: MIT */
/*
 * Read-only ext2 walker, enough to find and load one file by path. Classic revision-1 layout
 * only: no extents, no 64-bit block numbers, which is what the rootfs images this firmware boots
 * are built with. Metadata buffers live in scratch memory outside the firmware's reserved
 * region; they are boot-time only.
 */

#include "fw.h"

#define SUPERBLOCK_OFFSET 1024
#define EXT2_MAGIC 0xEF53
#define ROOT_INODE 2
#define MAX_BLOCK_SIZE 4096
#define SECTOR_SIZE 512

/* A larger read unit than one filesystem block; contiguous file blocks coalesce into it. */
#define MAX_RUN (16 * MAX_BLOCK_SIZE)

struct superblock {
    u32 inodes_count;
    u32 blocks_count;
    u32 reserved_blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_frag_size;
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mnt_count;
    u16 max_mnt_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_rev_level;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 rev_level;
    u16 def_resuid;
    u16 def_resgid;
    u32 first_ino;
    u16 inode_size;
};

struct inode {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
};

struct dir_entry {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
    char name[];
};

static struct {
    u64 base;
    u32 block_size;
    u32 inodes_per_group;
    u32 inode_size;
    u32 first_data_block;
    u8 *group_descriptors;
    u8 *block_buffer;
    u8 *indirect1;
    u8 *indirect2;
    u32 indirect1_block;
    u32 indirect2_block;
    struct inode inode;
} fs;

static int read_block(const u32 block, void *dst) {
    return blk_read(fs.base, (u64) block * (fs.block_size / SECTOR_SIZE), dst, fs.block_size);
}

int ext2_open(const u64 base) {
    u8 *scratch = (u8 *) (board.ram_base + SCRATCH_OFFSET);
    u8 *superblock_buffer = scratch;
    fs.group_descriptors = scratch + MAX_BLOCK_SIZE;
    fs.block_buffer = scratch + 2 * MAX_BLOCK_SIZE;
    fs.indirect1 = scratch + 3 * MAX_BLOCK_SIZE;
    fs.indirect2 = scratch + 4 * MAX_BLOCK_SIZE;
    fs.indirect1_block = 0;
    fs.indirect2_block = 0;

    fs.base = base;
    if (blk_read(base, SUPERBLOCK_OFFSET / SECTOR_SIZE, superblock_buffer, 1024) != 0) {
        return -1;
    }

    const struct superblock *sb = (const struct superblock *) superblock_buffer;
    if (sb->magic != EXT2_MAGIC) {
        return -1;
    }

    fs.block_size = 1024u << sb->log_block_size;
    if (fs.block_size > MAX_BLOCK_SIZE) {
        return -1;
    }
    fs.inodes_per_group = sb->inodes_per_group;
    fs.inode_size = sb->rev_level >= 1 ? sb->inode_size : 128;
    fs.first_data_block = sb->first_data_block;

    return 0;
}

static int read_inode(const u32 number) {
    const u32 group = (number - 1) / fs.inodes_per_group;
    const u32 index = (number - 1) % fs.inodes_per_group;
    /* The group descriptor table starts in the block after the superblock and may span several
     * blocks; only the one holding this group is read. Descriptors are 32 bytes, with the inode
     * table block number at offset 8. */
    const u32 per_block = fs.block_size / 32;
    if (read_block(fs.first_data_block + 1 + group / per_block, fs.group_descriptors) != 0) {
        return -1;
    }
    const u32 table = *(const u32 *) (fs.group_descriptors + (group % per_block) * 32 + 8);
    const u32 offset = index * fs.inode_size;
    if (read_block(table + offset / fs.block_size, fs.block_buffer) != 0) {
        return -1;
    }
    memcpy(&fs.inode, fs.block_buffer + offset % fs.block_size, sizeof(fs.inode));
    return 0;
}

/* Maps a file block index to a disk block number; 0 is a hole. Negative on error. */
static long get_block(const u32 index) {
    const u32 per_block = fs.block_size / 4;
    if (index < 12) {
        return fs.inode.block[index];
    }
    u32 remaining = index - 12;
    if (remaining < per_block) {
        const u32 indirect = fs.inode.block[12];
        if (indirect == 0) {
            return 0;
        }
        if (fs.indirect1_block != indirect) {
            if (read_block(indirect, fs.indirect1) != 0) {
                return -1;
            }
            fs.indirect1_block = indirect;
        }
        return ((const u32 *) fs.indirect1)[remaining];
    }
    remaining -= per_block;
    if (remaining < (u64) per_block * per_block) {
        const u32 doubly = fs.inode.block[13];
        if (doubly == 0) {
            return 0;
        }
        if (fs.indirect2_block != doubly) {
            if (read_block(doubly, fs.indirect2) != 0) {
                return -1;
            }
            fs.indirect2_block = doubly;
        }
        const u32 indirect = ((const u32 *) fs.indirect2)[remaining / per_block];
        if (indirect == 0) {
            return 0;
        }
        if (fs.indirect1_block != indirect) {
            if (read_block(indirect, fs.indirect1) != 0) {
                return -1;
            }
            fs.indirect1_block = indirect;
        }
        return ((const u32 *) fs.indirect1)[remaining % per_block];
    }
    return -1; /* triple indirection; nothing this loads is that large */
}

/* Finds the named entry in the currently loaded directory inode. Zero if absent. */
static u32 dir_lookup(const char *name) {
    u32 name_length = 0;
    while (name[name_length] != 0) {
        name_length++;
    }

    const u32 block_count = (fs.inode.size + fs.block_size - 1) / fs.block_size;
    for (u32 i = 0; i < block_count; i++) {
        const long block = get_block(i);
        if (block < 0) {
            return 0;
        }
        if (block == 0 || read_block((u32) block, fs.block_buffer) != 0) {
            continue;
        }
        for (u32 offset = 0; offset + 8 <= fs.block_size;) {
            const struct dir_entry *entry = (const struct dir_entry *) (fs.block_buffer + offset);
            if (entry->rec_len < 8) {
                break;
            }
            if (entry->inode != 0 && entry->name_len == name_length
                && memcmp(entry->name, name, name_length) == 0) {
                return entry->inode;
            }
            offset += entry->rec_len;
        }
    }
    return 0;
}

/*
 * Loads /dir/name to dst, coalescing contiguous blocks into large reads. Returns the file size,
 * or negative if the file is absent, too large, or unreadable.
 */
long ext2_load(const u64 base, const char *dir, const char *name, void *dst, const u64 max) {
    (void) base;
    if (read_inode(ROOT_INODE) != 0) {
        return -1;
    }
    const u32 dir_inode = dir_lookup(dir);
    if (dir_inode == 0 || read_inode(dir_inode) != 0) {
        return -1;
    }
    const u32 file_inode = dir_lookup(name);
    if (file_inode == 0 || read_inode(file_inode) != 0) {
        return -1;
    }

    const u64 size = fs.inode.size;
    const u32 block_count = (u32) ((size + fs.block_size - 1) / fs.block_size);
    if ((u64) block_count * fs.block_size > max) { /* whole blocks are written, so round up */
        return EXT2_TOO_LARGE;
    }
    u8 *out = dst;
    u32 run_start_index = 0;
    u32 run_start_block = 0;
    u32 run_length = 0;
    for (u32 i = 0; i <= block_count; i++) {
        long block = -1;
        if (i < block_count) {
            block = get_block(i);
            if (block < 0) {
                return -1;
            }
        }

        const int extends_run = run_length > 0 && block > 0
            && (u32) block == run_start_block + run_length
            && (u64) (run_length + 1) * fs.block_size <= MAX_RUN;
        if (extends_run) {
            run_length++;
            continue;
        }

        if (run_length > 0) {
            if (blk_read(fs.base, (u64) run_start_block * (fs.block_size / SECTOR_SIZE),
                out + (u64) run_start_index * fs.block_size,
                run_length * fs.block_size) != 0) {
                return -1;
            }
            run_length = 0;
        }

        if (i < block_count) {
            if (block == 0) {
                memset(out + (u64) i * fs.block_size, 0, fs.block_size);
            } else {
                run_start_index = i;
                run_start_block = (u32) block;
                run_length = 1;
            }
        }
    }

    return (long) size;
}
