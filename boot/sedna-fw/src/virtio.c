/* SPDX-License-Identifier: MIT */
/*
 * Polled virtio-blk over MMIO v2, just enough to read sectors: one queue, one request at a time.
 * The device is left reset after use so the kernel's driver finds it untouched, with no stale
 * interrupt pending.
 */

#include "fw.h"

#define REG_MAGIC 0x000
#define REG_VERSION 0x004
#define REG_DEVICE_ID 0x008
#define REG_DEVICE_FEATURES 0x010
#define REG_DEVICE_FEATURES_SEL 0x014
#define REG_DRIVER_FEATURES 0x020
#define REG_DRIVER_FEATURES_SEL 0x024
#define REG_QUEUE_SEL 0x030
#define REG_QUEUE_SIZE_MAX 0x034
#define REG_QUEUE_SIZE 0x038
#define REG_QUEUE_READY 0x044
#define REG_QUEUE_NOTIFY 0x050
#define REG_INTERRUPT_STATUS 0x060
#define REG_INTERRUPT_ACK 0x064
#define REG_STATUS 0x070
#define REG_QUEUE_DESC_LOW 0x080
#define REG_QUEUE_DESC_HIGH 0x084
#define REG_QUEUE_DRIVER_LOW 0x090
#define REG_QUEUE_DRIVER_HIGH 0x094
#define REG_QUEUE_DEVICE_LOW 0x0A0
#define REG_QUEUE_DEVICE_HIGH 0x0A4

#define MAGIC 0x74726976
#define DEVICE_ID_BLOCK 2

#define REG_CONFIG 0x100 /* virtio-blk: capacity in 512-byte sectors, le64 */

#define STATUS_ACKNOWLEDGE 1
#define STATUS_DRIVER 2
#define STATUS_DRIVER_OK 4
#define STATUS_FEATURES_OK 8
#define STATUS_DEVICE_NEEDS_RESET 64

#define FEATURE_VERSION_1 (1u << 0) /* bit 32, selected via features-sel 1 */

#define DESC_F_NEXT 1
#define DESC_F_WRITE 2

#define REQUEST_TYPE_IN 0

#define QUEUE_SIZE 4

struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

struct virtq {
    struct virtq_desc desc[QUEUE_SIZE] __attribute__((aligned(16)));
    struct {
        u16 flags;
        u16 idx;
        u16 ring[QUEUE_SIZE];
    } avail __attribute__((aligned(2)));
    struct {
        u16 flags;
        u16 idx;
        struct {
            u32 id;
            u32 len;
        } ring[QUEUE_SIZE];
    } used __attribute__((aligned(4)));
};

struct blk_request {
    u32 type;
    u32 reserved;
    u64 sector;
};

static struct virtq queue;
static struct blk_request request;
static volatile u8 request_status;
static u16 used_index;

/* Every virtio device shares the "virtio,mmio" compatible string, so the scan filters by ID. */
int blk_present(const u64 base) {
    return mmio_read32(base + REG_MAGIC) == MAGIC
        && mmio_read32(base + REG_VERSION) == 2
        && mmio_read32(base + REG_DEVICE_ID) == DEVICE_ID_BLOCK;
}

int blk_init(const u64 base) {
    if (!blk_present(base)) {
        return -1;
    }

    mmio_write32(base + REG_STATUS, 0);
    mmio_write32(base + REG_STATUS, STATUS_ACKNOWLEDGE);
    mmio_write32(base + REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER);

    mmio_write32(base + REG_DEVICE_FEATURES_SEL, 1);
    if ((mmio_read32(base + REG_DEVICE_FEATURES) & FEATURE_VERSION_1) == 0) {
        mmio_write32(base + REG_STATUS, 0);
        return -1;
    }
    mmio_write32(base + REG_DRIVER_FEATURES_SEL, 1);
    mmio_write32(base + REG_DRIVER_FEATURES, FEATURE_VERSION_1);
    mmio_write32(base + REG_DRIVER_FEATURES_SEL, 0);
    mmio_write32(base + REG_DRIVER_FEATURES, 0);

    mmio_write32(base + REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK);
    if ((mmio_read32(base + REG_STATUS) & STATUS_FEATURES_OK) == 0) {
        mmio_write32(base + REG_STATUS, 0);
        return -1;
    }

    memset(&queue, 0, sizeof(queue));
    used_index = 0;

    mmio_write32(base + REG_QUEUE_SEL, 0);
    if (mmio_read32(base + REG_QUEUE_SIZE_MAX) < QUEUE_SIZE) {
        mmio_write32(base + REG_STATUS, 0);
        return -1;
    }
    mmio_write32(base + REG_QUEUE_SIZE, QUEUE_SIZE);
    mmio_write32(base + REG_QUEUE_DESC_LOW, (u32) (u64) queue.desc);
    mmio_write32(base + REG_QUEUE_DESC_HIGH, (u32) ((u64) queue.desc >> 32));
    mmio_write32(base + REG_QUEUE_DRIVER_LOW, (u32) (u64) &queue.avail);
    mmio_write32(base + REG_QUEUE_DRIVER_HIGH, (u32) ((u64) &queue.avail >> 32));
    mmio_write32(base + REG_QUEUE_DEVICE_LOW, (u32) (u64) &queue.used);
    mmio_write32(base + REG_QUEUE_DEVICE_HIGH, (u32) ((u64) &queue.used >> 32));
    mmio_write32(base + REG_QUEUE_READY, 1);

    mmio_write32(base + REG_STATUS,
        STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK);

    /* An empty drive reports zero capacity, and reads past its end quietly succeed without
     * touching the destination, which would leave stale metadata looking valid. */
    const u64 capacity = mmio_read32(base + REG_CONFIG)
        | ((u64) mmio_read32(base + REG_CONFIG + 4) << 32);
    if (capacity == 0) {
        mmio_write32(base + REG_STATUS, 0);
        return -1;
    }

    return 0;
}

/* Reads len bytes (a multiple of 512) from the given 512-byte sector. Blocks until done. */
int blk_read(const u64 base, const u64 sector, void *dst, const u32 len) {
    request.type = REQUEST_TYPE_IN;
    request.reserved = 0;
    request.sector = sector;
    request_status = 0xFF;

    queue.desc[0].addr = (u64) &request;
    queue.desc[0].len = sizeof(request);
    queue.desc[0].flags = DESC_F_NEXT;
    queue.desc[0].next = 1;
    queue.desc[1].addr = (u64) dst;
    queue.desc[1].len = len;
    queue.desc[1].flags = DESC_F_NEXT | DESC_F_WRITE;
    queue.desc[1].next = 2;
    queue.desc[2].addr = (u64) &request_status;
    queue.desc[2].len = 1;
    queue.desc[2].flags = DESC_F_WRITE;
    queue.desc[2].next = 0;

    queue.avail.ring[queue.avail.idx % QUEUE_SIZE] = 0;
    asm volatile("fence w, w" ::: "memory");
    queue.avail.idx++;
    asm volatile("fence w, o" ::: "memory");
    mmio_write32(base + REG_QUEUE_NOTIFY, 0);

    while (*(volatile u16 *) &queue.used.idx == used_index) {
        if (mmio_read32(base + REG_STATUS) & STATUS_DEVICE_NEEDS_RESET) {
            return -1; /* the device failed the request and will never complete it */
        }
    }
    used_index++;
    asm volatile("fence r, r" ::: "memory");

    const u32 pending = mmio_read32(base + REG_INTERRUPT_STATUS);
    if (pending != 0) {
        mmio_write32(base + REG_INTERRUPT_ACK, pending);
    }

    return request_status == 0 ? 0 : -1;
}

void blk_reset(const u64 base) {
    mmio_write32(base + REG_STATUS, 0);
}
