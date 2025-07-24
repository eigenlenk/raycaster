#ifndef RAYCAST_MATH_INCLUDED
#define RAYCAST_MATH_INCLUDED

#include "macros.h"
#include "types.h"

#define MATHS_EPSILON 1e-5f
#define PRECISION_LOW 1e-2f

M_INLINED float
math_max(float a, float b) {
  return fmaxf(a, b);
}

M_INLINED float
math_min(float a, float b) {
  return fminf(a, b);
}

M_INLINED float
math_clamp(float v, float lo, float hi) {
  return fmaxf(fminf(v, hi), lo);
}

M_INLINED float
math_cross(vec2f a, vec2f b) {
  return (a.x * b.y) - (a.y * b.x);
}

M_INLINED float
math_dot(vec2f v) {
  return (v.x * v.x + v.y * v.y);
}

M_INLINED float
math_dot2(vec2f a, vec2f b) {
  return (a.x * b.x + a.y * b.y);
}

M_INLINED float
math_length(vec2f v) {
  return sqrtf(math_dot(v));
}

M_INLINED float
math_vec2f_distance(vec2f p0, vec2f p1) {
  return math_length(vec2f_sub(p1, p0));
}

M_INLINED float
math_sign(vec2f p0, vec2f p1, vec2f point) {
  /* > 0 = is left of the line*/
  return ((p1.x - p0.x) * (point.y - p0.y) - (point.x - p0.x) * (p1.y - p0.y));
}

M_INLINED float
math_vec3_dot(vec3f v) {
  return (v.x * v.x + v.y * v.y + v.z * v.z);
}

M_INLINED float
math_vec3_distance_squared(vec3f a, vec3f b) {
  return math_vec3_dot(vec3f_sub(b, a));
}

M_INLINED float
math_vec3_distance(vec3f a, vec3f b)
{
  return sqrtf(math_vec3_dot(vec3f_sub(b, a)));
}

M_INLINED vec2f
math_vec2f_perpendicular(vec2f v)
{
  return VEC2F(v.y, -v.x);
}

M_INLINED vec2f
math_normalize(vec2f v)
{
  return vec2f_div(v, math_length(v));
}

M_INLINED bool
math_find_line_intersection(
  vec2f A,
  vec2f B,
  vec2f C,
  vec2f D,
  vec2f *r,
  float *determinant
) {
  register const vec2f BA = vec2f_sub(B, A);
  register const vec2f DC = vec2f_sub(D, C);
  register const vec2f AC = vec2f_sub(A, C);

  register float cross = math_cross(BA, DC);

  if (fabsf(cross) < MATHS_EPSILON) {
    return false;
  }

  register float denom = 1.f / cross;
  register float uA, uB;

  // calculate the direction of the lines
  uB = math_cross(BA, AC) * denom;

  if (uB < 0.f || uB > 1.f) {
    return false;
  }

  uA = math_cross(DC, AC) * denom;

  if (uA < 0.f || uA > 1.f) {
    return false;
  }

  // if uA and uB are between [0...1], lines are colliding
  if (r) {
    (*r).x = A.x + (uA * BA.x);
    (*r).y = A.y + (uA * BA.y);
  }

  if (determinant) {
    *determinant = uA;
  }

  return true;
}

/* Slightly more optimised ray check where line directions have been precalculated */
M_INLINED bool
math_find_line_intersection_cached(
  vec2f A,
  vec2f C,
  vec2f BA,
  vec2f DC,
  vec2f *r,
  float *det_a,
  float *det_b
) {
  register const vec2f AC = vec2f_sub(A, C);
  register float cross = math_cross(BA, DC);

  if (fabsf(cross) < MATHS_EPSILON) {
    return false;
  }

  register float denom = 1.f / cross;
  register float uA, uB;

  // calculate the direction of the lines
  uB = math_cross(BA, AC) * denom;

  if (uB < 0.f || uB > 1.f) {
    return false;
  }

  uA = math_cross(DC, AC) * denom;

  if (uA < 0.f || uA > 1.f) {
    return false;
  }

  // if uA and uB are between [0...1], lines are colliding
  if (r) {
    (*r).x = A.x + (uA * BA.x);
    (*r).y = A.y + (uA * BA.y);
  }

  if (det_a) {
    *det_a = uA;
  }

  if (det_b) {
    *det_b = uB;
  }

  return true;
}

M_INLINED bool
math_point_in_triangle(vec2f point, vec2f v0, vec2f v1, vec2f v2) {
  register float d0 = math_sign(point, v0, v1), d1 = math_sign(point, v1, v2), d2 = math_sign(point, v2, v0);
  return !(((d0 < 0) || (d1 < 0) || (d2 < 0)) && ((d0 > 0) || (d1 > 0) || (d2 > 0)));
}

M_INLINED float
math_line_segment_point_perpendicular_distance(vec2f a, vec2f b, vec2f point) {
  return fabsf(math_cross(vec2f_sub(b, a), vec2f_sub(a, point))) / math_length(vec2f_sub(b, a));
}

M_INLINED float
math_line_segment_point_distance(vec2f a, vec2f b, vec2f point) {
  vec2f ab = vec2f_sub(b, a);
  vec2f ap = vec2f_sub(point, a);
  float ab_len2 = math_dot(ab);
  if (ab_len2 <= MATHS_EPSILON) {
    return math_length(vec2f_sub(point, a));
  }
  float t = math_dot2(ap, ap) / ab_len2;
  t = fmaxf(0.0f, fminf(1.0f, t));
  return math_length(vec2f_sub(point, VEC2F(a.x + t * ab.x, a.y + t * ab.y)));
}

M_INLINED bool
math_point_on_line_segment(vec2f P, vec2f B, vec2f A, float precision) {
  const vec2f BA = vec2f_sub(B, A);
  const vec2f PA = vec2f_sub(P, A);
  const float cross = math_cross(BA, PA);

  if (fabsf(cross) > precision) {
    return false;
  }

  const float dot = math_dot2(BA, PA);

  if (dot < 0.0f) {
    return false;
  }

  if (dot > math_dot(BA)) {
    return false;
  }

  return true;
}

#endif
