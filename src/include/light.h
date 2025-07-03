#ifndef RAYCASTER_LIGHT_INCLUDED
#define RAYCASTER_LIGHT_INCLUDED

#include "types.h"

#define MAX_LIGHTS_PER_SURFACE 4

struct sector;
struct level_data;

void
level_data_update_lights(struct level_data*);

typedef struct light {
  vec3f position;
  float radius,
        radius_sq;
  float strength;
  struct sector *in_sector;
  struct level_data *level;
} light;

M_INLINED
void light_set_position(light *this, vec3f position) {
  this->position = position;
  level_data_update_lights(this->level);
}

#endif
