// Player state, simulation and drawing.
#pragma once

#include "raylib.h"
#include "tiled_map.h"
#include "world.h"

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

constexpr float kGravity = 18.0f;        // levels / second^2
constexpr float kStepTolerance = 0.1f;   // max step-up treated as "same elevation"

// Places the player on the elevation-0 tile (with nothing stacked above) closest to the map center.
void SpawnPlayer(const cute_tiled_map_t *map, Player *player);

// Advances input, movement, jumping and gravity. Returns the surface under the player.
SurfaceHit UpdatePlayer(const cute_tiled_map_t *map, Player *p, float dt);

void DrawPlayer(const cute_tiled_map_t *map, const Player &p, const SurfaceHit &surface);
