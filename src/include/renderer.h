#ifndef RAYCAST_RENDERER_INCLUDED
#define RAYCAST_RENDERER_INCLUDED

#include "types.h"
#include "texture.h"

struct camera;
struct level_data;

typedef uint32_t pixel_type;
typedef pixel_type* frame_buffer;

#define RENDERER_DRAW_DISTANCE 16384.f

typedef struct {
  volatile frame_buffer buffer;
  volatile float *depth_values;
  vec2i buffer_size;
  uint32_t tick;

  struct {
    struct level_data *level;
    vec2f view_position;
    float unit_size, view_z;
    int32_t half_w, half_h, pitch_offset;
    texture_ref sky_texture;
  } frame_info;
} renderer;

void
renderer_init(renderer *this, vec2i size);

void
renderer_resize(renderer *this, vec2i new_size);

void
renderer_destroy(renderer *this);

void
renderer_draw(renderer *this, struct camera *camera);

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
  extern void (*renderer_step)(const renderer*);
#endif

#endif
