#include "patches.h"

typedef struct {
    s32 x;
    s32 y;
    s32 z;
} Vec3i;

typedef struct {
    s16 m[3][3];
    Vec3i translation;
} Transform3D;

extern Gfx *gRegionAllocPtr;
extern s16 gGraphicsMode;
extern Gfx D_800BAA30_AA8E0[];
extern Transform3D gScaleMatrix;

typedef struct {
    /* 0x00 */ u8 *data_ptr;
    /* 0x04 */ void *index_ptr;
    /* 0x08 */ u16 width;
    /* 0x0A */ u16 height;
} PlayerShadowTextureEntry;

typedef struct {
    /* 0x000 */ u8 pad0[0x18];
    /* 0x018 */ void *unk18;
    /* 0x01C */ u8 pad1[0x30 - 0x1C];
    /* 0x030 */ Vtx *jointVertices;
    /* 0x034 */ Mtx *jointMatrix;
    /* 0x038 */ u8 pad2[0xA10 - 0x38];
    /* 0xA10 */ Vec3i jointPositions[9];
    /* 0xA7C */ u8 pad3[0xBB8 - 0xA7C];
    /* 0xBB8 */ u8 playerIndex;
    /* 0xBB9 */ u8 pad4[0xBC1 - 0xBB9];
    /* 0xBC1 */ u8 jointShadowNeedsUpdate;
} PlayerShadowMatrixPlayer;

typedef PlayerShadowMatrixPlayer Player;

void *arenaAlloc16(s32 size);
void getTableEntryByU16Index(void *table, u16 index, PlayerShadowTextureEntry *output);
void transform3DToN64Mtx(Transform3D *transform, Mtx *mtx);
void *memcpy(void *dest, const void *src, size_t n);

RECOMP_PATCH void renderPlayerJointShadow(Player *player) {
    PlayerShadowMatrixPlayer *shadowPlayer = (PlayerShadowMatrixPlayer *)player;
    PlayerShadowTextureEntry textureEntry;
    s32 alpha;
    s32 i;

    if (shadowPlayer->jointShadowNeedsUpdate != 0) {
        shadowPlayer->jointVertices = arenaAlloc16(0x90);
        if (shadowPlayer->jointVertices != NULL) {
            i = 0;
            alpha = 0x50;
            do {
                s32 offset = i << 4;
                *(s16 *)(offset + (s32)shadowPlayer->jointVertices) =
                    (s16)((*(volatile s32 *)&shadowPlayer->jointPositions[i].x - shadowPlayer->jointPositions[0].x) >> 14);
                *(s16 *)(offset + (s32)shadowPlayer->jointVertices + 2) =
                    (s16)((*(volatile s32 *)&shadowPlayer->jointPositions[i].y - shadowPlayer->jointPositions[0].y) >> 14);
                *(s16 *)(offset + (s32)shadowPlayer->jointVertices + 4) =
                    (s16)((*(volatile s32 *)&shadowPlayer->jointPositions[i].z - shadowPlayer->jointPositions[0].z) >> 14);
                *(u8 *)(offset + (s32)shadowPlayer->jointVertices + 0xC) = 0;
                *(u8 *)(offset + (s32)shadowPlayer->jointVertices + 0xD) = 0;
                *(u8 *)(offset + (s32)shadowPlayer->jointVertices + 0xE) = 0;
                *(u8 *)(offset + (s32)shadowPlayer->jointVertices + 0xF) = alpha;
                i++;
            } while (i < 9);

            shadowPlayer->jointVertices[0].v.tc[0] = -0x20;
            shadowPlayer->jointVertices[0].v.tc[1] = -0x20;
            shadowPlayer->jointVertices[1].v.tc[0] = 0x3E0;
            shadowPlayer->jointVertices[1].v.tc[1] = -0x20;
            shadowPlayer->jointVertices[2].v.tc[0] = 0x7E0;
            shadowPlayer->jointVertices[2].v.tc[1] = -0x20;
            shadowPlayer->jointVertices[3].v.tc[0] = -0x20;
            shadowPlayer->jointVertices[3].v.tc[1] = 0x3E0;
            shadowPlayer->jointVertices[4].v.tc[0] = 0x3E0;
            shadowPlayer->jointVertices[4].v.tc[1] = 0x3E0;
            shadowPlayer->jointVertices[5].v.tc[0] = 0x7E0;
            shadowPlayer->jointVertices[5].v.tc[1] = 0x3E0;
            shadowPlayer->jointVertices[6].v.tc[0] = -0x20;
            shadowPlayer->jointVertices[6].v.tc[1] = 0x7E0;
            shadowPlayer->jointVertices[7].v.tc[0] = 0x3E0;
            shadowPlayer->jointVertices[7].v.tc[1] = 0x7E0;
            shadowPlayer->jointVertices[8].v.tc[0] = 0x7E0;
            shadowPlayer->jointVertices[8].v.tc[1] = 0x7E0;
        }

        shadowPlayer->jointMatrix = arenaAlloc16(0x40);
        if (shadowPlayer->jointMatrix != NULL) {
            memcpy(&gScaleMatrix.translation, &shadowPlayer->jointPositions[0], sizeof(Vec3i));
            transform3DToN64Mtx(&gScaleMatrix, shadowPlayer->jointMatrix);
        }
        shadowPlayer->jointShadowNeedsUpdate = 0;
    }

    if (shadowPlayer->jointVertices != NULL && shadowPlayer->jointMatrix != NULL) {
        gEXMatrixGroupDecomposedNormal(
            gRegionAllocPtr++,
            (u32)shadowPlayer,
            G_EX_PUSH,
            G_MTX_MODELVIEW,
            G_EX_EDIT_NONE
        );
        gSPMatrix(gRegionAllocPtr++, shadowPlayer->jointMatrix, (G_MTX_NOPUSH | G_MTX_LOAD) | G_MTX_MODELVIEW);
        gGraphicsMode = -1;
        gSPDisplayList(gRegionAllocPtr++, D_800BAA30_AA8E0);
        getTableEntryByU16Index(shadowPlayer->unk18, 0, &textureEntry);

        gDPSetTextureImage(gRegionAllocPtr++, G_IM_FMT_I, G_IM_SIZ_16b, 1, textureEntry.data_ptr);
        gDPSetTile(
            gRegionAllocPtr++,
            G_IM_FMT_I,
            G_IM_SIZ_16b,
            0,
            0,
            G_TX_LOADTILE,
            0,
            G_TX_CLAMP,
            5,
            G_TX_NOLOD,
            G_TX_CLAMP,
            5,
            G_TX_NOLOD
        );
        gDPLoadSync(gRegionAllocPtr++);
        gDPLoadBlock(gRegionAllocPtr++, G_TX_LOADTILE, 0, 0, 255, 1024);
        gDPPipeSync(gRegionAllocPtr++);
        gDPSetTile(
            gRegionAllocPtr++,
            G_IM_FMT_I,
            G_IM_SIZ_4b,
            2,
            0,
            G_TX_RENDERTILE,
            0,
            G_TX_CLAMP,
            5,
            G_TX_NOLOD,
            G_TX_CLAMP,
            5,
            G_TX_NOLOD
        );
        gDPSetTileSize(gRegionAllocPtr++, G_TX_RENDERTILE, 0, 0, 31 << 2, 31 << 2);

        gSPVertex(gRegionAllocPtr++, shadowPlayer->jointVertices, 9, 0);
        gSP2Triangles(gRegionAllocPtr++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(gRegionAllocPtr++, 1, 2, 5, 0, 5, 4, 1, 0);
        gSP2Triangles(gRegionAllocPtr++, 3, 4, 7, 0, 7, 6, 3, 0);
        gSP2Triangles(gRegionAllocPtr++, 4, 5, 7, 0, 5, 8, 7, 0);
        gEXPopMatrixGroup(gRegionAllocPtr++, G_MTX_MODELVIEW);
    }
}
