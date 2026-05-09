#include "patches.h"

extern Gfx *gRegionAllocPtr;
extern s16 gGraphicsMode;
extern Gfx D_800BAA30_AA8E0[];
extern Transform3D gScaleMatrix;

RECOMP_PATCH void renderPlayerJointShadow(Player *player) {
    OutputStruct_19E80 textureEntry;
    s32 alpha;
    s32 i;

    if (player->jointShadowNeedsUpdate != 0) {
        player->jointVertices = arenaAlloc16(0x90);
        if (player->jointVertices != NULL) {
            i = 0;
            alpha = 0x50;
            do {
                s32 offset = i << 4;
                *(s16 *)(offset + (s32)player->jointVertices) =
                    (s16)((*(volatile s32 *)&player->jointPositions[i].x - player->jointPositions[0].x) >> 14);
                *(s16 *)(offset + (s32)player->jointVertices + 2) =
                    (s16)((*(volatile s32 *)&player->jointPositions[i].y - player->jointPositions[0].y) >> 14);
                *(s16 *)(offset + (s32)player->jointVertices + 4) =
                    (s16)((*(volatile s32 *)&player->jointPositions[i].z - player->jointPositions[0].z) >> 14);
                *(u8 *)(offset + (s32)player->jointVertices + 0xC) = 0;
                *(u8 *)(offset + (s32)player->jointVertices + 0xD) = 0;
                *(u8 *)(offset + (s32)player->jointVertices + 0xE) = 0;
                *(u8 *)(offset + (s32)player->jointVertices + 0xF) = alpha;
                i++;
            } while (i < 9);

            player->jointVertices[0].v.tc[0] = -0x20;
            player->jointVertices[0].v.tc[1] = -0x20;
            player->jointVertices[1].v.tc[0] = 0x3E0;
            player->jointVertices[1].v.tc[1] = -0x20;
            player->jointVertices[2].v.tc[0] = 0x7E0;
            player->jointVertices[2].v.tc[1] = -0x20;
            player->jointVertices[3].v.tc[0] = -0x20;
            player->jointVertices[3].v.tc[1] = 0x3E0;
            player->jointVertices[4].v.tc[0] = 0x3E0;
            player->jointVertices[4].v.tc[1] = 0x3E0;
            player->jointVertices[5].v.tc[0] = 0x7E0;
            player->jointVertices[5].v.tc[1] = 0x3E0;
            player->jointVertices[6].v.tc[0] = -0x20;
            player->jointVertices[6].v.tc[1] = 0x7E0;
            player->jointVertices[7].v.tc[0] = 0x3E0;
            player->jointVertices[7].v.tc[1] = 0x7E0;
            player->jointVertices[8].v.tc[0] = 0x7E0;
            player->jointVertices[8].v.tc[1] = 0x7E0;
        }

        player->jointMatrix = arenaAlloc16(0x40);
        if (player->jointMatrix != NULL) {
            memcpy(&gScaleMatrix.translation, &player->jointPositions[0], sizeof(Vec3i));
            transform3DToN64Mtx(&gScaleMatrix, player->jointMatrix);
        }
        player->jointShadowNeedsUpdate = 0;
    }

    if (player->jointVertices != NULL && player->jointMatrix != NULL) {
        gEXMatrixGroupDecomposedVerts(
            gRegionAllocPtr++,
            (u32)&player->jointVertices,
            G_EX_PUSH,
            G_MTX_MODELVIEW,
            G_EX_EDIT_NONE
        );
        gSPMatrix(gRegionAllocPtr++, player->jointMatrix, (G_MTX_NOPUSH | G_MTX_LOAD) | G_MTX_MODELVIEW);
        gGraphicsMode = -1;
        gSPDisplayList(gRegionAllocPtr++, D_800BAA30_AA8E0);
        getTableEntryByU16Index(player->unk18, 0, &textureEntry);

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

        gSPVertex(gRegionAllocPtr++, player->jointVertices, 9, 0);
        gSP2Triangles(gRegionAllocPtr++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(gRegionAllocPtr++, 1, 2, 5, 0, 5, 4, 1, 0);
        gSP2Triangles(gRegionAllocPtr++, 3, 4, 7, 0, 7, 6, 3, 0);
        gSP2Triangles(gRegionAllocPtr++, 4, 5, 7, 0, 5, 8, 7, 0);
        gEXPopMatrixGroup(gRegionAllocPtr++, G_MTX_MODELVIEW);
    }
}
