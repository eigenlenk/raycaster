#ifndef RAYCASTER_LIGHT_INCLUDED
#define RAYCASTER_LIGHT_INCLUDED

#include "types.h"

#define MAX_LIGHTS_PER_SURFACE 4

struct sector;
struct level_data;

typedef struct light {
  vec3f position;
  float radius,
        radius_sq;
  float strength;
  struct sector *in_sector;
  const struct level_data *level;
} light;

#endif
