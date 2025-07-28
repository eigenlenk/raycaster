#ifndef RAYCAST_VEC2_T_INCLUDED
#define RAYCAST_VEC2_T_INCLUDED

#include "macros.h"

#include <stdlib.h>
#include <stdbool.h>

#define DECLARE_VEC2_T(T, NAME)                                                   \
typedef struct {                                                                  \
  T x;                                                                            \
  T y;                                                                            \
} NAME;                                                                           \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_make(T x, T y) {                                                           \
	return (NAME){ x, y };                                                          \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_zero(void) {                                                               \
	return (NAME){ 0 };                                                             \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_add(NAME a, NAME b) {                                                      \
	return (NAME) { a.x+b.x, a.y+b.y };                                             \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_sub(NAME a, NAME b) {                                                      \
	return (NAME) { a.x-b.x, a.y-b.y };                                             \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_mul(NAME a, T f) {                                                         \
	return (NAME) { f*a.x, f*a.y };                                                 \
}                                                                                 \
                                                                                  \
M_INLINED NAME                                                                    \
NAME##_div(NAME a, T f) {                                                         \
	return (NAME) { a.x/f, a.y/f };                                                 \
}                                                                                 \
                                                                                  \
M_INLINED bool                                                                    \
NAME##_equals(NAME a, NAME b) {                                                   \
	return (a.x == b.x && a.y == b.y);                                              \
}

#endif
