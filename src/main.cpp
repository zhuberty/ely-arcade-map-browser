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

#include <math.h>
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
// ---------------------------------------------------------------------------
// Elevation / world model
//
// Logical ground coordinates (u,v) are continuous, in tile units. An integer
// (u,v) is the CENTER of a tile. z is elevation in "levels"; one level == one
// map->tileheight pixels of vertical screen shift.
// ---------------------------------------------------------------------------

// TODO: fill from per-tile TSX metadata (class="Surface", walkable=true). The
// external .tsx loader above does not parse <tile> properties yet.
struct TileDefinition
{
    bool isSurface = false;
    bool walkable = false;
};

// Placeholder: until TSX metadata is parsed, every non-empty tile is a walkable surface.
static TileDefinition GetTileDefinition(int gid)
{
    TileDefinition def;
    def.isSurface = gid != 0;
    def.walkable = gid != 0;
    return def;
}

static bool LayerHasElevation(const cute_tiled_layer_t *layer)
{
    for (int i = 0; i < layer->property_count; i++)
    {
        const cute_tiled_property_t *p = &layer->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT && p->name.ptr && strcmp(p->name.ptr, "elevation") == 0)
            return true;
    }
    return false;
}

static int GetLayerElevation(const cute_tiled_layer_t *layer)
{
    for (int i = 0; i < layer->property_count; i++)
    {
        const cute_tiled_property_t *p = &layer->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT && p->name.ptr && strcmp(p->name.ptr, "elevation") == 0)
            return p->data.integer;
    }
    return 0;
}

// Projects continuous logical (u,v,z) to map pixels. Integer (u,v) lands on the
// center of the top face of the block drawn in that staggered cell.
static Vector2 WorldToMapPixel(const cute_tiled_map_t *map, Vector2 world, float z)
{
    float sx = (world.x - world.y) * map->tilewidth * 0.5f + map->tilewidth * 0.5f;
    // Tiles are bottom-aligned, so a block's visible top face is centered one half
    // tile height ABOVE the cell origin (cell origin - tileheight + tileheight/2).
    float sy = (world.x + world.y) * map->tileheight * 0.5f - map->tileheight * 0.5f;
    sy -= z * map->tileheight;
    return { sx, sy };
}

// Staggered cell -> logical tile coordinates (integer).
static void TileCellToWorld(int tileX, int tileY, int *u, int *v)
{
    *u = tileX + (tileY + 1) / 2;
    *v = tileY / 2 - tileX;
}

// Continuous logical position -> staggered Tiled cell (nearest tile center).
// Does not bounds-check against the map; callers must.
static bool WorldToTileCell(Vector2 worldPosition, int *tileX, int *tileY)
{
    int u = (int)floorf(worldPosition.x + 0.5f);
    int v = (int)floorf(worldPosition.y + 0.5f);
    int ty = u + v;
    *tileY = ty;
    *tileX = (u - v - (ty & 1)) / 2;   // numerator is always even
    return true;
}

struct SurfaceHit
{
    bool found = false;
    bool walkable = false;
    float elevation = 0.0f;
    const cute_tiled_layer_t *layer = nullptr;
};

// Highest walkable surface under worldPosition with elevation <= currentZ + stepTolerance.
static SurfaceHit FindSurfaceBelow(const cute_tiled_map_t *map, Vector2 worldPosition,
                                   float currentZ, float stepTolerance)
{
    SurfaceHit best;
    int tx, ty;
    if (!WorldToTileCell(worldPosition, &tx, &ty)) return best;
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) return best;

    for (const cute_tiled_layer_t *l = map->layers; l; l = l->next)
    {
        if (!l->visible || !l->data || !l->type.ptr) continue;
        if (strcmp(l->type.ptr, "tilelayer") != 0) continue;
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


// ---------------------------------------------------------------------------
// Player + simulation
// ---------------------------------------------------------------------------
struct Player
{
    Vector2 position = {};      // continuous logical world coordinates (tile units)
    float z = 0.0f;             // elevation in levels
    float verticalVelocity = 0.0f;
    float moveSpeed = 3.0f;     // tiles / second
    float jumpSpeed = 7.0f;     // levels / second
    float radius = 5.0f;        // drawing radius (pixels)
    bool grounded = true;
};

static const float kGravity = 18.0f;        // levels / second^2
static const float kStepTolerance = 0.1f;   // max step-up treated as "same elevation"

static Vector2 NormalizeOrZero(Vector2 v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    return len > 0.0001f ? Vector2{ v.x / len, v.y / len } : Vector2{ 0, 0 };
}

// Places the player on the elevation-0 tile (with nothing stacked above) closest to the map center.
static void SpawnPlayer(const cute_tiled_map_t *map, Player *player)
{
    int bestX = map->width / 2, bestY = map->height / 2;
    float bestDist = 1e30f;
    for (const cute_tiled_layer_t *l = map->layers; l; l = l->next)
    {
        if (!l->data || !l->type.ptr || strcmp(l->type.ptr, "tilelayer") != 0) continue;
        if (!LayerHasElevation(l) || GetLayerElevation(l) != 0) continue;
        for (int y = 0; y < l->height; y++)
            for (int x = 0; x < l->width; x++)
            {
                if (cute_tiled_unset_flags(l->data[y * l->width + x]) == 0) continue;
                int u, v;
                TileCellToWorld(x, y, &u, &v);
                SurfaceHit top = FindSurfaceBelow(map, { (float)u, (float)v }, 1000.0f, 0.0f);
                if (!top.found || top.elevation != 0.0f) continue;
                float dx = (float)(x - map->width / 2), dy = (float)(y - map->height / 2);
                float d = dx * dx + dy * dy;
                if (d < bestDist) { bestDist = d; bestX = x; bestY = y; }
            }
        break;
    }
    int u, v;
    TileCellToWorld(bestX, bestY, &u, &v);
    *player = Player();
    player->position = { (float)u, (float)v };
}

static SurfaceHit UpdatePlayer(const cute_tiled_map_t *map, Player *p, float dt)
{
    // 1. WASD (screen space) -> logical world direction
    Vector2 input = {};
    if (IsKeyDown(KEY_A)) input.x -= 1;
    if (IsKeyDown(KEY_D)) input.x += 1;
    if (IsKeyDown(KEY_W)) input.y -= 1;
    if (IsKeyDown(KEY_S)) input.y += 1;
    input = NormalizeOrZero(input);
    Vector2 dir = NormalizeOrZero({ input.x + input.y, input.y - input.x });

    // Horizontal move. Stepping up onto a higher surface is disallowed for now.
    Vector2 next = { p->position.x + dir.x * p->moveSpeed * dt, p->position.y + dir.y * p->moveSpeed * dt };
    SurfaceHit top = FindSurfaceBelow(map, next, 1000.0f, 0.0f);
    if (!(top.found && top.elevation > p->z + kStepTolerance))
        p->position = next;

    // 2. Jump
    if (p->grounded && IsKeyPressed(KEY_SPACE))
    {
        p->verticalVelocity = p->jumpSpeed;
        p->grounded = false;
    }

    // 3. Vertical
    float prevZ = p->z;
    if (!p->grounded)
    {
        p->verticalVelocity -= kGravity * dt;
        p->z += p->verticalVelocity * dt;
    }

    // 4. Surface lookup (from the height we were at before this frame's motion)
    SurfaceHit surface = FindSurfaceBelow(map, p->position, prevZ, kStepTolerance);
    if (p->grounded)
    {
        if (surface.found)
        {
            p->z = surface.elevation;
            p->verticalVelocity = 0.0f;
        }
        else
        {
            p->grounded = false;   // walked off an edge: fall from current z
        }
    }
    else if (p->verticalVelocity <= 0.0f && surface.found && p->z <= surface.elevation)
    {
        p->z = surface.elevation;
        p->verticalVelocity = 0.0f;
        p->grounded = true;
    }

    // Fell out of the world: respawn
    if (p->z < -10.0f)
    {
        SpawnPlayer(map, p);
        surface = FindSurfaceBelow(map, p->position, p->z, kStepTolerance);
    }
    return surface;
}

static void DrawPlayer(const cute_tiled_map_t *map, const Player &p, const SurfaceHit &surface)
{
    float groundZ = surface.found ? surface.elevation : p.z;
    Vector2 shadow = WorldToMapPixel(map, p.position, groundZ);
    Vector2 body = WorldToMapPixel(map, p.position, p.z);
    DrawEllipse((int)shadow.x, (int)shadow.y, p.radius, p.radius * 0.5f, Fade(BLACK, 0.4f));
    DrawCircleV(body, p.radius, RED);
}



static void DrawTileLayer(const cute_tiled_map_t *map, const cute_tiled_layer_t *layer,
                          const TilesetInfo &ts, Rectangle view,
                          const Shader *cutShader = nullptr,
                          int rowBegin = 0, int rowEnd = 0x7fffffff)
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

    Shader cutoutShader = LoadShader("shaders/cutout.vs", "shaders/cutout.fs");   // relative to resources/
    Player player;
    SurfaceHit surface;
    bool followPlayer = false;
    if (map)
    {
        SpawnPlayer(map, &player);
        surface = FindSurfaceBelow(map, player.position, player.z, kStepTolerance);
        player.z = surface.found ? surface.elevation : 0.0f;
    }

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;   // avoid huge steps after hitches
        if (map && tilesetOk)
        {
            surface = UpdatePlayer(map, &player, dt);
            if (IsKeyPressed(KEY_F)) followPlayer = !followPlayer;
            if (followPlayer)
            {
                Vector2 t = WorldToMapPixel(map, player.position, player.z);
                camera.target.x += (t.x - camera.target.x) * 0.1f;
                camera.target.y += (t.y - camera.target.y) * 0.1f;
            }
        }
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
            // Cutout hole around the player (map-pixel space), applied to layers above the feet.
            Vector2 holeCenter = WorldToMapPixel(map, player.position, player.z);
            Vector2 holeRadii = { player.radius * 3.5f, player.radius * 2.5f };
            float holeMinAlpha = 0.0f;   // 0 = fully transparent
            SetShaderValue(cutoutShader, GetShaderLocation(cutoutShader, "holeCenter"), &holeCenter, SHADER_UNIFORM_VEC2);
            SetShaderValue(cutoutShader, GetShaderLocation(cutoutShader, "holeRadii"), &holeRadii, SHADER_UNIFORM_VEC2);
            SetShaderValue(cutoutShader, GetShaderLocation(cutoutShader, "holeMinAlpha"), &holeMinAlpha, SHADER_UNIFORM_FLOAT);

            // Draw order:
            //  1. everything behind or beside the player: all rows of layers at/below the feet,
            //     and rows up to the player's row on higher layers (drawn normally)
            //  2. the player
            //  3. higher layers' rows in FRONT of the player (rows after theirs), through the
            //     cutout shader so the player shows through
            int pcx, pcy;
            WorldToTileCell(player.position, &pcx, &pcy);
            for (int pass = 0; pass < 3; pass++)
            {
                if (pass == 1) { DrawPlayer(map, player, surface); continue; }
                for (cute_tiled_layer_t *l = map->layers; l; l = l->next)
                {
                    if (!l->visible || !l->data || !l->type.ptr) continue;
                    if (strcmp(l->type.ptr, "tilelayer") != 0) continue;
                    bool above = (float)GetLayerElevation(l) > player.z + 0.01f;
                    if (pass == 0)
                        DrawTileLayer(map, l, tileset, view, nullptr, 0, above ? pcy + 1 : 0x7fffffff);
                    else if (above)
                        DrawTileLayer(map, l, tileset, view, &cutoutShader, pcy + 1);
                }
            }
            EndMode2D();

            int cx, cy;
            WorldToTileCell(player.position, &cx, &cy);
            DrawText(TextFormat("world %.2f, %.2f  z %.2f  vz %.2f", player.position.x, player.position.y,
                                player.z, player.verticalVelocity), 20, 80, 20, LIGHTGRAY);
            DrawText(TextFormat("grounded %d  surface %s %.0f  cell %d, %d", player.grounded,
                                surface.found ? "elev" : "none", surface.elevation, cx, cy),
                     20, 105, 20, LIGHTGRAY);
        }
        else
        {
            DrawText("Failed to load map (see log)", 100, 100, 40, RED);
        }

        DrawText("WASD: move  Space: jump  Arrows: pan  F: follow  Wheel: zoom  ESC: exit", 20, 20, 20, LIGHTGRAY);
        DrawFPS(20, 50);
        EndDrawing();
    }

    UnloadShader(cutoutShader);
    if (tilesetOk) UnloadTexture(tileset.texture);
    if (map) cute_tiled_free_map(map);
    CloseWindow();
    return 0;
}