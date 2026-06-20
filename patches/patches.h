#pragma once

#define RECOMP_EXPORT __attribute__((section(".recomp_export")))
#define RECOMP_PATCH __attribute__((section(".recomp_patch")))
#define RECOMP_FORCE_PATCH __attribute__((section(".recomp_force_patch")))
#define RECOMP_DECLARE_EVENT(func)                                                          \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
        __attribute__((noinline, weak, used, section(".recomp_event"))) void func {         \
    }                                                                                       \
    _Pragma("GCC diagnostic pop")

// TODO fix renaming symbols in patch recompilation
#define osCreateMesgQueue osCreateMesgQueue_recomp
#define osRecvMesg osRecvMesg_recomp
#define osSendMesg osSendMesg_recomp
#define osViGetCurrentFramebuffer osViGetCurrentFramebuffer_recomp
#define osFlashWriteArray osFlashWriteArray_recomp
#define osFlashWriteBuffer osFlashWriteBuffer_recomp
#define osWritebackDCache osWritebackDCache_recomp
#define osInvalICache osInvalICache_recomp
#define osGetTime osGetTime_recomp
#define osEepromLongRead osEepromLongRead_recomp
#define osEepromLongWrite osEepromLongWrite_recomp

#define osContStartReadData osContStartReadData_recomp
#define osContGetReadData osContGetReadData_recomp
#define osContStartQuery osContStartQuery_recomp
#define osContGetQuery osContGetQuery_recomp

#define __sinf __sinf_recomp
#define __cosf __cosf_recomp
#define sqrtf sqrtf_recomp
#define osPiStartDma osPiStartDma_recomp
#define gRandFloat sRandFloat

#include "PR/ultratypes.h"
#include "PR/os_pi.h"
#include "PR/gbi.h"
#include "rt64_extended_gbi.h"
#include "PR/abi.h"

#include "core/buffers.h"
#include "data/data_table.h"
#include "gamestate.h"
#include "graphics/displaylist.h"
#include "graphics/graphics.h"
#include "graphics/sprite_rdp.h"
#include "graphics/tiled_sprite_grid.h"
#include "math/geometry.h"
#include "race/race_effects.h"
#include "race/race_session.h"
#include "system/task_scheduler.h"

#ifndef gEXFillRectangle
#define gEXFillRectangle(cmd, lorigin, rorigin, ulx, uly, lrx, lry)                         \
    G_EX_COMMAND2(cmd, PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FILLRECT_V1, 24, 0), \
                  PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12),                           \
                                                                                            \
                  PARAM((ulx) * 4, 16, 16) | PARAM((uly) * 4, 16, 0),                       \
                  PARAM((lrx) * 4, 16, 16) | PARAM((lry) * 4, 16, 0))
#endif

#define gEXMatrixGroupSkipAll(cmd, id, push, proj, edit)                                                       \
    gEXMatrixGroup(cmd, id, G_EX_INTERPOLATE_SIMPLE, push, proj, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,     \
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,         \
                   G_EX_COMPONENT_SKIP, G_EX_ORDER_LINEAR, edit, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP,        \
                   G_EX_COMPONENT_SKIP)

#define gEXMatrixGroupSkipAllAspect(cmd, id, push, proj, edit, aspect)                                         \
    gEXMatrixGroup(cmd, id, G_EX_INTERPOLATE_SIMPLE, push, proj, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,     \
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,         \
                   G_EX_COMPONENT_SKIP, G_EX_ORDER_LINEAR, edit, aspect, G_EX_COMPONENT_SKIP,                  \
                   G_EX_COMPONENT_SKIP)

#define gEXMatrixGroupSimpleNormal(cmd, id, push, proj, edit)                                                   \
    gEXMatrixGroup(cmd, id, G_EX_INTERPOLATE_SIMPLE, push, proj, G_EX_COMPONENT_INTERPOLATE,                    \
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,          \
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE,                 \
                   G_EX_ORDER_LINEAR, edit, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupSimpleVerts(cmd, id, push, proj, edit)                                                    \
    gEXMatrixGroup(cmd, id, G_EX_INTERPOLATE_SIMPLE, push, proj, G_EX_COMPONENT_INTERPOLATE,                    \
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,          \
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,           \
                   G_EX_ORDER_LINEAR, edit, G_EX_ASPECT_AUTO, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupSimpleVertsSkipRot(cmd, id, push, proj, edit)                                             \
    gEXMatrixGroup(cmd, id, G_EX_INTERPOLATE_SIMPLE, push, proj, G_EX_COMPONENT_INTERPOLATE,                    \
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,                 \
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,           \
                   G_EX_ORDER_LINEAR, edit, G_EX_ASPECT_AUTO, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedNormal(cmd, id, push, proj, edit)                                                \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,        \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,           \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedNormalTcs(cmd, id, push, proj, edit)                                             \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,        \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,           \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedSkipRot(cmd, id, push, proj, edit)                                               \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,               \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,           \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedSkipPosRot(cmd, id, push, proj, edit)                                            \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,                      \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,           \
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedVerts(cmd, id, push, proj, edit)                                                 \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,        \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,    \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedVertsSkipRot(cmd, id, push, proj, edit)                                          \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,               \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, edit,    \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

#define gEXMatrixGroupDecomposedVertsOrderAuto(cmd, id, push, proj, edit)                                        \
    gEXMatrixGroupDecomposed(cmd, id, push, proj, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,        \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_AUTO, edit,      \
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO)

int recomp_printf(const char* fmt, ...);
int _Sprintf(char* buffer, const char* fmt, ...);
void setBoardSelectObjectInterpolationSkip(void* object, s32 skip);
void setBoardSelectSceneModelInterpolationSkip(void* model, s32 skip);
float recomp_powf(float, float);
f32 __sinf(f32);
f32 __cosf(f32);
float sqrtf(float f);
void Game_InitFullViewport(void);
void* memcpy2(void* dest, const void* src, size_t n);

#define INCBIN(identifier, filename)         \
    asm(".pushsection .rodata\n"             \
        "\t.local " #identifier "\n"         \
        "\t.type " #identifier ", @object\n" \
        "\t.balign 8\n" #identifier ":\n"    \
        "\t.incbin \"" filename "\"\n\n"     \
                                             \
        "\t.balign 8\n"                      \
        "\t.popsection\n");                  \
    extern u8 identifier[]

void clear_camera_skipped();

void recomp_crash(const char* err);
