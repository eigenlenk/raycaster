#ifndef RAYCAST_MAP_BUILDER_POLYGON_INCLUDED
#define RAYCAST_MAP_BUILDER_POLYGON_INCLUDED

#include "types.h"
#include "linedef.h"

#define VERTICES(...) M_ARRAY(vec2f, __VA_ARGS__)

#define LINES(...) M_ARRAY(polygon_line, __VA_ARGS__)

/* All the walls use the default options */
#define UNIFORM_LINES() 0, NULL

#define POLYGON_CLOCKWISE_WINDING(POLY) (polygon_signed_area(POLY) < 0)

#define VERT_OPTION_ANY 1
#define VERT_ANY VEC2F(M_FLAGGED_NAN(VERT_OPTION_ANY), 0)

#define LINE_CONFIG(V0,V1,CONFIG) ((polygon_line) { V0, V1, CONFIG })

#define SIDE_CONFIG(...) ((struct side_config) { __VA_ARGS__ })

typedef struct polygon_line {
  vec2f v0, v1;
  struct side_config {
    texture_ref texture_top,
                texture_middle,
                texture_bottom;
    linedef_flags flags;
  } side;
} polygon_line;

typedef struct polygon {
  struct side_config default_side_config;
  int32_t       floor_height,
                ceiling_height;
  float         brightness;
  texture_ref   floor_texture,
                ceiling_texture;
  size_t        vertices_count, original_vertices_count, lines_count;
  vec2f         *original_vertices;
  vec2f         *vertices;
  polygon_line  *lines;
} polygon;

bool
polygon_vertices_contains_point(const polygon*, vec2f);

bool
polygon_is_point_inside(const polygon*, vec2f, bool);

bool
polygon_overlaps_polygon(const polygon*, const polygon*);

bool
polygon_contains_polygon(const polygon*, const polygon*, bool);

float
polygon_signed_area(const polygon*);

struct side_config*
polygon_line_config(const polygon*, vec2f, vec2f);

void
polygon_insert_point(polygon*, vec2f, vec2f, vec2f);

void
polygon_remove_point(polygon*, vec2f);

void
polygon_reverse_vertices(polygon*);

void
polygon_add_line(polygon*, vec2f, vec2f, const struct side_config);

#endif
