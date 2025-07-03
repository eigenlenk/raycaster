#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"

struct sector;

typedef struct {
  vertex *v0, *v1;
  struct sector *side_sector[2];
  uint32_t color;
#ifdef LINE_VIS_CHECK
  uint32_t last_visible_tick;
#endif
  uint8_t lights_count[2];
  light *lights[2][MAX_LIGHTS_PER_SURFACE];
} linedef;

#endif
