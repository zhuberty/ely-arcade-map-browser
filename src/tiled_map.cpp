#include "tiled_map.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Single translation unit that holds the cute_tiled implementation.
#define CUTE_TILED_NO_EXTERNAL_TILESET_WARNING
#define CUTE_TILED_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "cute_tiled.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

bool IsDrawableTileLayer(const cute_tiled_layer_t *layer)
{
    return layer->visible && layer->data && layer->type.ptr &&
           strcmp(layer->type.ptr, "tilelayer") == 0;
}

bool LayerHasElevation(const cute_tiled_layer_t *layer)
{
    for (int i = 0; i < layer->property_count; i++)
    {
        const cute_tiled_property_t *p = &layer->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT && p->name.ptr && strcmp(p->name.ptr, "elevation") == 0)
            return true;
    }
    return false;
}

int GetLayerElevation(const cute_tiled_layer_t *layer)
{
    for (int i = 0; i < layer->property_count; i++)
    {
        const cute_tiled_property_t *p = &layer->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT && p->name.ptr && strcmp(p->name.ptr, "elevation") == 0)
            return p->data.integer;
    }
    return 0;
}

void LogMapInfo(cute_tiled_map_t *map)
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
