#include "level_data.h"
#include "polygon.h"
#include <assert.h>

#define XY(V) (int)V.x, (int)V.y

static bool
linedef_contains_light(const linedef*, const light*);

static bool
sector_contains_light(const sector*, const light*);

/* Just for debbuging for now */
static uint32_t linedef_color = 0, sector_color = 0;

/* FIND a vertex at given point OR CREATE a new one */
vertex* level_data_get_vertex(level_data *this, vec2f point)
{
  register size_t i;

  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i].point, point)) < 1) {
      return &this->vertices[i];
    }
  }

  this->vertices[this->vertices_count] = (vertex) {
    .point = point
  };

  return &this->vertices[this->vertices_count++];
}

/* FIND a linedef with given vertices OR CREATE a new one */
linedef* level_data_get_linedef(level_data *this, sector *sect, vertex *v0, vertex *v1)
{
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = &this->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      line->side_sector[1] = sect;
      M_DEBUG(printf("\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d, Front: 0x%p, Back: 0x%p)\n",
        line, XY(v0->point), XY(v1->point), line->color, line->side_sector[0], line->side_sector[1]
      ));
      return line;
    }
  }

  this->linedefs[this->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side_sector[0] = sect,
    .side_sector[1] = NULL,
    .color = linedef_color++
  };

  M_DEBUG(printf("\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d, Front: 0x%p, Back: 0x%p)\n",
    &this->linedefs[this->linedefs_count], XY(v0->point), XY(v1->point), linedef_color-1, sect, NULL
  ));

  return &this->linedefs[this->linedefs_count++];
}

sector* level_data_create_sector_from_polygon(level_data *this, polygon *poly)
{
  register size_t i;

  sector *sect = &this->sectors[this->sectors_count++];

  M_DEBUG(printf("\tNew sector (0x%p):\n", sect));

  sect->floor_height = poly->floor_height;
  sect->ceiling_height = poly->ceiling_height;
  sect->light = poly->light;
  sect->color = sector_color++;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

  for (i = 0; i < poly->vertices_count; ++i) {
    sector_add_linedef(
      sect,
      level_data_get_linedef(
        this,
        sect,
        level_data_get_vertex(this, poly->vertices[i]),
        level_data_get_vertex(this, poly->vertices[(i+1)%poly->vertices_count])
      )
    );
  }

  return sect;
}

light*
level_data_add_light(level_data *this, vec3f pos, float r, float s) {
  int si, li;
  sector *sect;
  linedef *line;
  vec2f pos2d = VEC2F(pos.x, pos.y);

  if (this->lights_count == 64) {
    return NULL;
  }

  light *new_light = &this->lights[this->lights_count++];
  new_light->position = pos;
  new_light->radius = r;
  new_light->radius_sq = r*r;
  new_light->strength = s;

  /* Find all sectors the light circle touches */
  for (si = 0; si < this->sectors_count; ++si) {
    sect = &this->sectors[si];

    if (sector_point_inside(sect, VEC2F(pos.x, pos.y)) && sect->lights_count < MAX_LIGHTS_PER_SURFACE) {
      new_light->in_sector = sect;
      sect->lights[sect->lights_count++] = new_light;
      printf("Light inside sector %d\n", si);
    }

    for (li = 0; li < sect->linedefs_count; ++li) {
      line = sect->linedefs[li];

      if (math_line_segment_point_distance(line->v0->point, line->v1->point, pos2d) <= r) {
        if (!sector_contains_light(sect, new_light) && sect->lights_count < MAX_LIGHTS_PER_SURFACE) {
          printf("Light intersects sector %d\n", si);
          sect->lights[sect->lights_count++] = new_light;
        }

        if (!linedef_contains_light(line, new_light) && line->lights_count < MAX_LIGHTS_PER_SURFACE) {
          printf("Light touches line %d of sector %d\n", li, si);
          line->lights[line->lights_count++] = new_light;
        }
      }
    }
  }

  return new_light;
}

M_INLINED bool
sector_visited(sector *sect, size_t *n, sector **history)
{
  size_t i;
  if (*n == 8) {
    return true;
  }
  for (i = 0; i < *n; ++i) {
    if (history[i] == sect) {
      return true;
    }
  }
  history[*n] = sect;
  *n = *n+1;
  return false;
}

bool
level_data_intersect_3d(const level_data *this, vec3f p0, vec3f p1, const sector *start)
{
  assert(start);
  size_t li = 0, h=0;
  linedef *line;
  sector *sect = (sector*)start, *back;
  sector *history[8];
  vec2f p0_2 = VEC2F(p0.x, p0.y);
  vec2f p1_2 = VEC2F(p1.x, p1.y);
  float det, z, z0 = p0.z, z1 = p1.z;

  do {
    for (li = 0, back = NULL; li < sect->linedefs_count; ++li) {
      line = sect->linedefs[li];
      if (math_find_line_intersection(p0_2, p1_2, line->v0->point, line->v1->point, NULL, &det) && det > MATHS_EPSILON) {
        back = line->side_sector[0] == sect ? line->side_sector[1] : line->side_sector[0];
        if (!back) {
          return true; // det > MATHS_EPSILON;
        }
        z = z0+(z1-z0)*det;
        if (z < back->floor_height || z > back->ceiling_height) {
          return true;
        }
      }
    }
    sect = back;
  } while(sect && !sector_visited(sect, &h, history));

  return false;
}

static bool
linedef_contains_light(const linedef *this, const light *lt)
{
  size_t i;
  for (i = 0; i < this->lights_count; ++i) {
    if (this->lights[i] == lt) {
      return true;
    }
  }
  return false;
}

static bool
sector_contains_light(const sector *this, const light *lt)
{
  size_t i;
  for (i = 0; i < this->lights_count; ++i) {
    if (this->lights[i] == lt) {
      return true;
    }
  }
  return false;
}
