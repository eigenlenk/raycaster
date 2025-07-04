#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"

struct sector;

typedef struct {
  vertex *v0, *v1;
  struct sector *side_sector[2];
  uint32_t color;
#ifdef LINE_VIS_CHECK
  uint32_t last_visible_tick;
#endif
} linedef;

#endif
