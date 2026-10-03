// Elevation / world model.
//
// Logical ground coordinates (u,v) are continuous, in tile units. An integer
// (u,v) is the CENTER of a tile. z is elevation in "levels"; one level == one
// map->tileheight pixels of vertical screen shift.
#pragma once

#include "raylib.h"
#include "tiled_map.h"

// TODO: fill from per-tile TSX metadata (class="Surface", walkable=true). The
// external .tsx loader does not parse <tile> properties yet.
struct TileDefinition
{
    bool isSurface = false;
    bool walkable = false;
};

struct SurfaceHit
{
    bool found = false;
    bool walkable = false;
    float elevation = 0.0f;
    const cute_tiled_layer_t *layer = nullptr;
};

// Placeholder: until TSX metadata is parsed, every non-empty tile is a walkable surface.
TileDefinition GetTileDefinition(int gid);

// Top-left pixel position of a cell for a staggered (staggeraxis=y, staggerindex=odd) map.
Vector2 StaggeredCellOrigin(const cute_tiled_map_t *map, int x, int y);

// Projects continuous logical (u,v,z) to map pixels. Integer (u,v) lands on the
// center of the top face of the block drawn in that staggered cell.
Vector2 WorldToMapPixel(const cute_tiled_map_t *map, Vector2 world, float z);

// Staggered cell -> logical tile coordinates (integer).
void TileCellToWorld(int tileX, int tileY, int *u, int *v);

// Continuous logical position -> staggered Tiled cell (nearest tile center).
// Does not bounds-check against the map; callers must.
bool WorldToTileCell(Vector2 worldPosition, int *tileX, int *tileY);

// Highest walkable surface under worldPosition with elevation <= currentZ + stepTolerance.
SurfaceHit FindSurfaceBelow(const cute_tiled_map_t *map, Vector2 worldPosition,
                            float currentZ, float stepTolerance);
