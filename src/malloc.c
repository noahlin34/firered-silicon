#include "global.h"
#include "platform/metrics.h"

static void *sHeapStart;
static u32 sHeapSize;

static EWRAM_DATA struct MemBlock *head = NULL;
static EWRAM_DATA struct MemBlock *pos = NULL;
static EWRAM_DATA struct MemBlock *splitBlock = NULL;

#define MALLOC_SYSTEM_ID 0xA3A3

struct MemBlock {
    // Whether this block is currently allocated.
    bool16 flag;

    // Magic number used for error checking. Should equal MALLOC_SYSTEM_ID.
    u16 magic_number;

    // Size of the block (not including this header struct).
    u32 size;

    // Previous block pointer. Equals sHeapStart if this is the first block.
    struct MemBlock *prev;

    // Next block pointer. Equals sHeapStart if this is the last block.
    struct MemBlock *next;

    // Data in the memory block. (Arrays of length 0 are a GNU extension.)
    u8 data[0];
};

void PutMemBlockHeader(void *block, struct MemBlock *prev, struct MemBlock *next, u32 size)
{
    struct MemBlock *header = (struct MemBlock *)block;

    header->flag = FALSE;
    header->magic_number = MALLOC_SYSTEM_ID;
    header->size = size;
    header->prev = prev;
    header->next = next;
}

void PutFirstMemBlockHeader(void *block, u32 size)
{
    PutMemBlockHeader(block, (struct MemBlock *)block, (struct MemBlock *)block, size - sizeof(struct MemBlock));
}

void *AllocInternal(void *heapStart, u32 size)
{
    u32 foundBlockSize;

    head = (struct MemBlock *)heapStart;
    pos = head;

    // Alignment
    if (size & 3)
        size = 4 * ((size / 4) + 1);

    for (;;) {
        // Loop through the blocks looking for unused block that's big enough.

        if (!pos->flag) {
            foundBlockSize = pos->size;

            if (foundBlockSize >= size) {
                if (foundBlockSize - size < 2 * sizeof(struct MemBlock)) {
                    // The block isn't much bigger than the requested size,
                    // so just use it.
                    pos->flag = TRUE;
                    return pos->data;
                } else {
                    // The block is significantly bigger than the requested
                    // size, so split the rest into a separate block.
                    int splitBlockSize = foundBlockSize;
                    splitBlockSize -= sizeof(struct MemBlock);
                    splitBlockSize -= size;

                    splitBlock = (struct MemBlock *)(pos->data + size);

                    pos->flag = TRUE;
                    pos->size = size;

                    PutMemBlockHeader(splitBlock, pos, pos->next, splitBlockSize);

                    pos->next = splitBlock;

                    if (splitBlock->next != head)
                        splitBlock->next->prev = splitBlock;
                    return pos->data;
                }
            }
        }

        if (pos->next == head)
        {
            AGB_ASSERT_EX(0, ABSPATH("gflib/malloc.c"), 174);
            return NULL;
        }

        pos = pos->next;
    }
}

void FreeInternal(void *heapStart, void *p)
{
    AGB_ASSERT_EX(p != NULL, ABSPATH("gflib/malloc.c"), 195);

    if (p) {
        struct MemBlock *head = (struct MemBlock *)heapStart;
        struct MemBlock *pos = (struct MemBlock *)((u8 *)p - sizeof(struct MemBlock));
        AGB_ASSERT_EX(pos->magic_number == MALLOC_SYSTEM_ID, ABSPATH("gflib/malloc.c"), 204);
        AGB_ASSERT_EX(pos->flag == TRUE, ABSPATH("gflib/malloc.c"), 205);
        pos->flag = FALSE;

        // If the freed block isn't the last one, merge with the next block
        // if it's not in use.
        if (pos->next != head) {
            if (!pos->next->flag) {
                AGB_ASSERT_EX(pos->next->magic_number == MALLOC_SYSTEM_ID, ABSPATH("gflib/malloc.c"), 211);
                pos->size += sizeof(struct MemBlock) + pos->next->size;
                pos->next->magic_number = 0;
                pos->next = pos->next->next;
                if (pos->next != head)
                    pos->next->prev = pos;
            }
        }

        // If the freed block isn't the first one, merge with the previous block
        // if it's not in use.
        if (pos != head) {
            if (!pos->prev->flag) {
                AGB_ASSERT_EX(pos->prev->magic_number == MALLOC_SYSTEM_ID, ABSPATH("gflib/malloc.c"), 228);

                pos->prev->next = pos->next;

                if (pos->next != head)
                    pos->next->prev = pos->prev;

                pos->magic_number = 0;
                pos->prev->size += sizeof(struct MemBlock) + pos->size;
            }
        }
    }
}

void *AllocZeroedInternal(void *heapStart, u32 size)
{
    void *mem = AllocInternal(heapStart, size);

    if (mem != NULL) {
        if (size & 3)
            size = 4 * ((size / 4) + 1);

        CpuFill32(0, mem, size);
    }

    return mem;
}

bool32 CheckMemBlockInternal(void *heapStart, void *pointer)
{
    struct MemBlock *head = (struct MemBlock *)heapStart;
    struct MemBlock *block = (struct MemBlock *)((u8 *)pointer - sizeof(struct MemBlock));

    if (block->magic_number != MALLOC_SYSTEM_ID)
        return FALSE;

    if (block->next->magic_number != MALLOC_SYSTEM_ID)
        return FALSE;

    if (block->next != head && block->next->prev != block)
        return FALSE;

    if (block->prev->magic_number != MALLOC_SYSTEM_ID)
        return FALSE;

    if (block->prev != head && block->prev->next != block)
        return FALSE;

    if (block->next != head && block->next != (struct MemBlock *)(block->data + block->size))
        return FALSE;

    return TRUE;
}

void InitHeap(void *heapStart, u32 heapSize)
{
    sHeapStart = heapStart;
    sHeapSize = heapSize;
    PutFirstMemBlockHeader(heapStart, heapSize);
}

void *Alloc(u32 size)
{
    void *mem = AllocInternal(sHeapStart, size);

    Platform_MetricsAddCounter(METRICS_COUNTER_ALLOCS, 1);
    return mem;
}

void *AllocZeroed(u32 size)
{
    void *mem = AllocZeroedInternal(sHeapStart, size);

    Platform_MetricsAddCounter(METRICS_COUNTER_ALLOCS, 1);
    return mem;
}

void Free(void *pointer)
{
    Platform_MetricsAddCounter(METRICS_COUNTER_FREES, 1);
    FreeInternal(sHeapStart, pointer);
}

bool32 CheckMemBlock(void *pointer)
{
    return CheckMemBlockInternal(sHeapStart, pointer);
}

/* Heap gauges are sampled once per frame rather than per allocation: walking the
 * block list is O(blocks) and would dominate the allocator's own cost at the
 * call sites. The block struct is private to this file, which is why the walk
 * lives here and not in src/platform/metrics.c. */
void Platform_MetricsSampleHeap(void)
{
    struct MemBlock *head = (struct MemBlock *)sHeapStart;
    struct MemBlock *pos;
    uint32_t used = 0;
    uint32_t largestFree = 0;
    uint32_t blocks = 0;

    if (!Platform_MetricsIsEnabled() || sHeapStart == NULL)
        return;

    pos = head;
    do
    {
        blocks++;
        if (pos->flag)
            used += pos->size;
        else if (pos->size > largestFree)
            largestFree = pos->size;

        pos = pos->next;
    } while (pos != head && pos != NULL);

    Platform_MetricsSetGauge(METRICS_GAUGE_HEAP_USED, used);
    Platform_MetricsSetGauge(METRICS_GAUGE_HEAP_LARGEST_FREE, largestFree);
    Platform_MetricsSetGauge(METRICS_GAUGE_HEAP_BLOCKS, blocks);

    /* High-water mark: the module keeps no per-gauge history, so the peak is
     * tracked here from the run's own reading. */
    {
        static uint32_t sHeapHighWater;
        if (used > sHeapHighWater)
            sHeapHighWater = used;
        Platform_MetricsSetGauge(METRICS_GAUGE_HEAP_HIGH_WATER, sHeapHighWater);
    }
}

bool32 CheckHeap()
{
    struct MemBlock *pos = (struct MemBlock *)sHeapStart;

    do {
        if (!CheckMemBlockInternal(sHeapStart, pos->data))
            return FALSE;
        pos = pos->next;
    } while (pos != (struct MemBlock *)sHeapStart);

    return TRUE;
}
