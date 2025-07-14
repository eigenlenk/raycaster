#include "linedef.h"
#include "sector.h"

void
linedef_update_floor_ceiling_limits(linedef *this)
{
  this->max_floor_height = M_MAX(
    this->side[0].sector->floor.height,
    this->side[1].sector ? this->side[1].sector->floor.height : 0
  );
  this->min_ceiling_height = M_MIN(
    this->side[0].sector->ceiling.height,
    this->side[1].sector ? this->side[1].sector->ceiling.height : 0
  );
}

void
linedef_create_segments_for_side(linedef *this, int side)
{
  size_t i;
  vec2f dir = vec2f_sub(this->v1->point, this->v0->point);
  float d, seg_len = 1.f / this->segments;
  // printf("Line (%d, %d) <-> (%d, %d) length %f has %d segments\n", XY(this->v0->point), XY(this->v1->point), this->length, this->segments);
  
  this->side[side].segments = malloc(this->segments * sizeof(linedef_segment));
  
  for (i = 0, d = 0.f; i < this->segments; ++i, d += seg_len) {
    this->side[side].segments[i].lights_count = 0;
    this->side[side].segments[i].p0 = vec2f_add(this->v0->point, vec2f_mul(dir, d));
    this->side[side].segments[i].p1 = vec2f_add(this->v0->point, vec2f_mul(dir, math_min(1.f, d + seg_len)));
    // printf("\tSegment %d: (%d, %d) <-> (%d, %d)\n", i, XY(this->side[side].segments[i].p0), XY(this->side[side].segments[i].p1));
  }
}
