#ifndef STRUCTS_H
#define STRUCTS_H

#include "patches.h"

typedef struct {
    u8 light1R;
    u8 light1G;
    u8 light1B;
    u8 pad14B;
    u8 light1R_dup;
    u8 light1G_dup;
    u8 light1B_dup;
    u8 unk14F;
    u8 light2R;
    u8 light2G;
    u8 light2B;
    u8 padding[0x5];
} Node_70B00_ColorData;

typedef struct PoolEntry {
    struct PoolEntry *next;
    void *callback;
    void *callbackData;
    u8 _padC[3];
    u8 poolIndex;
} PoolEntry;

typedef struct Node_70B00 {
    /* 0x00 */ union {
        struct Node_70B00 *next;
        u16 callback_selector;
    } unk0;
    /* 0x04 */ struct Node_70B00 *prev;
    /* 0x08 */ union {
        struct Node_70B00 *list2_next;
        u16 callback_selector;
    } unk8;
    /* 0x0C */ struct Node_70B00 *list2_prev;
    /* 0x10 */ struct Node_70B00 *list3_next;
    s8 unk14;
    s8 unk15;
    /* 0x16 */ u16 slot_index;
    /* 0x18 */ PoolEntry pool[7];
    /* 0x88 */ void *unk88;
    u8 padding2[0x10];
    /* 0x9C */ void *frameCallbackMsg;
    /* 0xA0 */ s16 unkA0;
    /* 0xA2 */ s16 unkA2;
    /* 0xA4 */ s16 unkA4;
    /* 0xA6 */ s16 unkA6;
    /* 0xA8 */ s16 unkA8;
    /* 0xAA */ s16 unkAA;
    /* 0xAC */ s16 unkAC;
    /* 0xAE */ s16 unkAE;
    /* 0xB0 */ s16 unkB0;
    /* 0xB2 */ s16 unkB2;
    /* 0xB4 */ s16 unkB4;
    /* 0xB6 */ s16 unkB6;
    u8 padding2b[0x4];
    /* 0xBC */ u8 envR;
    u8 envG;
    u8 envB;
    u8 prevFadeValue;
    u8 fadeValue;
    u8 fadeMode;
    u8 padding8[0x6];
    s16 viewportWidth;
    s16 viewportHeight;
    /* 0xCC */ s16 unkCC;
    /* 0xCE */ s16 unkCE;
    /* 0xD0 */ s16 unkD0;
    /* 0xD2 */ s16 unkD2;
    /* 0xD4 */ s16 unkD4;
    /* 0xD6 */ s16 unkD6;
    /* 0xD8 */ u16 perspNorm;
    /* 0xDA */ u16 id;
    /* 0xE0 */ Mtx perspectiveMatrix;
    u8 modelingMatrix[0x20];
    u16 unk140;
    u8 padding140[6];
    Node_70B00_ColorData unk148[1];
    u8 padding158[0x70];
    s16 fogMin;
    s16 fogMax;
    u8 fogR;
    u8 fogG;
    u8 fogB;
    u8 padding1CF;
    f32 scaleY;
    u8 padding5[0x2];
} Node_70B00;

#endif /* STRUCTS_H */