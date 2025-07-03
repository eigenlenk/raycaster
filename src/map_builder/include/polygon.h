#ifndef RAYCAST_MAP_BUILDER_POLYGON_INCLUDED
#define RAYCAST_MAP_BUILDER_POLYGON_INCLUDED

#include "types.h"

#define VERTICES(...) M_NARG(__VA_ARGS__), (vec2f[]) { __VA_ARGS__ }
#define POLYGON_CLOCKWISE_WINDING(POLY) (polygon_signed_area(POLY) < 0)

typedef struct polygon {
  int32_t floor_height,
          ceiling_height;
  float brightness;
  size_t vertices_count;
  vec2f *vertices;
} polygon;

bool
polygon_vertices_contains_point(const polygon*, vec2f);

bool
polygon_is_point_inside(const polygon*, vec2f, bool);

bool
polygon_overlaps_polygon(const polygon*, const polygon*);

bool
polygon_contains_polygon(const polygon*, const polygon*, bool);

void
polygon_insert_point(polygon*, vec2f, vec2f, vec2f);

void
polygon_remove_point(polygon*, vec2f);

void
polygon_reverse_vertices(polygon*);

float
polygon_signed_area(const polygon*);

#endif
