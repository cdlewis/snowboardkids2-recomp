/*
 * Single-task graphics dispatch.
 *
 * RT64 can only perform frame interpolation correctly when one gDPFullSync is
 * used per frame. Snowboard Kids 2 switches the graphics microcode between F3DEX
 * and S2DEX but instead using gSPLoadUcode to do so, it does a full sync and
 * sends a separate display list and task each time.
 *
 * The patch merges all of the generated display lists into one display list and
 * only issues a gDPFullSync at the very end of the frame.
 */

#include "PR/ultratypes.h"
#include "PR/sptask.h"
#include "PR/os_message.h"
#include "PR/os_convert.h"
#include "PR/mbi.h"
#include "PR/gbi.h"
#include "PR/ucode.h"
#include "core/buffers.h"
#include "graphics/graphics.h"
#include "rt64_extended_gbi.h"

#define RECOMP_PATCH __attribute__((section(".recomp_patch")))
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

extern ViewportNode gRootViewport;
extern FrameBuffer gAuxFrameBuffers[3];
extern s32 gFrameBufferFlags[];
extern s32 gCurrentDoubleBufferIndex;
extern s32 gCurrentDisplayBufferIndex;
extern s32 gFrameCounter;
extern s32 gBufferedFrameCounter;
extern s32 gFrameSkipCounter;
extern u32 __additional_scanline_0;
extern u8 gDisplayFramePending;
extern void* gDisplayBufferMsgs;
extern void* gDramStack;
extern void* gOutputBuffer;
extern void* gYieldBuffer;
extern long long int rspbootTextStart[];
extern long long int aspMainTextStart[];
extern Gfx gDefaultRenderDisplayList[];

void submitDisplayTask(OSMesg message);
void* allocateMemoryNode(s32 ownerID, u32 requestedSize, u8* nodeExists);
void initLinearArenaRegions(void);
void initLinearAllocator(void);
void initGraphicsArenas(void);
void initGraphicsSystem(void);

RECOMP_PATCH void initDisplayBuffers(void) {
    DisplayBufferMsg* msg;
    u8 exists;
    s32 i;
    Gfx* gfx;

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

    gDisplayBufferMsgs = msg = allocateMemoryNode(0, 3 * sizeof(DisplayBufferMsg), &exists);

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
        // @recomp remove call to gDPFullSync, we'll do this once at the end
        gSPEndDisplayList(gfx++);

        msg->yield_data_ptr = &gAuxFrameBuffers[i];
        msg->unk4C = 0;
        msg->unk4E = 2;

        msg->data_ptr = msg->displayList;
        msg->data_size = 0x70; // @recomp adjust message size to reflect missing gDPFullSync
        msg->type = 1;
        msg->flags = 0;
        msg->ucode_boot = rspbootTextStart;

        msg->ucode_boot_size = (u32) aspMainTextStart;
        msg->ucode_boot_size = msg->ucode_boot_size - ((u32) rspbootTextStart);

        msg->ucode = microcodeGroups[1].ucode;
        msg->output_buff_size = microcodeGroups[1].ucode_data;
        msg->ucode_data_size = 0x800;
        msg->ucode_data = gDramStack;
        msg->dram_stack_size = 0x400;
        msg->dram_stack = gOutputBuffer;
        msg->task_2C = (u32) msg->dram_stack + 0x10000;
        msg->output_buff = gYieldBuffer;
        msg->yield_data_size = 0xC00;
    }
}

RECOMP_PATCH void processDisplayFrameUpdate(void) {
    ViewportNode* node;
    ViewportNode* temp;
    FrameCallbackMsg* firstMsg = NULL;
    FrameCallbackMsg* lastMsg = NULL;
    FrameCallbackMsg* initMsg;
    Gfx* mergedDL = NULL;
    Gfx* mergedGfx = NULL;
    s32 msgCount = 0;
    s32 groupCount = 0;
    s32 nextDisplayBufferIndex;

    temp = gRootViewport.list3_next;
    gDisplayFramePending = 0;
    if (temp == NULL) {
        temp = &gRootViewport;
    }

    // @recomp Prepare merge state before the original task-submission walk.
    // If this frame has multiple graphics groups, start a synthetic display list
    // that embeds the next framebuffer-init task before the grouped draws.
    node = temp;
    while (node != NULL) {
        FrameCallbackMsg* msg = node->frameCallbackMsg;

        if (msg != NULL) {
            Gfx* wrapper = (Gfx*) msg->t.t.data_ptr;

            msgCount++;
            // @recomp Only merge patched wrappers whose second command jumps to the real draw list.
            if ((wrapper[1].words.w0 >> 24) == G_DL) {
                groupCount++;
                if (firstMsg == NULL) {
                    firstMsg = msg;
                }
                lastMsg = msg;
            }
        }

        node = node->list3_next;
    }
    if ((groupCount > 1) && (groupCount == msgCount)) {
        nextDisplayBufferIndex = gCurrentDisplayBufferIndex + 1;
        if (nextDisplayBufferIndex >= 3) {
            nextDisplayBufferIndex = 0;
        }
        initMsg = (FrameCallbackMsg*) ((u8*) gDisplayBufferMsgs + (nextDisplayBufferIndex * 0x150));

        mergedDL = (Gfx*) arenaAlloc16(48 * (s32) sizeof(Gfx));
        mergedGfx = mergedDL;
        gEXEnable(mergedGfx++);
        gSPDisplayList(mergedGfx++, initMsg->t.t.data_ptr);
    }

    node = temp;
    if (node != NULL) {
        do {
            if (node->frameCallbackMsg != NULL) {
                // @recomp Merge multi-group frames instead of submitting one task per group.
                if ((groupCount > 1) && (groupCount == msgCount)) {
                    Gfx* wrapper = (Gfx*) node->frameCallbackMsg->t.t.data_ptr;

                    Gfx* gfx = mergedGfx;
                    u32 drawsAddr = wrapper[1].words.w1;

                    gSPLoadUcode(gfx++, node->frameCallbackMsg->t.t.ucode, node->frameCallbackMsg->t.t.ucode_data);
                    gSPSegment(gfx++, 0, 0);
                    gSPDisplayList(gfx++, drawsAddr);

                    mergedGfx = gfx;
                    gFrameBufferFlags[gCurrentDoubleBufferIndex] = 1;
                } else {
                    gFrameBufferFlags[gCurrentDoubleBufferIndex] = 1;
                    submitDisplayTask((OSMesg) node->frameCallbackMsg);
                }
            }
            node = node->list3_next;
        } while (node != NULL);
    }
    // @recomp submit a single, merged task list if there was more than one task
    if ((groupCount > 1) && (groupCount == msgCount)) {
        FrameCallbackMsg* msg = firstMsg;

        gDPFullSync(mergedGfx++);
        gSPEndDisplayList(mergedGfx++);

        msg->t.t.ucode = (u64*) gspF3DEX2_fifoTextStart;
        msg->t.t.ucode_data = (u64*) gspF3DEX2_fifoDataStart;
        msg->t.t.data_ptr = (u64*) mergedDL;
        msg->t.t.data_size = (u32) ((u8*) mergedGfx - (u8*) mergedDL);

        if (lastMsg != msg) {
            msg->taskFlags = lastMsg->taskFlags;
            msg->msgQueue = lastMsg->msgQueue;
            msg->msgData = lastMsg->msgData;
        }

        submitDisplayTask((OSMesg) msg);
    }
    gFrameCounter = (gFrameCounter + 1) & 0x0FFFFFFF;
    gCurrentDoubleBufferIndex = (gCurrentDoubleBufferIndex + 1) & 1;
    gCurrentDisplayBufferIndex = gCurrentDisplayBufferIndex + 1;
    if (gCurrentDisplayBufferIndex >= 3) {
        gCurrentDisplayBufferIndex = 0;
    }

    if ((groupCount <= 1) || (groupCount != msgCount)) {
        // @recomp Non-merged frames submit the display-buffer init task just like the original.
        submitDisplayTask((OSMesg) ((u8*) gDisplayBufferMsgs + (gCurrentDisplayBufferIndex * 0x150)));
    }
}
