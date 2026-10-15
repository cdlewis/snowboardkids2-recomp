#include "patches.h"

#include "abi.h"
#include "audio/audio_manager.h"
#include "libaudio.h"
#include "os_ai.h"
#include "os_convert.h"
#include "system/thread_manager.h"
#include "ucode.h"

extern long long int rspbootTextStart[];
extern long long int rspbootTextEnd[];
extern long long int aspMainTextStart[];
extern long long int aspMainDataStart[];

// @recomp Preserve the original audio task construction with a guard against stale startup buffers.
RECOMP_PATCH s32 audioCreateAndScheduleTask(AudioInfo *audioTaskDesc, AudioInfo *prevBuffer) {
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
        // @recomp Reject stale previous-buffer state at startup using the initialized self-pointer and buffer bounds.
        if ((prevBuffer->doneMessage.info == prevBuffer) && (prevBuffer->outputBuffer != NULL) &&
            (prevBuffer->frameSamples > 0) &&
            ((s32)prevBuffer->frameSamples <= (s32)(gAudioBufferSize + gAudioBufferPadding + 0x10))) {
            osAiSetNextBuffer(prevBuffer->outputBuffer, prevBuffer->frameSamples * 4);
        }
    }

    currentSamplesInBuffer = osAiGetLength() / 4;
    samplesToProcess = (((gAudioBufferSize - currentSamplesInBuffer) + gAudioBufferPadding) + 16) & 0xFFF0;

    audioTaskDesc->frameSamples = samplesToProcess;
    if ((s16)samplesToProcess < gMinAudioFrameSize) {
        audioTaskDesc->frameSamples = gMinAudioFrameSize;
    }

    commandBuffer = gAudioManager.commandLists[gAudioCmdBufferToggle];
    commandBufferEnd = alAudioFrame(commandBuffer, &commandLength, (void *)outputBuffer, audioTaskDesc->frameSamples);
    if (commandLength == 0) {
        return 0;
    }

    audioTaskDesc->messageQueue = &gAudioManager.taskDoneQueue;
    audioTaskDesc->message = &audioTaskDesc->doneMessage;
    audioTaskDesc->task.t.data_ptr = (u64 *)gAudioManager.commandLists[gAudioCmdBufferToggle];
    commandBufferStart = gAudioManager.commandLists[gAudioCmdBufferToggle];
    commandBufferSize = commandBufferEnd - commandBufferStart;
    commandBufferSize = commandBufferSize * 8;

    audioTaskDesc->task.t.type = M_AUDTASK;
    audioTaskDesc->task.t.ucode_boot = (u64 *)rspbootTextStart;
    audioTaskDesc->task.t.ucode_boot_size = (u32)rspbootTextEnd - (u32)rspbootTextStart;
    audioTaskDesc->task.t.ucode = (u64 *)aspMainTextStart;
    audioTaskDesc->task.t.flags = 0;
    audioTaskDesc->task.t.ucode_data = (u64 *)&aspMainDataStart;
    audioTaskDesc->task.t.ucode_data_size = 0x800;
    audioTaskDesc->task.t.dram_stack = NULL;
    audioTaskDesc->task.t.dram_stack_size = 0;
    audioTaskDesc->task.t.output_buff = NULL;
    audioTaskDesc->task.t.output_buff_size = NULL;
    audioTaskDesc->task.t.yield_data_ptr = NULL;
    audioTaskDesc->task.t.yield_data_size = 0;
    audioTaskDesc->task.t.data_size = commandBufferSize;

    submitAudioTask((OSMesg)&audioTaskDesc->task);

    gAudioCmdBufferToggle ^= 1;

    return 1;
}
