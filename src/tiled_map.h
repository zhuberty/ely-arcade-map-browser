// Thin wrapper around cute_tiled plus helpers for reading layer data.
#pragma once

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "cute_tiled.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// True if the layer is a visible tile layer with data.
bool IsDrawableTileLayer(const cute_tiled_layer_t *layer);

// True if the layer has an integer "elevation" property.
bool LayerHasElevation(const cute_tiled_layer_t *layer);

// Value of the layer's "elevation" property (0 if missing).
int GetLayerElevation(const cute_tiled_layer_t *layer);

// Dumps map/tileset/layer details to the raylib log.
void LogMapInfo(cute_tiled_map_t *map);
