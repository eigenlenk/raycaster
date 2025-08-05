#include "level_data.h"
#include "polygon.h"
#include <assert.h>

#define XY(V) (int)V.x, (int)V.y

static linedef*
level_data_get_linedef(level_data*, vertex*, vertex*, int*);

static bool
linedef_segment_contains_light(const linedef_segment*, const light*);

/* FIND a vertex at given point OR CREATE a new one */
vertex*
level_data_get_vertex(level_data *this, vec2f point)
{
  register size_t i;

  if (!this->vertices_count) {
    this->min = VEC2F(FLT_MAX, FLT_MAX);
    this->max = VEC2F(-FLT_MAX, -FLT_MAX);
  }

  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i].point, point)) < 1) {
      return &this->vertices[i];
    }
  }

  this->vertices[this->vertices_count] = (vertex) {
    .point = point
  };

  if (point.x < this->min.x) { this->min.x = point.x; }
  if (point.y < this->min.y) { this->min.y = point.y; }
  if (point.x > this->max.x) { this->max.x = point.x; }
  if (point.y > this->max.y) { this->max.y = point.y; }

  return &this->vertices[this->vertices_count++];
}

sector*
level_data_create_sector_from_polygon(level_data *this, polygon *poly)
{
  register size_t i;
  linedef *line;
  polygon_line *config;
  int side;

  sector *sect = &this->sectors[this->sectors_count++];

  IF_DEBUG(printf("\tNew sector (0x%p):\n", (void*)sect))

  sect->floor.height = poly->floor_height;
  sect->floor.texture = poly->floor_texture;
  sect->ceiling.height = poly->ceiling_height;
  sect->ceiling.texture = poly->ceiling_texture;
  sect->brightness = poly->brightness;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  sect->visible_linedefs = NULL;
  sect->visible_linedefs_count = 0;
#endif

  for (i = 0; i < poly->vertices_count; ++i) {
    vec2f v0 = poly->vertices[i];
    vec2f v1 = poly->vertices[(i+1)%poly->vertices_count];

    line = level_data_get_linedef(
      this,
      level_data_get_vertex(this, v0),
      level_data_get_vertex(this, v1),
      &side
    );

    linedef_configure_side(line, sect, poly, side);
    sector_add_linedef(sect, line);
    linedef_update_floor_ceiling_limits(line);
  }

  return sect;
}

light*
level_data_add_light(level_data *this, vec3f pos, float r, float s) {
  if (this->lights_count == 64) {
    return NULL;
  }

  light *new_light = &this->lights[this->lights_count++];

  new_light->entity = (entity) {
    .level = this,
    .sector = NULL,
    .position = VEC2F(pos.x, pos.y),
    .z = pos.z,
    .data = (void*)new_light,
    .type = ENTITY_LIGHT
  };

  new_light->radius = r;
  new_light->radius_sq = r*r;
  new_light->radius_sq_inverse = 1.f / new_light->radius_sq;
  new_light->strength = s;

  level_data_update_lights(this);
  map_cache_process_light(&this->cache, new_light, pos);

  return new_light;
}

void
level_data_update_lights(level_data *this)
{
  int i, si, li, segi, side;
  float sign;
  light *lite;
  sector *sect;
  linedef *line;
  linedef_segment *seg;
  vec2f pos2d;

  for (si = 0; si < this->sectors_count; ++si) {
    sect = &this->sectors[si];

    for (li = 0; li < sect->linedefs_count; ++li) {
      line = sect->linedefs[li];
      for (segi = 0; segi < line->segments; ++segi) {
        line->side[0].segments[segi].lights_count = 0;
        if (line->side[1].segments) {
          line->side[1].segments[segi].lights_count = 0;
        }
      }
    }
  }

  for (i = 0; i < this->lights_count; ++i) {
    lite = &this->lights[i];

    pos2d = VEC2F(lite->entity.position.x, lite->entity.position.y);

    /* Find all sectors the light circle touches */
    for (si = 0; si < this->sectors_count; ++si) {
      sect = &this->sectors[si];

      for (li = 0; li < sect->linedefs_count; ++li) {
        line = sect->linedefs[li];
        side = sect==line->side[0].sector?0:1;
        sign = math_sign(line->v0->point, line->v1->point, pos2d);

        for (segi = 0; segi < line->segments; ++segi) {
          seg = &line->side[side].segments[segi];

#ifdef RAYCASTER_DYNAMIC_SHADOWS
          /*
           * In dynamic shadow mode, a surface is lightable when the line simply
           * intersects the light circle. Pixel perfect ray check is performed
           * in the renderer later on.
           */
          if (math_line_segment_point_distance(seg->p0, seg->p1, pos2d) <= lite->radius) {
            if ((side == 0 ? (sign < 0) : (sign > 0)) &&
                seg->lights_count < MAX_LIGHTS_PER_SURFACE &&
                !linedef_segment_contains_light(seg, lite)
            ) {
              seg->lights[seg->lights_count++] = lite;
            }
          }
#else
          /*
           * In non-shadowed version, a wall segment is lit when either
           * vertex has a line of sight to the light.
           */
          if ((side == 0 ? (sign < 0) : (sign > 0)) &&
              seg->lights_count < MAX_LIGHTS_PER_SURFACE &&
              !linedef_segment_contains_light(seg, lite)
          ) {
            vec3f world_pos = entity_world_position(&lite->entity);

            if (!map_cache_intersect_3d(&this->cache, VEC3F(seg->p0.x, seg->p0.y, sect->floor.height), world_pos) ||
                !map_cache_intersect_3d(&this->cache, VEC3F(seg->p1.x, seg->p1.y, sect->floor.height), world_pos) ||
                !map_cache_intersect_3d(&this->cache, VEC3F(seg->p0.x, seg->p0.y, sect->ceiling.height), world_pos) ||
                !map_cache_intersect_3d(&this->cache, VEC3F(seg->p1.x, seg->p1.y, sect->ceiling.height), world_pos)
            ) {
              seg->lights[seg->lights_count++] = lite;
            }
          }
#endif
        }
      }
    }
  }
}

/* FIND a linedef with given vertices OR CREATE a new one */
static linedef*
level_data_get_linedef(level_data *this, vertex *v0, vertex *v1, int *side)
{
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = &this->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      IF_DEBUG(printf("\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Front: 0x%p)\n",
        (void*)line, XY(v0->point), XY(v1->point), (void*)line->side[0].sector
      ))
      *side = 1;
      return line;
    }
  }

  const float line_length = math_vec2f_distance(v0->point, v1->point);

  this->linedefs[this->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side[0] = {
      .sector = NULL,
      .flags = 0,
      .normal = math_normalize(math_vec2f_perpendicular(vec2f_sub(v1->point, v0->point)))
    },
    .side[1] = {
      .sector = NULL,
      .flags = 0,
      .normal = math_normalize(math_vec2f_perpendicular(vec2f_sub(v0->point, v1->point)))
    },
    .direction = vec2f_sub(v1->point, v0->point),
    .length = line_length,
    .segments = (uint16_t)ceilf(line_length * LINEDEF_SEGMENT_LENGTH_INV),
    .xmin = fminf(v0->point.x, v1->point.x),
    .xmax = fmaxf(v0->point.x, v1->point.x),
    .ymin = fminf(v0->point.y, v1->point.y),
    .ymax = fmaxf(v0->point.y, v1->point.y)
  };

  IF_DEBUG(printf("\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d)\n",
    (void*)&this->linedefs[this->linedefs_count], XY(v0->point), XY(v1->point)
  ))

  *side = 0;

  return &this->linedefs[this->linedefs_count++];
}

static bool
linedef_segment_contains_light(const linedef_segment *this, const light *lt)
{
  size_t i;
  for (i = 0; i < this->lights_count; ++i) {
    if (this->lights[i] == lt) {
      return true;
    }
  }
  return false;
}
