#ifndef RAYCAST_LEVEL_DATA_INCLUDED
#define RAYCAST_LEVEL_DATA_INCLUDED

#include "sector.h"
#include "light.h"
#include "texture.h"
#include "map_cache.h"

struct polygon;

typedef struct level_data {
  size_t sectors_count,
         linedefs_count,
         vertices_count,
         lights_count;
  vertex vertices[16384];
  linedef linedefs[8192];
  sector sectors[2048];
  light lights[64];
  vec2f min,
        max;
  map_cache cache;
  texture_ref sky_texture;
} level_data;

vertex*
level_data_get_vertex(level_data*, vec2f);

sector*
level_data_create_sector_from_polygon(level_data*, struct polygon*);

light*
level_data_add_light(level_data*, vec3f, float, float);

void
level_data_update_lights(level_data*);

#endif
