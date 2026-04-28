/*
 * Single-task graphics dispatch.
 *
 * RT64 does strange things (leading to visible screen flickering) if gDPFullSync is
 * called more than once per frame. This is entirely possible to accomplish. The
 * Snowboard Kids 2 developers probably did not realise that different microcode
 * can co-exist within one display list.
 */

#include "PR/ultratypes.h"
#include "PR/sptask.h"
#include "PR/os_message.h"
#include "PR/os_convert.h"
#include "PR/mbi.h"
#include "PR/gbi.h"
#include "PR/ucode.h"
#include "rt64_extended_gbi.h"

#define RECOMP_PATCH __attribute__((section(".recomp_patch")))

typedef struct ViewportNode {
    u8 _pad00[0x10];
    /* 0x10 */ struct ViewportNode* list3_next;
    u8 _pad14[0x88];
    /* 0x9C */ struct FrameCallbackMsg* frameCallbackMsg;
} ViewportNode;

typedef struct FrameCallbackMsg {
    /* 0x00 */ OSTask t;
    /* 0x40 */ OSMesgQueue* msgQueue;
    /* 0x44 */ s32 msgData;
    /* 0x48 */ void* auxBuffer;
    /* 0x4C */ u16 scanlineValue;
    /* 0x4E */ u16 taskFlags;
} FrameCallbackMsg;

extern ViewportNode gRootViewport;
extern s32 gFrameBufferFlags[];
extern s32 gCurrentDoubleBufferIndex;
extern s32 gCurrentDisplayBufferIndex;
extern s32 gFrameCounter;
extern u8 gDisplayFramePending;
extern void* gDisplayBufferMsgs;

void sendMessageToThreadSyncQueue(OSMesg message);
void* arenaAlloc16(s32 size);

RECOMP_PATCH void processDisplayFrameUpdate(void) {
    ViewportNode* node;
    FrameCallbackMsg* firstMsg = NULL;
    FrameCallbackMsg* lastMsg = NULL;
    s32 nextDisplayBufferIndex;
    FrameCallbackMsg* initMsg;
    Gfx* mergedDL;
    Gfx* gfx;

    gDisplayFramePending = 0;

    nextDisplayBufferIndex = gCurrentDisplayBufferIndex + 1;
    if (nextDisplayBufferIndex >= 3) {
        nextDisplayBufferIndex = 0;
    }
    initMsg = (FrameCallbackMsg*) ((u8*) gDisplayBufferMsgs + (nextDisplayBufferIndex * 0x150));

    mergedDL = (Gfx*) arenaAlloc16(48 * (s32) sizeof(Gfx));

    /* First pass: find firstMsg and lastMsg among groups with patched wrappers. */
    node = gRootViewport.list3_next;
    if (node == NULL) {
        node = &gRootViewport;
    }
    while (node != NULL) {
        FrameCallbackMsg* msg = node->frameCallbackMsg;
        if (msg != NULL) {
            Gfx* wrapper = (Gfx*) msg->t.t.data_ptr;
            if ((wrapper[1].words.w0 >> 24) == G_DL) {
                if (firstMsg == NULL) {
                    firstMsg = msg;
                }
                lastMsg = msg;
            }
        }
        node = node->list3_next;
    }

    if (firstMsg != NULL) {
        /* Build merged DL — INIT FIRST under F3DEX2, then graphics groups, then ONE
         * final gDPFullSync. Init's clear of gFrameBuffer thus happens BEFORE any
         * graphics renders into it; graphics' content survives to the final fullsync. */
        gfx = mergedDL;

        /* Enable RT64 extended commands for the merged task before any child
         * display list can emit gEX* commands. */
        gEXEnable(gfx++);

        /* Init runs first under the boot ucode (which we set below to F3DEX2). */
        gSPDisplayList(gfx++, initMsg->t.t.data_ptr);

        /* Each graphics group: swap to its microcode, re-init segment table, jump to draws. */
        node = gRootViewport.list3_next;
        if (node == NULL) {
            node = &gRootViewport;
        }
        while (node != NULL) {
            FrameCallbackMsg* msg = node->frameCallbackMsg;
            if (msg != NULL) {
                Gfx* wrapper = (Gfx*) msg->t.t.data_ptr;
                if ((wrapper[1].words.w0 >> 24) == G_DL) {
                    u32 drawsAddr = wrapper[1].words.w1;
                    gSPLoadUcode(gfx++, msg->t.t.ucode, msg->t.t.ucode_data);
                    gSPSegment(gfx++, 0, 0);
                    gSPDisplayList(gfx++, drawsAddr);
                }
            }
            node = node->list3_next;
        }

        /* Sole fullsync per frame. */
        gDPFullSync(gfx++);
        gSPEndDisplayList(gfx++);

        /* Override OSTask: boot under F3DEX2 (for init), data_ptr = our merged DL. */
        firstMsg->t.t.ucode = (u64*) gspF3DEX2_fifoTextStart;
        firstMsg->t.t.ucode_data = (u64*) gspF3DEX2_fifoDataStart;
        firstMsg->t.t.data_ptr = (u64*) mergedDL;
        firstMsg->t.t.data_size = (u32) ((u8*) gfx - (u8*) mergedDL);

        if (lastMsg != firstMsg) {
            firstMsg->taskFlags = lastMsg->taskFlags;
            firstMsg->msgQueue = lastMsg->msgQueue;
            firstMsg->msgData = lastMsg->msgData;
        }
        gFrameBufferFlags[gCurrentDoubleBufferIndex] = 1;
        sendMessageToThreadSyncQueue((OSMesg) firstMsg);
    } else {
        /* No graphics this frame — submit init alone like the original. */
        sendMessageToThreadSyncQueue((OSMesg) initMsg);
    }

    gFrameCounter = (gFrameCounter + 1) & 0x0FFFFFFF;
    gCurrentDoubleBufferIndex = (gCurrentDoubleBufferIndex + 1) & 1;
    gCurrentDisplayBufferIndex = nextDisplayBufferIndex;
}
