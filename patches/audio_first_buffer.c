#include "patches.h"

#include "abi.h"
#include "libaudio.h"
#include "os_ai.h"
#include "os_convert.h"
#include "system/thread_manager.h"
#include "ucode.h"

typedef struct {
    /* 0x00 */ void *outputBuffer;
    /* 0x04 */ s16 frameSizeInSamples;
    s16 _pad6;
    /* 0x08 */ s32 taskType;
    /* 0x0C */ s32 taskFlags;
    /* 0x10 */ void *bootCode;
    /* 0x14 */ s32 bootCodeSize;
    /* 0x18 */ void *taskCode;
    /* 0x1C */ s32 taskCodeSize;
    /* 0x20 */ void *dataMemory;
    /* 0x24 */ s32 dataMemorySize;
    s32 unk28;
    s32 unk2C;
    s32 unk30;
    s32 unk34;
    /* 0x38 */ Acmd *commandBuffer;
    /* 0x3C */ s32 commandBufferSize;
    s32 unk40;
    s32 unk44;
    void *unk48;
    void *unk4C;
    /* 0x50 */ s16 unk50;
    s16 _pad52;
    /* 0x54 */ void *unk54;
    u8 _pad58[0x18];
} AudioStruct;

extern Acmd *gAudioCmdBuffers[];
extern s32 gAudioCmdBufferToggle;
extern u32 gAudioBufferSize;
extern u32 gAudioBufferPadding;
extern u32 gMinAudioFrameSize;
extern long long int rspbootTextStart[];
extern long long int rspbootTextEnd[];
extern long long int aspMainTextStart[];
extern long long int aspMainDataStart[];

void processAudioNodeList(void);

RECOMP_PATCH s32 audioCreateAndScheduleTask(AudioStruct *audioTaskDesc, AudioStruct *prevBuffer) {
    s32 commandLength;
    s16 *outputBuffer;
    s32 commandBufferSize;
    Acmd *commandBuffer;
    Acmd *commandBufferStart;
    Acmd *commandBufferEnd;
    s32 samplesToProcess;
    s32 currentSamplesInBuffer;

    processAudioNodeList();
    outputBuffer = (s16 *)osVirtualToPhysical(audioTaskDesc->outputBuffer);

    if (prevBuffer != 0) {
        s16 frameSizeInSamples = prevBuffer->frameSizeInSamples;

        // @recomp The first ROM launch on Linux can report an invalid previous-buffer size; clamp it so
        // osAiSetNextBuffer still starts AI without asking the runtime to queue a huge sample count.
        // TODO(#42): root-cause what is actually going on here.
        if ((frameSizeInSamples <= 0) ||
            ((s32)frameSizeInSamples > (s32)(gAudioBufferSize + gAudioBufferPadding + 0x10))) {
            frameSizeInSamples = gAudioBufferSize;
        }

        osAiSetNextBuffer(prevBuffer->outputBuffer, frameSizeInSamples * 4);
    }

    currentSamplesInBuffer = osAiGetLength() / 4;
    samplesToProcess = (((gAudioBufferSize - currentSamplesInBuffer) + gAudioBufferPadding) + 16) & 0xFFF0;

    audioTaskDesc->frameSizeInSamples = samplesToProcess;
    if ((s16)samplesToProcess < gMinAudioFrameSize) {
        audioTaskDesc->frameSizeInSamples = gMinAudioFrameSize;
    }

    commandBuffer = gAudioCmdBuffers[gAudioCmdBufferToggle];
    commandBufferEnd =
        alAudioFrame(commandBuffer, &commandLength, (void *)outputBuffer, audioTaskDesc->frameSizeInSamples);
    if (commandLength == 0) {
        return 0;
    }

    audioTaskDesc->unk48 = &gAudioCmdBuffers[0x80];
    audioTaskDesc->unk4C = &audioTaskDesc->unk50;
    audioTaskDesc->commandBuffer = gAudioCmdBuffers[gAudioCmdBufferToggle];
    commandBufferStart = gAudioCmdBuffers[gAudioCmdBufferToggle];
    commandBufferSize = commandBufferEnd - commandBufferStart;
    commandBufferSize = commandBufferSize * 8;

    audioTaskDesc->taskType = M_AUDTASK;
    audioTaskDesc->bootCode = rspbootTextStart;
    audioTaskDesc->bootCodeSize = (u32)rspbootTextEnd - (u32)rspbootTextStart;
    audioTaskDesc->taskCode = aspMainTextStart;
    audioTaskDesc->taskFlags = 0;
    audioTaskDesc->dataMemory = &aspMainDataStart;
    audioTaskDesc->dataMemorySize = 0x800;
    audioTaskDesc->unk28 = 0;
    audioTaskDesc->unk2C = 0;
    audioTaskDesc->unk30 = 0;
    audioTaskDesc->unk34 = 0;
    audioTaskDesc->unk40 = 0;
    audioTaskDesc->unk44 = 0;
    audioTaskDesc->commandBufferSize = commandBufferSize;

    submitAudioTask(((OSMesg)&audioTaskDesc->taskType));

    gAudioCmdBufferToggle ^= 1;

    return 1;
}
