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
  vertex vertices[32768];
  linedef linedefs[16384];
  sector sectors[8192];
  light lights[64];
  vec2f min,
        max;
  map_cache cache;
  texture_ref sky_texture;
  float brightness;
} level_data;

typedef struct line_dto {
  vec2f v0, v1;
  texture_ref texture_top,
              texture_middle,
              texture_bottom;
  linedef_flags flags;
} line_dto;

#define VERT_OPTION_APPEND 1
#define VERT_OPTION_FINISH 2
#define VERT_APPEND VEC2F(M_FLAGGED_NAN(VERT_OPTION_APPEND), 0)
#define VERT_FINISH VEC2F(M_FLAGGED_NAN(VERT_OPTION_FINISH), 0)

#define LINE(T, F, V0, V1)        ((struct line_dto) { V0, V1, T, F })
#define LINE_CREATE(T, F, V0, V1) ((struct line_dto) { V0, V1, T, F })
#define LINE_APPEND(T, F,     V1) ((struct line_dto) { VERT_APPEND, V1, T, F })
#define LINE_FINISH(T, F        ) ((struct line_dto) { VERT_APPEND, VERT_FINISH, T, F })

M_INLINED level_data*
level_data_allocate(void)
{
  level_data *new_data = malloc(sizeof(level_data));
  new_data->sectors_count = 0;
  new_data->linedefs_count = 0;
  new_data->vertices_count = 0;
  new_data->lights_count = 0;
  new_data->sky_texture = TEXTURE_NONE;
  new_data->brightness = 0; /* Global adjustent on sector brightness */
  return new_data;
}

vertex*
level_data_get_vertex(level_data*, vec2f);

sector*
level_data_create_sector_from_polygon(level_data*, struct polygon*);

sector*
level_data_begin_sector(level_data*, int32_t, int32_t, float, texture_ref, texture_ref);

void
level_data_end_sector(void);

void
level_data_update_sector_lines(level_data*, sector*, size_t, line_dto[]);

light*
level_data_add_light(level_data*, vec3f, float, float);

void
level_data_update_lights(level_data*);

#endif
