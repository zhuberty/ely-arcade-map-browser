// ely-arcade-map-browser
//
// Press ESC to exit back to the arcade menu.
// The menu launches this as a child process and waits for it to terminate.

#include "raylib.h"
#include "resource_dir.h"
#include "arcade_input.h"

#include <string.h>

#include "map_renderer.h"
#include "player.h"
#include "tiled_map.h"
#include "tileset.h"
#include "world.h"

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
