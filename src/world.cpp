#include "world.h"

#include <math.h>

TileDefinition GetTileDefinition(int gid)
{
    TileDefinition def;
    def.isSurface = gid != 0;
    def.walkable = gid != 0;
    return def;
}

Vector2 StaggeredCellOrigin(const cute_tiled_map_t *map, int x, int y)
{
    float px = (float)(x * map->tilewidth) + ((y & 1) ? map->tilewidth * 0.5f : 0.0f);
    float py = (float)(y * map->tileheight) * 0.5f;
    return { px, py };
}

Vector2 WorldToMapPixel(const cute_tiled_map_t *map, Vector2 world, float z)
{
    float sx = (world.x - world.y) * map->tilewidth * 0.5f + map->tilewidth * 0.5f;
    // Tiles are bottom-aligned, so a block's visible top face is centered one half
    // tile height ABOVE the cell origin (cell origin - tileheight + tileheight/2).
    float sy = (world.x + world.y) * map->tileheight * 0.5f - map->tileheight * 0.5f;
    sy -= z * map->tileheight;
    return { sx, sy };
}

void TileCellToWorld(int tileX, int tileY, int *u, int *v)
{
    *u = tileX + (tileY + 1) / 2;
    *v = tileY / 2 - tileX;
}

bool WorldToTileCell(Vector2 worldPosition, int *tileX, int *tileY)
{
    int u = (int)floorf(worldPosition.x + 0.5f);
    int v = (int)floorf(worldPosition.y + 0.5f);
    int ty = u + v;
    *tileY = ty;
    *tileX = (u - v - (ty & 1)) / 2;   // numerator is always even
    return true;
}

SurfaceHit FindSurfaceBelow(const cute_tiled_map_t *map, Vector2 worldPosition,
                            float currentZ, float stepTolerance)
{
    SurfaceHit best;
    int tx, ty;
    if (!WorldToTileCell(worldPosition, &tx, &ty)) return best;
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) return best;

    for (const cute_tiled_layer_t *l = map->layers; l; l = l->next)
    {
        if (!IsDrawableTileLayer(l)) continue;
        if (!LayerHasElevation(l)) continue;
        if (tx >= l->width || ty >= l->height) continue;

        int gid = cute_tiled_unset_flags(l->data[ty * l->width + tx]);
        if (gid == 0) continue;
        TileDefinition def = GetTileDefinition(gid);
        if (!def.isSurface || !def.walkable) continue;

        float elev = (float)GetLayerElevation(l);
        if (elev > currentZ + stepTolerance) continue;
        if (!best.found || elev > best.elevation)
        {
            best.found = true;
            best.walkable = true;
            best.elevation = elev;
            best.layer = l;
        }
    }
    return best;
}
