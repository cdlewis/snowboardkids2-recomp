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