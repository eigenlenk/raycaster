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
  struct {
    int32_t     height;
    texture_ref texture;
  } floor, ceiling;
  size_t      linedefs_count;  
  float       brightness;
  linedef     **linedefs;
#ifdef RAYCASTER_PRERENDER_VISCHECK
  uint32_t    last_visibility_check_tick;
  linedef     **visible_linedefs;
  size_t      visible_linedefs_count;
#endif
} sector;

bool
sector_references_vertex(const sector*, vertex*, size_t);

bool
sector_connects_vertices(const sector*, vertex*, vertex*);

linedef*
sector_add_linedef(sector*, linedef*);

void
sector_remove_linedef(sector*, linedef*);

void
sector_update_floor_ceiling_limits(sector*);

M_INLINED bool
sector_point_inside(const sector *this, vec2f point)
{
  register size_t i;
  const linedef *line;
  int wn = 0;

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
