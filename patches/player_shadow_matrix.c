#include "patches.h"

extern Gfx *gDisplayListAllocPtr;
extern s16 gGraphicsMode;
extern Gfx gPlayerShadowRenderSetupDl[];
extern Transform3D gScaleMatrix;

RECOMP_PATCH void renderRacerProjectedShadow(Player *player) {
    OutputStruct_19E80 textureEntry;
    s32 alpha;
    s32 i;

    if (player->shadowMeshNeedsUpdate != 0) {
        player->shadowVertices = arenaAlloc16(0x90);
        if (player->shadowVertices != NULL) {
            i = 0;
            alpha = 0x50;
            do {
                s32 offset = i << 4;
                *(s16 *)(offset + (s32)player->shadowVertices) =
                    (s16)((*(volatile s32 *)&player->shadowSamplePositions[i].x - player->shadowSamplePositions[0].x) >> 14);
                *(s16 *)(offset + (s32)player->shadowVertices + 2) =
                    (s16)((*(volatile s32 *)&player->shadowSamplePositions[i].y - player->shadowSamplePositions[0].y) >> 14);
                *(s16 *)(offset + (s32)player->shadowVertices + 4) =
                    (s16)((*(volatile s32 *)&player->shadowSamplePositions[i].z - player->shadowSamplePositions[0].z) >> 14);
                *(u8 *)(offset + (s32)player->shadowVertices + 0xC) = 0;
                *(u8 *)(offset + (s32)player->shadowVertices + 0xD) = 0;
                *(u8 *)(offset + (s32)player->shadowVertices + 0xE) = 0;
                *(u8 *)(offset + (s32)player->shadowVertices + 0xF) = alpha;
                i++;
            } while (i < 9);

            player->shadowVertices[0].v.tc[0] = -0x20;
            player->shadowVertices[0].v.tc[1] = -0x20;
            player->shadowVertices[1].v.tc[0] = 0x3E0;
            player->shadowVertices[1].v.tc[1] = -0x20;
            player->shadowVertices[2].v.tc[0] = 0x7E0;
            player->shadowVertices[2].v.tc[1] = -0x20;
            player->shadowVertices[3].v.tc[0] = -0x20;
            player->shadowVertices[3].v.tc[1] = 0x3E0;
            player->shadowVertices[4].v.tc[0] = 0x3E0;
            player->shadowVertices[4].v.tc[1] = 0x3E0;
            player->shadowVertices[5].v.tc[0] = 0x7E0;
            player->shadowVertices[5].v.tc[1] = 0x3E0;
            player->shadowVertices[6].v.tc[0] = -0x20;
            player->shadowVertices[6].v.tc[1] = 0x7E0;
            player->shadowVertices[7].v.tc[0] = 0x3E0;
            player->shadowVertices[7].v.tc[1] = 0x7E0;
            player->shadowVertices[8].v.tc[0] = 0x7E0;
            player->shadowVertices[8].v.tc[1] = 0x7E0;
        }

        player->shadowMatrix = arenaAlloc16(0x40);
        if (player->shadowMatrix != NULL) {
            memcpy(&gScaleMatrix.translation, &player->shadowSamplePositions[0], sizeof(Vec3i));
            transform3DToN64Mtx(&gScaleMatrix, player->shadowMatrix);
        }
        player->shadowMeshNeedsUpdate = 0;
    }

    if (player->shadowVertices != NULL && player->shadowMatrix != NULL) {
        gEXMatrixGroupDecomposedVerts(
            gDisplayListAllocPtr++,
            (u32)&player->shadowVertices,
            G_EX_PUSH,
            G_MTX_MODELVIEW,
            G_EX_EDIT_NONE
        );
        gSPMatrix(gDisplayListAllocPtr++, player->shadowMatrix, (G_MTX_NOPUSH | G_MTX_LOAD) | G_MTX_MODELVIEW);
        gGraphicsMode = -1;
        gSPDisplayList(gDisplayListAllocPtr++, gPlayerShadowRenderSetupDl);
        getTableEntryByU16Index(player->assetTable, 0, &textureEntry);

        gDPSetTextureImage(gDisplayListAllocPtr++, G_IM_FMT_I, G_IM_SIZ_16b, 1, textureEntry.data_ptr);
        gDPSetTile(
            gDisplayListAllocPtr++,
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
        gDPLoadSync(gDisplayListAllocPtr++);
        gDPLoadBlock(gDisplayListAllocPtr++, G_TX_LOADTILE, 0, 0, 255, 1024);
        gDPPipeSync(gDisplayListAllocPtr++);
        gDPSetTile(
            gDisplayListAllocPtr++,
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
        gDPSetTileSize(gDisplayListAllocPtr++, G_TX_RENDERTILE, 0, 0, 31 << 2, 31 << 2);

        gSPVertex(gDisplayListAllocPtr++, player->shadowVertices, 9, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 1, 2, 5, 0, 5, 4, 1, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 3, 4, 7, 0, 7, 6, 3, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 4, 5, 7, 0, 5, 8, 7, 0);
        gEXPopMatrixGroup(gDisplayListAllocPtr++, G_MTX_MODELVIEW);
    }
}
