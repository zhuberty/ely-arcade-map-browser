// Tile layer rendering.
#pragma once

#include "raylib.h"
#include "tiled_map.h"
#include "tileset.h"

// Draws rows [rowBegin, rowEnd) of a tile layer that fall inside `view` (map-pixel space).
// If cutShader is non-null the layer is drawn through it.
void DrawTileLayer(const cute_tiled_map_t *map, const cute_tiled_layer_t *layer,
                   const TilesetInfo &ts, Rectangle view,
                   const Shader *cutShader = nullptr,
                   int rowBegin = 0, int rowEnd = 0x7fffffff);
