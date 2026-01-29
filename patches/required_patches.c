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

#if 0
extern Node_70B00 D_800A3370_A3F70;
extern u8 gDisplayFramePending;
extern s32 gCurrentDoubleBufferIndex;
extern s32 gFrameCounter;
extern s32 gCurrentDisplayBufferIndex;
extern void* gDisplayBufferMsgs;

void sendMessageToThreadSyncQueue(OSMesg message);

RECOMP_PATCH void processDisplayFrameUpdate(void) {
    Node_70B00* node;
    Node_70B00* temp;

    temp = D_800A3370_A3F70.list3_next;
    gDisplayFramePending = 0;
    if (temp == NULL) {
        temp = &D_800A3370_A3F70;
    }
    node = temp;
    if (node != NULL) {
        do {
            if (node->frameCallbackMsg != 0) {
                gFrameBufferFlags[gCurrentDoubleBufferIndex] = 1;
                sendMessageToThreadSyncQueue((OSMesg) node->frameCallbackMsg);
            }
            node = node->list3_next;
        } while (node != NULL);
    }
    gFrameCounter = (gFrameCounter + 1) & 0x0FFFFFFF;
    gCurrentDoubleBufferIndex = (gCurrentDoubleBufferIndex + 1) & 1;
    gCurrentDisplayBufferIndex = gCurrentDisplayBufferIndex + 1;
    if (gCurrentDisplayBufferIndex >= 3) {
        gCurrentDisplayBufferIndex = 0;
    }
    sendMessageToThreadSyncQueue((OSMesg) ((u8*) gDisplayBufferMsgs + (gCurrentDisplayBufferIndex * 0x150)));
}
#endif

#if 0

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
extern s32 D_8009AFD0_9BBD0;
extern u32 __additional_scanline_0;
extern u8 gDisplayFramePending;
extern void *D_800A3360_A3F60;
extern void *D_800A3364_A3F64;
extern void *D_800A3368_A3F68;
extern long long int rspbootTextStart[];
extern long long int aspMainTextStart[];
extern Gfx D_8009AED0_9BAD0[];
extern s32 D_8009AFE0_9BBE0[];

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

RECOMP_PATCH void func_8006DC40_6E840(void) __attribute__((optnone)) {
    DisplayBufferMsg *msg;
    u8 exists;
    s32 i;
    Gfx *gfx;

    initLinearArenaRegions();
    initLinearAllocator();
    initGraphicsArenas();

    D_800A3360_A3F60 = allocateMemoryNode(0, 0x400, &exists);
    D_800A3364_A3F64 = allocateMemoryNode(0, 0x10000, &exists);
    D_800A3368_A3F68 = allocateMemoryNode(0, 0xC00, &exists);
    initGraphicsSystem();

    gFrameBufferFlags[0] = 0;
    gFrameBufferFlags[1] = 0;
    gFrameCounter = 1;
    gBufferedFrameCounter = 0;
    D_8009AFD0_9BBD0 = 0;
    __additional_scanline_0 = 0;
    gDisplayFramePending = 0;

    gDisplayBufferMsgs = msg = allocateMemoryNode(0, 3*sizeof(DisplayBufferMsg), &exists);

    for (i = 0; i < 3; msg++, i++) {
        gfx = msg->displayList;
        gSPSegment(gfx++, 0, 0);
        gSPDisplayList(gfx++, D_8009AED0_9BAD0);
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
        gDPFullSync(gfx++);
        gSPEndDisplayList(gfx++);

        msg->yield_data_ptr = &gAuxFrameBuffers[i];
        msg->unk4C = 0;
        msg->unk4E = 2;

        msg->data_ptr = msg->displayList;
        msg->data_size = 0x78;
        msg->type = 1;
        msg->flags = 0;
        msg->ucode_boot = rspbootTextStart;
        
        msg->ucode_boot_size = (u32) aspMainTextStart;
        msg->ucode_boot_size = msg->ucode_boot_size - ((u32) rspbootTextStart);
        
        msg->ucode = (void*)D_8009AFE0_9BBE0[0];
        msg->output_buff_size = (void*)D_8009AFE0_9BBE0[1];
        msg->ucode_data_size = 0x800;
        msg->ucode_data = D_800A3360_A3F60;
        msg->dram_stack_size = 0x400;
        msg->dram_stack = D_800A3364_A3F64;
        msg->task_2C = (u32)msg->dram_stack + 0x10000;
        msg->output_buff = D_800A3368_A3F68;
        msg->yield_data_size = 0xC00;
    }
}
#endif