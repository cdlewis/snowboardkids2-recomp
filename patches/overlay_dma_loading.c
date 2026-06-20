#include "patches.h"

extern void recomp_load_overlays(u32 rom, void* ram, u32 size);

extern OSMesgQueue gPiDmaMsgQueue;

// @recomp Work around a recompiler bug where it'll crash if no .data section is present.
int gDummyValue = 1;

static void clearMemory(void* start, u32 size) {
    u8* bytes = start;

    for (u32 i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

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
        clearMemory(bssStart, bssEnd - bssStart);
    }

    // @recomp Load the overlay in the recomp runtime.
    recomp_load_overlays((u32) romStart, ramStart, romEnd - romStart);

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
