#include "map_renderer.h"
#include "world.h"

void DrawTileLayer(const cute_tiled_map_t *map, const cute_tiled_layer_t *layer,
                   const TilesetInfo &ts, Rectangle view,
                   const Shader *cutShader, int rowBegin, int rowEnd)
{
    if (cutShader) BeginShaderMode(*cutShader);
    float elevationPixels = (float)(GetLayerElevation(layer) * map->tileheight);
    Color tint = Fade(WHITE, layer->opacity);
    if (rowBegin < 0) rowBegin = 0;
    if (rowEnd > layer->height) rowEnd = layer->height;
    for (int y = rowBegin; y < rowEnd; y++)
    {
        for (int x = 0; x < layer->width; x++)
        {
            int raw = layer->data[y * layer->width + x];
            int hflip, vflip, dflip;
            cute_tiled_get_flags(raw, &hflip, &vflip, &dflip);
            int gid = cute_tiled_unset_flags(raw);
            if (gid < ts.firstGid) continue;

            int id = gid - ts.firstGid;
            Vector2 o = StaggeredCellOrigin(map, x, y);
            // Tiles are bottom-aligned to their map cell
            float dx = o.x + layer->offsetx;
            float dy = o.y + (float)(map->tileheight - ts.tileHeight) + layer->offsety
                       - elevationPixels;

            if (dx + ts.tileWidth < view.x || dx > view.x + view.width ||
                dy + ts.tileHeight < view.y || dy > view.y + view.height) continue;

            Rectangle src = {
                (float)(ts.margin + (id % ts.columns) * (ts.tileWidth + ts.spacing)),
                (float)(ts.margin + (id / ts.columns) * (ts.tileHeight + ts.spacing)),
                (float)(hflip ? -ts.tileWidth : ts.tileWidth),
                (float)(vflip ? -ts.tileHeight : ts.tileHeight)
            };
            DrawTextureRec(ts.texture, src, { dx, dy }, tint);
        }
    }
    if (cutShader) EndShaderMode();
}
