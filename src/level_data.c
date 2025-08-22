#include "level_data.h"
#include "polygon.h"
#include <gpc.h>
#include <assert.h>

#define XY(V) (int)V.x, (int)V.y

static struct {
  struct {
    size_t count;
    sector *list[32];
    sector *top;
  } sectors;
} open_context = { 0 };

static linedef*
level_data_get_linedef(level_data*, vertex*, vertex*, int*);

static bool
linedef_segment_contains_light(const linedef_segment*, const light*);

static void
subtract_top_sector_if_needed(void);

/* Maps vertices from polygon type to GPC polygon */
static void
sector_to_gpc_polygon(const sector *sect, gpc_polygon *gpc_poly);

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

sector*
level_data_begin_sector(
  level_data *this,
  int32_t floor_height,
  int32_t ceiling_height,
  float brightness,
  texture_ref floor_texture,
  texture_ref ceiling_texture
) {
  sector *sect = &this->sectors[this->sectors_count++];

  IF_DEBUG(printf("\tNew sector (0x%p, count: %d):\n", (void*)sect, this->sectors_count))

  sect->floor.height = floor_height;
  sect->floor.texture = floor_texture;
  sect->ceiling.height = ceiling_height;
  sect->ceiling.texture = ceiling_texture;
  sect->brightness = brightness;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  sect->visible_linedefs = NULL;
  sect->visible_linedefs_count = 0;
#endif

  open_context.sectors.list[open_context.sectors.count++] = sect;
  open_context.sectors.top = sect;

  return sect;
}

void
level_data_end_sector(void)
{
  assert(open_context.sectors.count > 0);
  open_context.sectors.count -= 1;
  if (open_context.sectors.count > 0) {
    open_context.sectors.top = open_context.sectors.list[open_context.sectors.count-1];
  } else {
    open_context.sectors.top = NULL;
  }
}

void
level_data_subtract_polygon(
  level_data *this,
  sector *sect,
  texture_ref texture_top,
  texture_ref texture_middle,
  texture_ref texture_bottom,
  linedef_flags flags,
  size_t vertices_count,
  vec2f vertices[]
) {
  sect = sect ? sect : open_context.sectors.top;
  assert(sect);



}

// void
// level_data_begin_polygon_clipping(void) { }

// void
// level_data_end_polygon_clipping(void) { }

void
level_data_update_sector_lines(level_data *this, sector *sect, size_t num_lines, line_dto lines[])
{
  sect = sect ? sect : open_context.sectors.top;
  assert(sect);

  size_t i;
  linedef *line;
  line_dto *dto;
  int side;
  bool switch_vertices = false, open_polygon=true;
  float signed_area;
  vec2f v0, v1, _v1;
  bool first_polygon = sect->linedefs_count == 0;

  for (i = 0, signed_area = 0; i < num_lines; ++i) {
    v0 = lines[i].v0;
    v1 = lines[i].v1;
    if (M_FLAGGED_NAN_CHECK(v0.x, VERT_OPTION_APPEND)) { v0 = _v1; }
    if (M_FLAGGED_NAN_CHECK(v1.x, VERT_OPTION_FINISH)) { v1 = lines[0].v0; }
    lines[i].v0 = v0;
    lines[i].v1 = v1;
    signed_area += math_cross(v0, v1);
    _v1 = v1;
    IF_DEBUG(printf("LINE: (%d,%d) <-> (%d,%d), %d, %d, %d, %d\n", XY(v0), XY(v1), lines[i].texture_top, lines[i].texture_middle, lines[i].texture_bottom, lines[i].flags))
    if (i == num_lines-1) {
      open_polygon = !VEC2F_EQUAL(v1, lines[0].v0);
    }
  }
  signed_area *= 0.5;
  switch_vertices = !open_polygon && num_lines > 2 && ((first_polygon && signed_area > 0) || (!first_polygon && signed_area < 0));

  for (i = 0; i < num_lines; ++i) {
    dto = &lines[i];

    line = level_data_get_linedef(
      this,
      level_data_get_vertex(this, switch_vertices ? dto->v1 : dto->v0),
      level_data_get_vertex(this, switch_vertices ? dto->v0 : dto->v1),
      &side
    );

    line->side[side].sector = sect;

    linedef_create_segments_for_side(line, side);

    line->side[side].flags = dto->flags;
    line->side[side].texture[LINE_TEXTURE_TOP] = dto->texture_top;
    line->side[side].texture[LINE_TEXTURE_BOTTOM] = dto->texture_bottom;

    if (side == 0) {
      line->side[0].texture[LINE_TEXTURE_MIDDLE] = dto->texture_middle;

      if (dto->flags & LINEDEF_FREESTANDING) {
        line->side[1].sector = sect;

        if (dto->flags & LINEDEF_DOUBLE_SIDED) {
          line->side[1].flags = dto->flags;
          line->side[1].texture[LINE_TEXTURE_MIDDLE] = dto->texture_middle;
        }
      }
    } else {
      if (dto->flags & LINEDEF_TRANSPARENT_MIDDLE_TEXTURE) {
        line->side[1].texture[LINE_TEXTURE_MIDDLE] = dto->texture_middle;
      } else if (line->side[0].flags & LINEDEF_TRANSPARENT_MIDDLE_TEXTURE) {
        if (line->side[0].flags & LINEDEF_DOUBLE_SIDED) {
          line->side[1].flags |= LINEDEF_TRANSPARENT_MIDDLE_TEXTURE | LINEDEF_DOUBLE_SIDED;
          line->side[1].texture[LINE_TEXTURE_MIDDLE] = line->side[0].texture[LINE_TEXTURE_MIDDLE] ;
        } else {
          line->side[1].texture[LINE_TEXTURE_MIDDLE] = TEXTURE_NONE;
        }
      } else {
        line->side[0].texture[LINE_TEXTURE_MIDDLE] = TEXTURE_NONE;
        line->side[1].texture[LINE_TEXTURE_MIDDLE] = TEXTURE_NONE;
      }
    }

    sector_add_linedef(sect, line);
    linedef_update_floor_ceiling_limits(line);
  }
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

static void
subtract_top_sector_if_needed(void)
{
  int ci, vi, external_contours;

  if (open_context.sectors.count < 2) {
    return;
  }

  printf("### subtract_top_sector_if_needed\n\n");

  sector *bottom_sector = open_context.sectors.list[open_context.sectors.count-2],
         *top_sector = open_context.sectors.top;
  gpc_polygon bottom_poly = {0}, top_poly = {0}, result = {0};

  sector_to_gpc_polygon(bottom_sector, &bottom_poly);
  sector_to_gpc_polygon(top_sector, &top_poly);

  printf("bottom poly: %d vertices\n", bottom_poly.contour[0].num_vertices);
  printf("top poly: %d vertices\n", top_poly.contour[0].num_vertices);

  gpc_polygon_clip(GPC_DIFF, &bottom_poly, &top_poly, &result);

  /* Read contours */
  for (ci = 0, external_contours = 0; ci < result.num_contours; ++ci) {
    if (!result.hole[ci]) {
      if (external_contours++ == 0) {
        printf("RESULTING COUNTOUR:\n");
        for (vi = 0; vi < result.contour[ci].num_vertices; ++vi) {
          printf("\t(%d, %d)\n", XY(result.contour[ci].vertex[vi]));
        }

        // pj->vertices_count = result.contour[ci].num_vertices;
        // pj->vertices = realloc(pj->vertices, pj->vertices_count * sizeof(vec2f));
        // for (vi = 0; vi < result.contour[ci].num_vertices; ++vi) {
        //   pj->vertices[vi] = VEC2F(result.contour[ci].vertex[vi].x, result.contour[ci].vertex[vi].y);
        // }
      } else {
        /* Create new polygon/sector from the other countour(s) */
        // map_builder_insert_polygon(
        //   this,
        //   j+1,
        //   pj->floor_height,
        //   pj->ceiling_height,
        //   pj->brightness,
        //   pj->default_side_config,
        //   pj->floor_texture,
        //   pj->ceiling_texture,
        //   result.contour[ci].num_vertices,
        //   result.contour[ci].vertex,
        //   GPC_VERTEX_LIST,
        //   0,
        //   NULL
        // );
      }
    }
  }

  gpc_free_polygon(&bottom_poly);
  gpc_free_polygon(&top_poly);
  gpc_free_polygon(&result);
}

static void
sector_to_gpc_polygon(const sector *sect, gpc_polygon *gpc_poly)
{
  size_t i;
  gpc_vertex_list contour;
  contour.vertex = (gpc_vertex*)malloc(sect->linedefs_count * sizeof(gpc_vertex));
  const vertex *vfirst = sect->linedefs[0]->v0;

  printf("Making contour:\n");

  for (i = 0; i < sect->linedefs_count; ++i) {
    contour.vertex[i] = (gpc_vertex) { sect->linedefs[i]->v0->point.x, sect->linedefs[i]->v0->point.y };

    printf("(%d, %d)\n", XY(contour.vertex[i]));

    if (i > 1 && (sect->linedefs[i]->v1 == vfirst || sect->linedefs[i]->v0 == vfirst)) {
      break;
    }
  }

  printf("num_vertices == %d\n", i);

  contour.vertex = realloc(contour.vertex, sizeof(gpc_vertex) * i);
  contour.num_vertices = i;

  gpc_add_contour(gpc_poly, &contour, 0);

  free(contour.vertex);
}
