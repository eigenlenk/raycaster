#ifndef RAYCASTER_TEXTURE_INCLUDED
#define RAYCASTER_TEXTURE_INCLUDED

#include "macros.h"

/* You may define your own type or reference */
typedef int32_t texture_ref;

/* Some value for your type to identify a no-texture */
#define TEXTURE_NONE -1

/*
 * Texture sampler accepting world space texture coordinates
 * and expects texture to repeat.
 */
extern void (*texture_sampler_scaled)(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);

/*
 * Texture sampler accepting normalized texture coordinates
 * that should be clamped at the edges.
 */
extern void (*texture_sampler_normalized)(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);

M_INLINED void
debug_texture_sampler_scaled(
  texture_ref texture,
  float fx,
  float fy,
  uint8_t mip_level,
  uint8_t *pixel,
  uint8_t *mask
) {
  M_UNUSED(mip_level);

  if (pixel) {
    pixel[0] = (int32_t)floorf(fx) & 127;
    pixel[1] = (int32_t)floorf(fy) & 127;
    pixel[2] = (int32_t)floorf(fy) & 127;
  }

  /*
   * Only masked textures are supported for now, so any pixel
   * with a non-zero mask value will be drawn.
   */
  if (mask)
    *mask = 255;
}

M_INLINED void
debug_texture_sampler_normalized(
  texture_ref texture,
  float fx,
  float fy,
  uint8_t mip_level,
  uint8_t *pixel,
  uint8_t *mask
) {
  M_UNUSED(mip_level);

  if (pixel) {
    pixel[0] = (int32_t)(fx * 127);
    pixel[1] = (int32_t)(fy * 127);
    pixel[2] = (int32_t)(fy * 127);
  }

  /*
   * Only masked textures are supported for now, so any pixel
   * with a non-zero mask value will be drawn.
   */
  if (mask)
    *mask = 255;
}

/* For passing wall texture list to map builder as part of a polygon */
#define TEXLIST(...) __UNPACK_N(__VA_ARGS__, __UNPACK_3, __UNPACK_2, __UNPACK_1, _0)(__VA_ARGS__)

#define __UNPACK_N(_1, _2, _3, NAME, ...) NAME
#define __UNPACK_3(UPPER, MIDDLE, LOWER) UPPER, MIDDLE, LOWER
#define __UNPACK_2(UPPER_LOWER, MIDDLE) UPPER_LOWER, MIDDLE, UPPER_LOWER
#define __UNPACK_1(MIDDLE) MIDDLE, MIDDLE, MIDDLE

#endif
