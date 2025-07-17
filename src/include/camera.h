#ifndef RAYCAST_CAMERA_INCLUDED
#define RAYCAST_CAMERA_INCLUDED

#define MIN_CAMERA_PITCH -1.0f
#define MAX_CAMERA_PITCH 1.0f

#include "entity.h"

typedef struct camera {
  entity entity;
  vec2f plane;
  float fov, pitch;
} camera;

void
camera_init(camera *this, struct level_data *level);

void
camera_move(camera *this, float movement);

void
camera_rotate(camera *this, float rotation);

void
camera_set_fov(camera*, float);

#endif
