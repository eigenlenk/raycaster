#ifndef RAYCAST_LEVEL_DATA_INCLUDED
#define RAYCAST_LEVEL_DATA_INCLUDED

#include "sector.h"

struct polygon;

typedef struct level_data {
  size_t sectors_count,
         linedefs_count,
         vertices_count;
  vertex vertices[16384];
  linedef linedefs[8192];
  sector sectors[2048];
} level_data;

vertex*
level_data_get_vertex(level_data*, vec2f);

linedef*
level_data_get_linedef(level_data*, sector*, vertex*, vertex*);

sector*
level_data_create_sector_from_polygon(level_data*, struct polygon*);

#endif
