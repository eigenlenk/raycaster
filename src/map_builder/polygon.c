#include "polygon.h"
#include "maths.h"
#include <stdio.h>

bool polygon_vertices_contains_point(const polygon *this, vec2f point)
{
  register size_t i;
  for (i = 0; i < this->vertices_count; ++i) {
    if (VEC2F_EQUAL(this->vertices[i], point)) {
      return true;
    }
  }
  return false;
}

bool polygon_is_point_inside(const polygon *this, vec2f point, bool include_edges)
{
  register int i, wn = 0;
  vec2f v0, v1;

  /* Winding number algorithm */
  for (i = 0; i < this->vertices_count; ++i) {
    v0 = this->vertices[i];
    v1 = this->vertices[(i+1)%this->vertices_count];

    if (math_point_on_line_segment(point, v0, v1)) {
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

bool polygon_overlaps_polygon(const polygon *this, const polygon *other)
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
      if (math_line_segments_intersect(
        other->vertices[i],
        other->vertices[i2],
        this->vertices[j],
        this->vertices[j2]
      )) { return true; }
    }
  }

  return false;
}

bool polygon_contains_polygon(const polygon *this, const polygon *other, bool include_edges)
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

void polygon_insert_point(polygon *this, vec2f point, vec2f after, vec2f before)
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

void polygon_remove_point(polygon *this, vec2f point)
{
  size_t i, j;

  for (i = 0; i < this->vertices_count; ++i) {
    if (VEC2F_EQUAL(this->vertices[i], point)) {
      for (j = i; j < this->vertices_count; ++j) {
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

float polygon_signed_area(const polygon *this)
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
