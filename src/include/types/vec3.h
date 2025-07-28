#ifndef RAYCAST_VEC3_T_INCLUDED
#define RAYCAST_VEC3_T_INCLUDED

#include "macros.h"

#include <stdlib.h>
#include <stdbool.h>

/* TODO: float vecs should do "nearly equal" instead somehow? */

#define DECLARE_VEC3_T(T, NAME)                                                   \
typedef struct {                                                                  \
  T x;                                                                            \
  T y;                                                                            \
  T z;                                                                            \
} NAME;                                                                           \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_make(T x, T y, T z) {                                                      \
  return (NAME){ x, y, z };                                                       \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_zero(void) {                                                               \
  return (NAME){ 0 };                                                             \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_add(NAME a, NAME b) {                                                      \
  return (NAME) { a.x+b.x, a.y+b.y, a.z+b.z };                                    \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_sub(NAME a, NAME b) {                                                      \
  return (NAME) { a.x-b.x, a.y-b.y, a.z-b.z };                                    \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_mul(NAME a, T f) {                                                         \
  return (NAME) { f*a.x, f*a.y, f*a.z };                                          \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_div(NAME a, T f) {                                                         \
  return (NAME) { a.x/f, a.y/f, a.z/f };                                          \
}                                                                                 \
                                                                                  \
M_INLINED bool                                                                    \
NAME##_equals(NAME a, NAME b) {                                                   \
  return (a.x == b.x && a.y == b.y && a.z == b.z);                                \
}

#endif
