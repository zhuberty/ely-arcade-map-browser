#include "tileset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool LoadExternalTileset(const char *mapDir, cute_tiled_tileset_t *ts, TilesetInfo *info)
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
