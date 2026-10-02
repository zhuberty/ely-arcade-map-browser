// ely-arcade-map-browser
//
// Press ESC to exit back to the arcade menu.
// The menu launches this as a child process and waits for it to terminate.

#include "raylib.h"
#include "resource_dir.h"
#include "arcade_input.h"
#define CUTE_TILED_NO_EXTERNAL_TILESET_WARNING
#define CUTE_TILED_IMPLEMENTATION
#include "cute_tiled.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Info read from an external .tsx tileset (cute_tiled does not parse these).
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

// Reads an integer attribute (name="123") from XML text. Returns fallback if not found.
static int XmlIntAttr(const char *xml, const char *name, int fallback)
{
    char key[64];
    snprintf(key, sizeof(key), " %s=\"", name);
    const char *p = strstr(xml, key);
    return p ? atoi(p + strlen(key)) : fallback;
}

// Reads a string attribute (name="value") from XML text.
static bool XmlStrAttr(const char *xml, const char *name, char *out, size_t outSize)
{
    char key[64];
    snprintf(key, sizeof(key), " %s=\"", name);
    const char *p = strstr(xml, key);
    if (!p) return false;
    p += strlen(key);
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool LoadExternalTileset(const char *mapDir, cute_tiled_tileset_t *ts, TilesetInfo *info)
{
    info->firstGid = ts->firstgid;
    if (!ts->source.ptr) return false;   // embedded tilesets not handled here

    char tsxPath[512];
    snprintf(tsxPath, sizeof(tsxPath), "%s/%s", mapDir, ts->source.ptr);
    char *xml = LoadFileText(tsxPath);
    if (!xml) { TraceLog(LOG_ERROR, "MAP: failed to read tileset %s", tsxPath); return false; }

    info->tileWidth = XmlIntAttr(xml, "tilewidth", 0);
    info->tileHeight = XmlIntAttr(xml, "tileheight", 0);
    info->columns = XmlIntAttr(xml, "columns", 0);
    info->spacing = XmlIntAttr(xml, "spacing", 0);
    info->margin = XmlIntAttr(xml, "margin", 0);

    char image[256];
    bool hasImage = XmlStrAttr(xml, "source", image, sizeof(image));
    UnloadFileText(xml);
    if (!hasImage) return false;

    char imagePath[512];
    snprintf(imagePath, sizeof(imagePath), "%s/%s", mapDir, image);
    info->texture = LoadTexture(imagePath);
    TraceLog(LOG_INFO, "MAP: tileset '%s' image=%s tile=%dx%d columns=%d",
             ts->source.ptr, imagePath, info->tileWidth, info->tileHeight, info->columns);
    return info->texture.id != 0 && info->columns > 0;
}

static void LogMapInfo(cute_tiled_map_t *map)
{
    TraceLog(LOG_INFO, "MAP: orientation=%s renderorder=%s tiled=%s infinite=%d",
             map->orientation.ptr ? map->orientation.ptr : "?",
             map->renderorder.ptr ? map->renderorder.ptr : "?",
             map->tiledversion.ptr ? map->tiledversion.ptr : "?", map->infinite);
    TraceLog(LOG_INFO, "MAP: staggeraxis=%s staggerindex=%s",
             map->staggeraxis.ptr ? map->staggeraxis.ptr : "-",
             map->staggerindex.ptr ? map->staggerindex.ptr : "-");
    TraceLog(LOG_INFO, "MAP: size=%dx%d tiles, tile=%dx%d px", map->width, map->height, map->tilewidth, map->tileheight);

    for (cute_tiled_tileset_t *ts = map->tilesets; ts; ts = ts->next)
        TraceLog(LOG_INFO, "MAP: tileset firstgid=%d source=%s", ts->firstgid, ts->source.ptr ? ts->source.ptr : "(embedded)");

    for (cute_tiled_layer_t *l = map->layers; l; l = l->next)
    {
        TraceLog(LOG_INFO, "MAP: layer id=%d name='%s' type=%s %dx%d visible=%d opacity=%.2f",
                 l->id, l->name.ptr, l->type.ptr, l->width, l->height, l->visible, l->opacity);
        for (int i = 0; i < l->property_count; i++)
        {
            cute_tiled_property_t *p = &l->properties[i];
            if (p->type == CUTE_TILED_PROPERTY_INT)
                TraceLog(LOG_INFO, "MAP:   property %s=%d", p->name.ptr, p->data.integer);
            else if (p->type == CUTE_TILED_PROPERTY_STRING)
                TraceLog(LOG_INFO, "MAP:   property %s=%s", p->name.ptr, p->data.string.ptr);
        }
    }
}

// Top-left pixel position of a cell for a staggered (staggeraxis=y, staggerindex=odd) map.
static Vector2 StaggeredCellOrigin(const cute_tiled_map_t *map, int x, int y)
{
    float px = (float)(x * map->tilewidth) + ((y & 1) ? map->tilewidth * 0.5f : 0.0f);
    float py = (float)(y * map->tileheight) * 0.5f;
    return { px, py };
}

static void DrawTileLayer(const cute_tiled_map_t *map, const cute_tiled_layer_t *layer,
                          const TilesetInfo &ts, Rectangle view)
{
    Color tint = Fade(WHITE, layer->opacity);
    for (int y = 0; y < layer->height; y++)
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
            float dy = o.y + (float)(map->tileheight - ts.tileHeight) + layer->offsety;

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
}

int main(void)
{
    // SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(1200, 1200, "ely-arcade-map-browser");   // 0,0 = monitor native resolution
    SetExitKey(KEY_ESCAPE);      // ESC exits back to the arcade menu
    SetTargetFPS(60);

    SearchAndSetResourceDir("resources");

    const char *mapDir = "game-map";
    cute_tiled_map_t *map = cute_tiled_load_map_from_file("game-map/example-map.tmj", NULL);
    TilesetInfo tileset;
    bool tilesetOk = false;
    if (map)
    {
        LogMapInfo(map);
        tilesetOk = map->tilesets && LoadExternalTileset(mapDir, map->tilesets, &tileset);
    }
    else
    {
        TraceLog(LOG_ERROR, "MAP: failed to load: %s (line %d)", cute_tiled_error_reason, cute_tiled_error_line);
    }

    Camera2D camera = {};
    camera.zoom = 2.0f;
    camera.offset = { GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f };
    if (map)
        camera.target = { map->width * map->tilewidth * 0.5f, map->height * map->tileheight * 0.25f };

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        float speed = 600.0f / camera.zoom;
        if (IsKeyDown(KEY_RIGHT)) camera.target.x += speed * dt;
        if (IsKeyDown(KEY_LEFT))  camera.target.x -= speed * dt;
        if (IsKeyDown(KEY_DOWN))  camera.target.y += speed * dt;
        if (IsKeyDown(KEY_UP))    camera.target.y -= speed * dt;
        camera.zoom += GetMouseWheelMove() * 0.1f * camera.zoom;
        if (camera.zoom < 0.25f) camera.zoom = 0.25f;
        if (camera.zoom > 8.0f)  camera.zoom = 8.0f;

        BeginDrawing();
        ClearBackground(BLACK);

        if (map && tilesetOk)
        {
            Vector2 tl = GetScreenToWorld2D({ 0, 0 }, camera);
            Vector2 br = GetScreenToWorld2D({ (float)GetScreenWidth(), (float)GetScreenHeight() }, camera);
            Rectangle view = { tl.x, tl.y, br.x - tl.x, br.y - tl.y };

            BeginMode2D(camera);
            for (cute_tiled_layer_t *l = map->layers; l; l = l->next)
            {
                if (!l->visible || !l->data || !l->type.ptr) continue;
                if (strcmp(l->type.ptr, "tilelayer") != 0) continue;
                DrawTileLayer(map, l, tileset, view);
            }
            EndMode2D();
        }
        else
        {
            DrawText("Failed to load map (see log)", 100, 100, 40, RED);
        }

        DrawText("Arrows: pan   Wheel: zoom   ESC: exit", 20, 20, 20, LIGHTGRAY);
        DrawFPS(20, 50);
        EndDrawing();
    }

    if (tilesetOk) UnloadTexture(tileset.texture);
    if (map) cute_tiled_free_map(map);
    CloseWindow();
    return 0;
}