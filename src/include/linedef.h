#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"
#include "texture.h"

static const float LINEDEF_SEGMENT_LENGTH_INV = 1.f / 128;

struct polygon;
struct sector;

typedef enum {
  LINE_TEXTURE_TOP = 0,
  LINE_TEXTURE_MIDDLE,
  LINE_TEXTURE_BOTTOM
} linedef_side_texture;

typedef enum {
  /*  */
  LINEDEF_TRANSPARENT_MIDDLE_TEXTURE = M_BIT(0),
  /*  */
  LINEDEF_DOUBLE_SIDED = M_BIT(1),
  /* Keeps top texture in place when changing ceiling height */
  LINEDEF_PIN_TOP_TEXTURE = M_BIT(2),
  /* Keeps bottom texture in place when changing floor height */
  LINEDEF_PIN_BOTTOM_TEXTURE = M_BIT(3),
  /*  */
  LINEDEF_MIRROR = M_BIT(4)
} linedef_flags;

typedef struct linedef_segment {
  vec2f p0, p1;
  light *lights[MAX_LIGHTS_PER_SURFACE];
  uint8_t lights_count;
} linedef_segment;

typedef struct linedef {
  const vertex *v0, *v1;
  struct linedef_side {
    struct sector *sector;
    texture_ref texture[3];
    linedef_segment *segments;
    linedef_flags flags;
    vec2f normal;
  } side[2];
  vec2f direction;
  int32_t max_floor_height,
          min_ceiling_height;
  uint16_t segments;
  float length, xmin, xmax, ymin, ymax;
#ifdef RAYCASTER_PRERENDER_VISCHECK
  uint32_t last_visibility_check_tick;
#endif
} linedef;

void
linedef_update_floor_ceiling_limits(linedef*);

void
linedef_create_segments_for_side(linedef*, int side);

void
linedef_configure_side(linedef*, const struct sector*, const struct polygon*, int);

#endif
