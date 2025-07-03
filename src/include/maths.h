#ifndef RAYCAST_MATH_INCLUDED
#define RAYCAST_MATH_INCLUDED

#include "macros.h"
#include "types.h"

#define MATHS_EPSILON 1e-5f

M_INLINED float math_max(float a, float b) {
  return fmaxf(a, b);
}

M_INLINED float math_min(float a, float b) {
  return fminf(a, b);
}

M_INLINED float math_clamp(float v, float lo, float hi) {
  return fmaxf(fminf(v, hi), lo);
}

M_INLINED float math_cross(vec2f a, vec2f b) {
  return (a.x * b.y) - (a.y * b.x);
}

M_INLINED float math_dot(vec2f v) {
  return (v.x * v.x + v.y * v.y);
}

M_INLINED float math_dot2(vec2f a, vec2f b) {
  return (a.x * b.x + a.y * b.y);
}

M_INLINED float math_length(vec2f v) {
  return sqrtf(math_dot(v));
}

M_INLINED int32_t math_sign(vec2f p0, vec2f p1, vec2f point) {
  /* > 0 = is left of the line*/
  return (int32_t)((p1.x - p0.x) * (point.y - p0.y) - (point.x - p0.x) * (p1.y - p0.y));
}

M_INLINED float math_vec3_dot(vec3f v) {
  return (v.x * v.x + v.y * v.y + v.z * v.z);
}

M_INLINED float math_vec3_distance_squared(vec3f a, vec3f b) {
  return math_vec3_dot(vec3f_sub(b, a));
}

M_INLINED float math_vec3_distance(vec3f a, vec3f b) {
  return sqrtf(math_vec3_dot(vec3f_sub(b, a)));
}

M_INLINED bool math_find_line_intersection(
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

M_INLINED bool math_point_in_triangle(vec2f point, vec2f v0, vec2f v1, vec2f v2) {
  register float d0 = math_sign(point, v0, v1), d1 = math_sign(point, v1, v2), d2 = math_sign(point, v2, v0);
  return !(((d0 < 0) || (d1 < 0) || (d2 < 0)) && ((d0 > 0) || (d1 > 0) || (d2 > 0)));
}

M_INLINED float math_line_segment_point_perpendicular_distance(vec2f a, vec2f b, vec2f point) {
  return fabs(math_cross(vec2f_sub(b, a), vec2f_sub(a, point))) / math_length(vec2f_sub(b, a));
}

M_INLINED float math_line_segment_point_distance(vec2f a, vec2f b, vec2f point) {
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

M_INLINED bool math_point_on_line_segment(vec2f P, vec2f B, vec2f A) {
  const vec2f BA = vec2f_sub(B, A);
  const vec2f PA = vec2f_sub(P, A);
  const float cross = math_cross(BA, PA);

  if (fabsf(cross) > MATHS_EPSILON) {
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

M_INLINED int _math_orientation(vec2f p, vec2f q, vec2f r) {
  const float val = math_sign(p, q, r);
  if (fabsf(val) < 1e-6f) { return 0; }
  return (val > 0) ? 1 : 2;
}

M_INLINED bool math_line_segments_intersect(vec2f p1, vec2f p2, vec2f q1, vec2f q2) {
    int o1 = _math_orientation(p1, p2, q1);
    int o2 = _math_orientation(p1, p2, q2);
    int o3 = _math_orientation(q1, q2, p1);
    int o4 = _math_orientation(q1, q2, p2);

    // General case
    if (o1 != o2 && o3 != o4) {
      return true;
    }

    // Special cases (colinear)
    if (o1 == 0 && math_point_on_line_segment(q1, p1, p2)) { return true; }
    if (o2 == 0 && math_point_on_line_segment(q2, p1, p2)) { return true; }
    if (o3 == 0 && math_point_on_line_segment(p1, q1, q2)) { return true; }
    if (o4 == 0 && math_point_on_line_segment(p2, q1, q2)) { return true; }

    return false;
}


#endif
