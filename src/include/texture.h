#ifndef RAYCASTER_TEXTURE_INCLUDED
#define RAYCASTER_TEXTURE_INCLUDED

#include "macros.h"

/* You may define your own type or reference */
typedef int32_t texture_ref;

/* Some value for your type to identify a no-texture */
#define TEXTURE_NONE -1

typedef void (*texture_coordinates_func)(float, float, int, int, int32_t*, int32_t*);

extern void (*texture_sampler)(texture_ref, float, float, texture_coordinates_func, uint8_t, uint8_t*, uint8_t*);

M_INLINED void
debug_texture_sampler(
  texture_ref texture,
  float fx,
  float fy,
  texture_coordinates_func coords,
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
texture_coordinates_scaled(float fx, float fy, int w, int h, int32_t *x, int32_t *y)
{
  *x = (int32_t)floorf(fx) & (w-1); // / mip_level) * mip_level;
  *y = (int32_t)floorf(fy) & (h-1); // / mip_level) * mip_level;
}

M_INLINED void
texture_coordinates_normalized(float fx, float fy, int w, int h, int32_t *x, int32_t *y)
{
  *x = (int32_t)(fx * (w-1)); // / mip_level) * mip_level;
  *y = (int32_t)(fy * (h-1)); // / mip_level) * mip_level;
}

/* For passing wall texture list to map builder as part of a polygon */
#define WALLTEX(...) __WALLTEX_N(__VA_ARGS__, __WALLTEX_3, __WALLTEX_2, __WALLTEX_1)(__VA_ARGS__)

#define __WALLTEX_N(_1, _2, _3, NAME, ...) NAME
#define __WALLTEX_3(UPPER, MIDDLE, LOWER) ((texture_ref[]) { UPPER, MIDDLE, LOWER })
#define __WALLTEX_2(UPPER_LOWER, MIDDLE)  ((texture_ref[]) { UPPER_LOWER, MIDDLE, UPPER_LOWER })
#define __WALLTEX_1(MIDDLE)               ((texture_ref[]) { MIDDLE, MIDDLE, MIDDLE })

#endif
