#ifndef RAYCAST_SECTOR_INCLUDED
#define RAYCAST_SECTOR_INCLUDED

#include "linedef.h"
#include "maths.h"
#include "macros.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define LINEDEFS(...) M_NARG(__VA_ARGS__), (linedef[]) { __VA_ARGS__ }

typedef struct sector {
  int32_t floor_height,
          ceiling_height;
  size_t linedefs_count;
  float light;
  uint32_t color;
  uint32_t last_visibility_check_tick;
  linedef **linedefs;
  uint8_t lights_count;
  light *lights[MAX_LIGHTS_PER_SURFACE];
} sector;

bool
sector_references_vertex(sector*, vertex*, size_t);

bool
sector_connects_vertices(sector*, vertex*, vertex*);

linedef*
sector_add_linedef(sector*, linedef*);

void
sector_remove_linedef(sector*, linedef*);

M_INLINED bool
sector_point_inside(const sector *this, vec2f point)
{
  register int i, wn = 0;
  register const linedef *line;

  /* Winding number algorithm */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = this->linedefs[i];

    if (line->v0->point.y <= point.y) {
      if (line->v1->point.y > point.y) {
        if (math_sign(line->v0->point, line->v1->point, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (line->v1->point.y <= point.y) {
        if (math_sign(line->v0->point, line->v1->point, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn == 1 || wn == -1;
}

#endif
