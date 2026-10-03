#include "player.h"

#include <math.h>
#include <string.h>

static Vector2 NormalizeOrZero(Vector2 v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    return len > 0.0001f ? Vector2{ v.x / len, v.y / len } : Vector2{ 0, 0 };
}

void SpawnPlayer(const cute_tiled_map_t *map, Player *player)
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

SurfaceHit UpdatePlayer(const cute_tiled_map_t *map, Player *p, float dt)
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
        if (surface.found && surface.elevation >= p->z - kStepTolerance)
        {
            p->z = surface.elevation;
            p->verticalVelocity = 0.0f;
        }
        else
        {
            p->grounded = false;   // walked off an edge (or onto a lower tile): fall from current z
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

void DrawPlayer(const cute_tiled_map_t *map, const Player &p, const SurfaceHit &surface)
{
    float groundZ = surface.found ? surface.elevation : p.z;
    Vector2 shadow = WorldToMapPixel(map, p.position, groundZ);
    Vector2 body = WorldToMapPixel(map, p.position, p.z);
    DrawEllipse((int)shadow.x, (int)shadow.y, p.radius, p.radius * 0.5f, Fade(BLACK, 0.4f));
    DrawCircleV(body, p.radius, RED);
}
