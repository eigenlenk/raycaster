#ifndef RAYCAST_VERTEX_INCLUDED
#define RAYCAST_VERTEX_INCLUDED

#include "types.h"

typedef struct {
  vec2f point;
  uint32_t last_visibility_check_tick;
#ifdef RAYCASTER_PRERENDER_VISCHECK
  bool visible;
#endif
} vertex;

#endif
