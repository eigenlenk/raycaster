#ifndef RAYCAST_MAP_BUILDER_INCLUDED
#define RAYCAST_MAP_BUILDER_INCLUDED

#include "polygon.h"

struct level_data;

typedef struct {
  size_t polygons_count;
  polygon *polygons;
} map_builder;

polygon*
map_builder_add_polygon(
  map_builder*,
  int32_t floor_height,
  int32_t ceiling_height,
  float brightness,
  texture_ref floor_texture,
  texture_ref ceiling_texture,
  size_t vertices_count,
  vec2f vertices[],
  struct side_config default_side_config,
  size_t lines_count,
  polygon_line lines[]
);

struct level_data*
map_builder_build(map_builder*);

void
map_builder_free(map_builder*);

M_INLINED polygon*
map_builder_polygon_at_point(map_builder *this, vec2f point)
{
  int i;
  for (i = this->polygons_count - 1; i >= 0; --i) {
    if (polygon_is_point_inside(&this->polygons[i], point, true)) {
      return &this->polygons[i];
    }
  }
  return NULL;
}

#endif
