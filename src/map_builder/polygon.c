#include "polygon.h"
#include "maths.h"
#include <stdio.h>

#define XY(V) (int)V.x, (int)V.y

bool
polygon_vertices_contains_point(const polygon *this, vec2f point)
{
  register size_t i;

  for (i = 0; i < this->vertices_count; ++i) {
    if (VEC2F_EQUAL(this->vertices[i], point)) {
      return true;
    }
  }

  return false;
}

bool
polygon_is_point_inside(const polygon *this, vec2f point, bool include_edges)
{
  register size_t i;
  int wn = 0;
  vec2f v0, v1;

  /* Winding number algorithm */
  for (i = 0; i < this->vertices_count; ++i) {
    v0 = this->vertices[i];
    v1 = this->vertices[(i+1)%this->vertices_count];

    if (math_point_on_line_segment(point, v0, v1, MATHS_EPSILON)) {
      return include_edges;
    }

    if (v0.y <= point.y) {
      if (v1.y > point.y) {
        if (math_sign(v0, v1, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (v1.y <= point.y) {
        if (math_sign(v0, v1, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn == 1 || wn == -1;
}

bool
polygon_overlaps_polygon(const polygon *this, const polygon *other)
{
  size_t i, j, i2, j2;

  for (i = 0; i < other->vertices_count; ++i) {
    if (polygon_vertices_contains_point(this, other->vertices[i])) {
      continue;
    }
    if (polygon_is_point_inside(this, other->vertices[i], true)) {
      return true;
    }
    i2 = (i + 1) % other->vertices_count;
    for (j = 0; j < this->vertices_count; ++j) {
      j2 = (j + 1) % this->vertices_count;
      if (VEC2F_EQUAL(other->vertices[i],   this->vertices[j])  ||
          VEC2F_EQUAL(other->vertices[i2],  this->vertices[j])  ||
          VEC2F_EQUAL(other->vertices[i],   this->vertices[j2]) ||
          VEC2F_EQUAL(other->vertices[i2],  this->vertices[j2])
      ) { continue; }
      if (math_find_line_intersection(
        other->vertices[i],
        other->vertices[i2],
        this->vertices[j],
        this->vertices[j2],
        NULL,
        NULL
      )) { return true; }
    }
  }

  return false;
}

bool
polygon_contains_polygon(const polygon *this, const polygon *other, bool include_edges)
{
  size_t i;

  /* All points of 'other' must be inside 'this' */
  for (i = 0; i < other->vertices_count; ++i) {
    if (!polygon_is_point_inside(this, other->vertices[i], include_edges)) {
      return false;
    }
  }

  return true;
}

float
polygon_signed_area(const polygon *this)
{
  size_t i;
  float area = 0.f;
  
  for (i = 0; i < this->vertices_count; ++i) {
    vec2f v0 = this->vertices[i];
    vec2f v1 = this->vertices[(i + 1) % this->vertices_count];
    area += math_cross(v0, v1);
  }

  return area * 0.5;
}

struct side_config*
polygon_line_config(const polygon *this, vec2f v0, vec2f v1)
{
  /*
   * Find best line config for these veritces in the order of preference:
   *
   * 1. Explicit config for these 2 vertices
   * 2. Line config matching one vertex
   * 3. Default line config
   */

  size_t i;
  polygon_line *best_match = NULL, *partial_match = NULL;

  for (i = 0; i < this->lines_count; ++i) {
    polygon_line *config = &this->lines[i];

    bool v0_any = M_FLAGGED_NAN_CHECK(config->v0.x, VERT_OPTION_ANY);
    bool v1_any = M_FLAGGED_NAN_CHECK(config->v1.x, VERT_OPTION_ANY);

    if (!v0_any && !v1_any && ((VEC2F_EQUAL(config->v0, v0) && VEC2F_EQUAL(config->v1, v1)) || (VEC2F_EQUAL(config->v0, v1) && VEC2F_EQUAL(config->v1, v0)))) {
      best_match = config;
      break;
    }

    bool either_vertex_matches = (!v0_any && VEC2F_EQUAL(config->v0, v0)) ||
      (!v1_any && VEC2F_EQUAL(config->v0, v1)) ||
      (!v0_any && VEC2F_EQUAL(config->v1, v0)) ||
      (!v1_any && VEC2F_EQUAL(config->v1, v1));

    if ((either_vertex_matches && (v0_any || v1_any)) || (v0_any && v1_any)) {
      partial_match = config;
    }
  }

  return best_match
    ? &best_match->side
    : partial_match
      ? &partial_match->side
      : (struct side_config*)&this->default_side_config;
}

void
polygon_insert_point(polygon *this, vec2f point, vec2f after, vec2f before)
{
  register size_t i,i2,j;

  for (i = 0; i < this->vertices_count; ++i) {
    i2 = (i + 1) % this->vertices_count;
    if ((VEC2F_EQUAL(this->vertices[i], after) && VEC2F_EQUAL(this->vertices[i2], before)) ||
        (VEC2F_EQUAL(this->vertices[i], before) && VEC2F_EQUAL(this->vertices[i2], after))) {
      this->vertices = realloc(this->vertices, (this->vertices_count+1)*sizeof(vec2f));
      for (j = this->vertices_count; j > i + 1; --j) {
        this->vertices[j] = this->vertices[j - 1];
      }
      this->vertices[i + 1] = point;
      this->vertices_count++;
      break;
    }
  }
}

void
polygon_remove_point(polygon *this, vec2f point)
{
  size_t i, j;

  for (i = 0; i < this->vertices_count; ++i) {
    if (VEC2F_EQUAL(this->vertices[i], point)) {
      for (j = i; j < this->vertices_count-1; ++j) {
        this->vertices[j] = this->vertices[j+1];
      }
      this->vertices = realloc(this->vertices, (--this->vertices_count)*sizeof(vec2f));
      break;
    }
  }
}

void
polygon_reverse_vertices(polygon *this)
{
  int i,j, w = this->vertices_count / 2;
  vec2f temp_swap;
  for (i = 0; i < w; ++i) {
    j = this->vertices_count-1-i;
    temp_swap = this->vertices[j];
    this->vertices[j] = this->vertices[i];
    this->vertices[i] = temp_swap;
  }
}

void
polygon_add_line(polygon *this, vec2f v0, vec2f v1, const struct side_config config)
{
  printf("\t\tAdd new line config: (%d, %d) <-> (%d, %d)\n", XY(v0), XY(v1));

  if (!this->lines) {
    this->lines = malloc(sizeof(polygon_line));
  } else {
    this->lines = realloc(this->lines, (this->lines_count+1) * sizeof(polygon_line));
  }

  this->lines[this->lines_count++] = (polygon_line) {
    .v0 = v0,
    .v1 = v1,
    .side = {
      .texture_top = config.texture_top,
      .texture_middle = config.texture_middle,
      .texture_bottom = config.texture_bottom,
      .flags = config.flags
    }
  };
}
