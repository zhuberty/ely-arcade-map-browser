// External .tsx tileset loading (cute_tiled does not parse these).
#pragma once

#include "raylib.h"
#include "tiled_map.h"

// Info read from an external .tsx tileset.
struct TilesetInfo
{
    int firstGid = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int spacing = 0;
    int margin = 0;
    Texture2D texture = {};
};

// Loads the .tsx referenced by ts (relative to mapDir) and its image texture.
bool LoadExternalTileset(const char *mapDir, cute_tiled_tileset_t *ts, TilesetInfo *info);
