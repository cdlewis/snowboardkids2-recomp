#include "patches.h"
#include "misc_funcs.h"

int dummydata = 0;
int dummybss;

int dummyfunc(void) {
    return 0;
}

extern OSMesgQueue gPiDmaMsgQueue;
extern s32 gFrameBufferFlags[];
extern s32 gFrameBufferCounters[];
extern s32 gBufferedFrameCounter;

RECOMP_PATCH void dmaLoadAndInvalidate(void* romStart, void* romEnd, void* ramStart, void* icacheStart, void* icacheEnd,
                                       void* dcacheStart, void* dcacheEnd, void* bssStart, void* bssEnd) {
    OSIoMesg dmaMessage;
    void* dummyMessage;
    u32 remainingBytes;
    void* currentRomOffset;
    u32 currentChunkSize;
    void* currentRamDest;

    // Zero out BSS or other region if requested
    if (bssEnd != bssStart) {
        bzero(bssStart, bssEnd - bssStart);
    }

    // @recomp Load the overlay in the recomp runtime.
    recomp_load_overlays((u32) romStart, ramStart, romEnd - romStart);

    // Invalidate instruction and data caches for specified ranges
    // osInvalICache(icacheStart, icacheEnd - icacheStart);
    // osInvalDCache(dcacheStart, dcacheEnd - dcacheStart);

    remainingBytes = romEnd - romStart;
    currentRomOffset = romStart;
    currentRamDest = ramStart;

    while (remainingBytes > 0) {
        currentChunkSize = remainingBytes;

        // Cap the transfer size to 0x1000 bytes (4KB)
        if (remainingBytes > 0x1000) {
            currentChunkSize = 0x1000;
        }

        osPiStartDma(&dmaMessage, OS_MESG_PRI_NORMAL, OS_READ, (u32) currentRomOffset, currentRamDest, currentChunkSize,
                     &gPiDmaMsgQueue);
        osRecvMesg(&gPiDmaMsgQueue, &dummyMessage, OS_MESG_BLOCK);

        currentRomOffset += currentChunkSize;
        currentRamDest += currentChunkSize;
        remainingBytes -= currentChunkSize;
    }
}

#if 0
RECOMP_PATCH void handleFrameBufferComplete(s32 bufferIndex) {
    s32 index = bufferIndex & 0xF;
    gFrameBufferFlags[index] = 0;
    gBufferedFrameCounter = gFrameBufferCounters[index];
    recomp_printf("index = %d\n", index);
}
#endif

/* Reimplement initDisplayBuffers without the per-buffer gDPFullSync.
 * The original DL ends with gDPFullSync + gSPEndDisplayList; under RT64 LLE
 * each fullsync is a workload boundary that triggers a buffer swap, so the
 * init task's sync collides with the merged graphics task's sync (see
 * graphics_patch.c) and produces flicker. We drop it; the trailing
 * gSPEndDisplayList still terminates the DL cleanly. */

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define MEMORY_HEAP_SIZE 0x200000
#define gMemoryHeapEnd (gMemoryHeapBase + MEMORY_HEAP_SIZE)

typedef union {
    u16 data[SCREEN_HEIGHT * SCREEN_WIDTH];
    u16 array[SCREEN_HEIGHT][SCREEN_WIDTH];
} FrameBuffer; // size = 0x25800

extern FrameBuffer gFrameBuffer;
extern FrameBuffer gAuxFrameBuffers[3];
extern void *gDisplayBufferMsgs;
extern void initLinearArenaRegions(void);
extern void initLinearAllocator(void);
extern void initGraphicsArenas(void);
extern void initGraphicsSystem(void);
extern s32 gFrameBufferFlags[];
extern s32 gFrameCounter;
extern s32 gBufferedFrameCounter;
extern s32 gFrameSkipCounter;
extern u32 __additional_scanline_0;
extern u8 gDisplayFramePending;
extern void *gDramStack;
extern void *gOutputBuffer;
extern void *gYieldBuffer;
extern long long int rspbootTextStart[];
extern long long int aspMainTextStart[];
extern Gfx gDefaultRenderDisplayList[];
extern s32 microcodeGroups[];

typedef struct {
    /* 0x00 */ u32 type;
    /* 0x04 */ u32 flags;
    /* 0x08 */ void *ucode_boot;
    /* 0x0C */ u32 ucode_boot_size;
    /* 0x10 */ void *ucode;
    /* 0x14 */ u32 ucode_size;
    /* 0x18 */ void *output_buff_size;
    /* 0x1C */ u32 ucode_data_size;
    /* 0x20 */ void *ucode_data;
    /* 0x24 */ u32 dram_stack_size;
    /* 0x28 */ void *dram_stack;
    /* 0x2C */ u32 task_2C;
    /* 0x30 */ void *data_ptr;
    /* 0x34 */ u32 data_size;
    /* 0x38 */ void *output_buff;
    /* 0x3C */ u32 yield_data_size;
    /* 0x40 */ u32 pad40[2];
    /* 0x48 */ void *yield_data_ptr;
    /* 0x4C */ u16 unk4C;
    /* 0x4E */ u16 unk4E;
    /* 0x50 */ Gfx displayList[15];
    u32 pad[34];
} DisplayBufferMsg;

extern void *gDisplayBufferMsgs;
void *allocateMemoryNode(s32, u32, u8 *);

RECOMP_PATCH void initDisplayBuffers(void) __attribute__((optnone)) {
    DisplayBufferMsg *msg;
    u8 exists;
    s32 i;
    Gfx *gfx;

    initLinearArenaRegions();
    initLinearAllocator();
    initGraphicsArenas();

    gDramStack = allocateMemoryNode(0, 0x400, &exists);
    gOutputBuffer = allocateMemoryNode(0, 0x10000, &exists);
    gYieldBuffer = allocateMemoryNode(0, 0xC00, &exists);
    initGraphicsSystem();

    gFrameBufferFlags[0] = 0;
    gFrameBufferFlags[1] = 0;
    gFrameCounter = 1;
    gBufferedFrameCounter = 0;
    gFrameSkipCounter = 0;
    __additional_scanline_0 = 0;
    gDisplayFramePending = 0;

    gDisplayBufferMsgs = msg = allocateMemoryNode(0, 3*sizeof(DisplayBufferMsg), &exists);

    for (i = 0; i < 3; msg++, i++) {
        gfx = msg->displayList;
        gSPSegment(gfx++, 0, 0);
        gSPDisplayList(gfx++, gDefaultRenderDisplayList);
        gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        gDPSetCycleType(gfx++, G_CYC_FILL);
        gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, &gFrameBuffer);
        gDPSetFillColor(gfx++, 0xFFFCFFFC);
        gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
        gDPPipeSync(gfx++);
        gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, &gAuxFrameBuffers[i]);
        gDPSetFillColor(gfx++, 0x10001);
        gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
        gDPPipeSync(gfx++);
        gSPEndDisplayList(gfx++);

        msg->yield_data_ptr = &gAuxFrameBuffers[i];
        msg->unk4C = 0;
        msg->unk4E = 2;

        msg->data_ptr = msg->displayList;
        msg->data_size = 0x70;
        msg->type = 1;
        msg->flags = 0;
        msg->ucode_boot = rspbootTextStart;
        
        msg->ucode_boot_size = (u32) aspMainTextStart;
        msg->ucode_boot_size = msg->ucode_boot_size - ((u32) rspbootTextStart);
        
        msg->ucode = (void*)microcodeGroups[2];
        msg->output_buff_size = (void*)microcodeGroups[3];
        msg->ucode_data_size = 0x800;
        msg->ucode_data = gDramStack;
        msg->dram_stack_size = 0x400;
        msg->dram_stack = gOutputBuffer;
        msg->task_2C = (u32)msg->dram_stack + 0x10000;
        msg->output_buff = gYieldBuffer;
        msg->yield_data_size = 0xC00;
    }
}