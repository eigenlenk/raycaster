#include "renderer.h"
#include "camera.h"
#include "level_data.h"
#include "maths.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifdef RAYCASTER_PARALLEL_RENDERING
  #include <omp.h>
#endif

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  #include <emmintrin.h>
  #include <xmmintrin.h>
#endif

#define MAX_SECTOR_HISTORY 64
#define MAX_LINE_HITS_PER_COLUMN 48

void (*texture_sampler_scaled)(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);
void (*texture_sampler_normalized)(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
  #define INSERT_RENDER_BREAKPOINT if (renderer_step) { renderer_step(this); }
  void (*renderer_step)(const renderer*) = NULL;
#else
  #define INSERT_RENDER_BREAKPOINT
#endif

typedef struct ray_info {
  vec2f start,
        end,
        direction,
        direction_normalized;
  float theta_inverse;
} ray_info;

typedef struct ray_intersection {
  struct {
    vec2f origin,
          direction_normalized;
  } ray;
  vec2f point;
  float planar_distance,
        point_distance_inverse,
        depth_scale_factor,
        cz_scaled,
        fz_scaled,
        vz_scaled,
        cz_local,
        fz_local,
        determinant;
  linedef *line;
  sector *front_sector, *back_sector;
  uint8_t side;
  uint8_t distance_steps;
#if !defined RAYCASTER_LIGHT_STEPS || (RAYCASTER_LIGHT_STEPS == 0)
  float light_falloff;
#endif
  struct ray_intersection *next;
} ray_intersection;

typedef struct ray_context {
  size_t count;
  const sector *sectors[MAX_SECTOR_HISTORY];
  ray_intersection *head;
  ray_intersection *full_wall;
} ray_context;

/* Column-specific data */
typedef struct {
  struct ray_intersections {
    ray_intersection list[MAX_LINE_HITS_PER_COLUMN];
    size_t count;
  } intersections;
  float top_limit, bottom_limit;
  uint32_t index, buffer_stride;
  pixel_type *buffer_start;
  bool finished;
} column_info;

#define DIMMING_DISTANCE 4096.f

#if RAYCASTER_LIGHT_STEPS > 0
// static const float LIGHT_STEP_DISTANCE = DIMMING_DISTANCE / RAYCASTER_LIGHT_STEPS;
static const float LIGHT_STEP_DISTANCE_INVERSE = 1.f / (DIMMING_DISTANCE / RAYCASTER_LIGHT_STEPS);
static const float LIGHT_STEP_VALUE_CHANGE = 1.f / RAYCASTER_LIGHT_STEPS;
static const float LIGHT_STEP_VALUE_CHANGE_INVERSE = 1.f / (1.f / RAYCASTER_LIGHT_STEPS);
#else
static const float LIGHT_STEP_DISTANCE_INVERSE = 1.f / (DIMMING_DISTANCE / 4);
static const float DIMMING_DISTANCE_INVERSE = 1.f / DIMMING_DISTANCE;
#endif

#ifdef RAYCASTER_PRERENDER_VISCHECK
  static void
  refresh_sector_visibility(const renderer*, sector*);
#endif

static int
find_sector_intersections(const renderer*, const sector*, const ray_info*, ray_context*, column_info*);

static void
draw_wall_segment(const renderer*, const ray_intersection*, column_info*, uint32_t from, uint32_t to, float, texture_ref);

static void
draw_floor_segment(const renderer*, const ray_intersection*, column_info*, uint32_t from, uint32_t to);

static void
draw_ceiling_segment(const renderer*, const ray_intersection*, column_info*, uint32_t from, uint32_t to);

static void
draw_column_intersection(const renderer*, const ray_intersection*, column_info*);

static void
draw_full_wall(const renderer*, const ray_intersection*, column_info*);

static void
draw_segmented_wall(const renderer*, const ray_intersection*, column_info*);

static void
draw_sky_segment(const renderer *this, const ray_intersection*, const column_info*, uint32_t, uint32_t);

M_INLINED void
init_depth_values(renderer *this)
{
  register size_t y, h = this->buffer_size.y;
  this->depth_values = malloc(h*sizeof(float));
  for (y = 0; y < h; ++y) {
    this->depth_values[y] = 1.f / (y+1);
  }
}

M_INLINED void
insert_sorted(ray_intersection *value, ray_intersection **head)
{
  if (!*head || value->planar_distance < (*head)->planar_distance) {
    value->next = *head;
    *head = value;
    return;
  }
  ray_intersection *cur = *head;
  while (cur->next && cur->next->planar_distance <= value->planar_distance) {
    cur = cur->next;
  }
  value->next = cur->next;
  cur->next = value;
}

void
renderer_init(
  renderer *this,
  vec2i size
) {
  this->buffer_size = size;
  this->buffer = malloc(size.x * size.y * sizeof(pixel_type));
  init_depth_values(this);
}

void
renderer_resize(
  renderer *this,
  vec2i new_size
) {
  this->buffer_size = new_size;
  this->buffer = realloc(this->buffer, new_size.x * new_size.y * sizeof(pixel_type));
  free((float*)this->depth_values);
  init_depth_values(this);
}

void
renderer_destroy(renderer *this)
{
  if (this->buffer) {
    free(this->buffer);
    this->buffer = NULL;
  }
}

void
renderer_draw(
  renderer *this,
  camera *camera
) {
  int32_t x;

  assert(this->buffer);
  memset(this->buffer, 0, this->buffer_size.x * this->buffer_size.y * sizeof(pixel_type));
  
  const int32_t half_h = this->buffer_size.y >> 1;
  sector *root_sector = camera->entity.sector;
  const vec2f view_position = camera->entity.position;
  const vec2f view_direction = camera->entity.direction;
  const vec2f view_plane = camera->plane;

  this->frame_info.level = camera->entity.level;
  this->frame_info.view_position = view_position;
  this->frame_info.far_left = vec2f_add(view_position, vec2f_mul(vec2f_sub(view_direction, camera->plane), RENDERER_DRAW_DISTANCE));
  this->frame_info.far_right = vec2f_add(view_position, vec2f_mul(vec2f_add(view_direction, camera->plane), RENDERER_DRAW_DISTANCE));
  this->frame_info.half_w = this->buffer_size.x >> 1;
  this->frame_info.pitch_offset = (int32_t)floorf(camera->pitch * half_h);
  this->frame_info.half_h = half_h + this->frame_info.pitch_offset;
  this->frame_info.unit_size = (this->buffer_size.x >> 1) / camera->fov;
  this->frame_info.view_z = camera->entity.z;
  this->frame_info.sky_texture = this->frame_info.level->sky_texture;
  this->tick++;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  refresh_sector_visibility(this, root_sector);
#endif

#ifdef RAYCASTER_PARALLEL_RENDERING
  #pragma omp parallel for
#endif
  for (x = 0; x < this->buffer_size.x; ++x) {
    const float cam_x = ((x << 1) / (float)this->buffer_size.x) - 1;
    const vec2f ray_dir_norm = VEC2F(
      view_direction.x + (view_plane.x * cam_x),
      view_direction.y + (view_plane.y * cam_x)
    );
    const vec2f ray_end = VEC2F(
      view_position.x + (ray_dir_norm.x * RENDERER_DRAW_DISTANCE),
      view_position.y + (ray_dir_norm.y * RENDERER_DRAW_DISTANCE)
    );

    column_info column = (column_info) {
      .index = x,
      .intersections = { .count = 0 },
      .buffer_stride = this->buffer_size.x,
      .top_limit = 0.f,
      .bottom_limit = this->buffer_size.y,
      .buffer_start = &this->buffer[x],
      .finished = false
    };

    ray_context context = { 0 };

    ray_info ray = (ray_info) {
      .start = view_position,
      .end = ray_end,
      .direction = vec2f_sub(ray_end, view_position),
      .direction_normalized = ray_dir_norm,
      .theta_inverse = 1.f / math_dot2(view_direction, ray_dir_norm)
    };

    find_sector_intersections(this, root_sector, &ray, &context, &column);
    
    if (context.full_wall) {
      insert_sorted(context.full_wall, &context.head);
      context.full_wall->next = NULL;
    }
    
    draw_column_intersection(this, context.head, &column);
  }

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
  renderer_step = NULL;
#endif
}

/* ----- */

#ifdef RAYCASTER_PRERENDER_VISCHECK

static void
refresh_sector_visibility(
  const renderer *this,
  sector *sect
) {
  register size_t i;
  linedef *line;
  sector *back_sector;

  sect->last_visibility_check_tick = this->tick;

  if (!sect->visible_linedefs) {
    sect->visible_linedefs = malloc(sect->linedefs_count * sizeof(linedef*));
  }
  sect->visible_linedefs_count = 0;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];

    if (line->v0->last_visibility_check_tick != this->tick) {
      line->v0->last_visibility_check_tick = this->tick;
      line->v0->visible = math_point_in_triangle(line->v0->point, this->frame_info.view_position, this->frame_info.far_left, this->frame_info.far_right);
    }

    if (line->v1->last_visibility_check_tick != this->tick) {
      line->v1->last_visibility_check_tick = this->tick;
      line->v1->visible = math_point_in_triangle(line->v1->point, this->frame_info.view_position, this->frame_info.far_left, this->frame_info.far_right);
    }

    if (line->v0->visible || line->v1->visible
      || math_find_line_intersection(line->v0->point, line->v1->point, this->frame_info.view_position, this->frame_info.far_left, NULL, NULL)
      || math_find_line_intersection(line->v0->point, line->v1->point, this->frame_info.view_position, this->frame_info.far_right, NULL, NULL)) {
      sect->visible_linedefs[sect->visible_linedefs_count++] = line;
      back_sector = line->side[0].sector == sect ? line->side[1].sector : line->side[0].sector;

      if (back_sector && back_sector->last_visibility_check_tick != this->tick) {
        refresh_sector_visibility(this, back_sector);
      }
    }
  }
}

#endif

static int
find_sector_intersections(
  const renderer *this,
  const sector *sect,
  const ray_info *ray,
  ray_context *context,
  column_info *column
) {
  register size_t i;
  float planar_distance, point_distance,
    line_det, ray_det,
    depth_scale_factor, cz_scaled, fz_scaled, vz_scaled,
    sign;
  vec2f point;
  int side, result_count = 0;
  linedef *line;
  
  if (context->count == MAX_SECTOR_HISTORY) {
    return result_count;
  }

  for (i = 0; i < context->count; ++i) {
    if (context->sectors[i] == sect) {
      return result_count;
    }
  }

  context->sectors[context->count++] = sect;

  size_t insert_index;
  sector *back_sector;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  for (i = 0; i < sect->visible_linedefs_count && column->intersections.count < MAX_LINE_HITS_PER_COLUMN; ++i) {
    line = sect->visible_linedefs[i];
#else
  for (i = 0; i < sect->linedefs_count && column->intersections.count < MAX_LINE_HITS_PER_COLUMN; ++i) {
    line = sect->linedefs[i];
#endif

    side = line->side[0].sector == sect ? 0 : 1;
    sign = math_sign(line->v0->point, line->v1->point, ray->start);

    if ((side == 0 && sign > 0) || (side == 1 && sign < 0)) {
      continue;
    }

    if (math_find_line_intersection_cached(line->v0->point, ray->start, line->direction, ray->direction, &point, &line_det, &ray_det)) {
      planar_distance = ray_det * RENDERER_DRAW_DISTANCE;
      point_distance = planar_distance * ray->theta_inverse;

      depth_scale_factor = this->frame_info.unit_size / planar_distance;
      cz_scaled = sect->ceiling.height * depth_scale_factor;
      fz_scaled = sect->floor.height * depth_scale_factor;
      vz_scaled = this->frame_info.view_z * depth_scale_factor;

      result_count ++;
      insert_index = column->intersections.count++;

      column->intersections.list[insert_index] = (ray_intersection) {
        .ray = {
          .origin = ray->start,
          .direction_normalized = ray->direction_normalized
        },
        .point = point,
        .planar_distance = planar_distance,
        .point_distance_inverse = 1.f / point_distance,
        .depth_scale_factor = depth_scale_factor,
        .cz_scaled = cz_scaled,
        .fz_scaled = fz_scaled,
        .vz_scaled = vz_scaled,
        .cz_local = this->frame_info.half_h - cz_scaled + vz_scaled,
        .fz_local = this->frame_info.half_h - fz_scaled + vz_scaled,
        .determinant = line_det,
        .line = line,
        .front_sector = (sector*)sect,
        .back_sector = line->side[!side].sector,
        .side = side,
        .distance_steps = (uint8_t)(point_distance * LIGHT_STEP_DISTANCE_INVERSE),
#if !defined RAYCASTER_LIGHT_STEPS || (RAYCASTER_LIGHT_STEPS == 0)
        .light_falloff = point_distance * DIMMING_DISTANCE_INVERSE,
#endif
        .next = NULL
      };

      if ((back_sector = line->side[!side].sector)) {
        if (!context->full_wall || planar_distance < context->full_wall->planar_distance) {
          insert_sorted(&column->intersections.list[insert_index], &context->head);
          result_count += find_sector_intersections(this, back_sector, ray, context, column);
        }
      } else if (!context->full_wall || planar_distance < context->full_wall->planar_distance) {
        context->full_wall = &column->intersections.list[insert_index];
      }
    }
  }
  
  return result_count;
}

static void
draw_column_intersection(
  const renderer *this,
  const ray_intersection *intersection,
  column_info *column
) {
  if (!intersection) {
    return;
  }

  if (intersection->next) {
    draw_segmented_wall(this, intersection, column);
  } else {
    draw_full_wall(this, intersection, column);
  }
}

static void
draw_full_wall(const renderer *this, const ray_intersection *intersection, column_info *column)
{
  const struct linedef_side *fside = &intersection->line->side[intersection->side];
  const float sy = ceilf(M_MAX(intersection->cz_local, column->top_limit));
  const float ey = M_CLAMP(intersection->fz_local, column->top_limit, column->bottom_limit);

  draw_wall_segment(this, intersection, column, sy, ey, sy - this->frame_info.half_h - intersection->vz_scaled, fside->texture[LINE_TEXTURE_MIDDLE]);
  
  if (intersection->front_sector->ceiling.texture != TEXTURE_NONE) {
    draw_ceiling_segment(this, intersection, column, column->top_limit, M_MIN(sy, column->bottom_limit));
  } else {
    draw_sky_segment(this, intersection, column, column->top_limit, M_MIN(sy, column->bottom_limit));
  }
  
  draw_floor_segment(this, intersection, column, ey, column->bottom_limit);
  
  column->finished = true;
}

static void
draw_segmented_wall(const renderer *this, const ray_intersection *intersection, column_info *column)
{
  const struct linedef_side *fside = &intersection->line->side[intersection->side];

  /* Draw top and bottom segments of the wall and the sector behind */
  const float top_h = (intersection->front_sector->ceiling.height - intersection->back_sector->ceiling.height) * intersection->depth_scale_factor;
  const float bottom_h = (intersection->back_sector->floor.height - intersection->front_sector->floor.height) * intersection->depth_scale_factor;

  /* Top start _ end | bottom start _ end*/
  const float ts_y = ceilf(math_clamp(intersection->cz_local, column->top_limit, column->bottom_limit));
  const float te_y = ceilf(math_clamp(intersection->cz_local + top_h, column->top_limit, column->bottom_limit));
  const float be_y = math_clamp(intersection->fz_local, column->top_limit, column->bottom_limit);
  const float bs_y = math_clamp(intersection->fz_local - bottom_h, column->top_limit, column->bottom_limit);

  const bool back_sector_has_sky = intersection->back_sector->ceiling.texture == TEXTURE_NONE;

  float n_top = column->top_limit;
  float n_bottom = column->bottom_limit;

  if (!back_sector_has_sky) {
    if (top_h > 0) {
      const float tex_sy = fside->flags & LINEDEF_PIN_BOTTOM_TEXTURE
        ? ts_y - top_h - this->frame_info.half_h - intersection->vz_scaled
        : ts_y - this->frame_info.half_h - intersection->vz_scaled;
      draw_wall_segment(this, intersection, column, ts_y, te_y, tex_sy, fside->texture[LINE_TEXTURE_TOP]);
      n_top = te_y;
    } else {
      n_top = ts_y;
    }
  }

  if (bottom_h > 0) {
    const float tex_sy = fside->flags & LINEDEF_PIN_BOTTOM_TEXTURE
      ? bs_y + bottom_h - this->frame_info.half_h - intersection->vz_scaled
      : bs_y - this->frame_info.half_h - intersection->vz_scaled;
    draw_wall_segment(this, intersection, column, bs_y, be_y, tex_sy, fside->texture[LINE_TEXTURE_BOTTOM]);
    n_bottom = bs_y;
  } else {
    n_bottom = be_y;
  }

  if (intersection->front_sector->ceiling.texture != TEXTURE_NONE) {
    draw_ceiling_segment(this, intersection, column, column->top_limit, ts_y);
    if (back_sector_has_sky) {
      n_top = ts_y;
    }
  } else {
    draw_sky_segment(this, intersection, column, column->top_limit, M_MAX(ts_y, column->top_limit));
  }
    
  draw_floor_segment(this, intersection, column, be_y, column->bottom_limit);

  column->top_limit = n_top;
  column->bottom_limit = n_bottom;

  if ((int)column->top_limit == (int)column->bottom_limit || intersection->back_sector->floor.height == intersection->back_sector->ceiling.height) {
    column->finished = true;
    return;
  }

  /* Render next ray intersection */
  draw_column_intersection(this, intersection->next, column);

  /* Draw transparent middle texture from back to front, with overdraw for now. */
  if (fside->texture[LINE_TEXTURE_MIDDLE] != TEXTURE_NONE) {
    draw_wall_segment(this, intersection, column, n_top, n_bottom, n_top - this->frame_info.half_h - intersection->vz_scaled, fside->texture[LINE_TEXTURE_MIDDLE]);
  }
}

/*
 * There are three light functions here:
 * 
 * When a surface is affected by a dynamic light:
 *   1. For horizontal surfaces (floors, ceilings) with a little falloff/attenuation
 *      as the light approaches and goes below it
 *   2. For vertical surfaces (walls)
 * 
 * When it's not:
 *   3. Basic brightness and dimming
 */

#define VERTICAL_FADE_DIST 2.5f

M_INLINED float
calculate_horizontal_surface_light(const sector *sect, vec3f pos, bool is_floor, size_t num_lights, light **lights,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  size_t i;
  vec3f world_pos;
  light *lt;
  float dz, v = sect->brightness, dsq;

  for (i = 0; i < num_lights; ++i) {
    lt = lights[i];

    /* Too far off the floor or ceiling */
    if ((dz = is_floor ? (lt->entity.z - sect->floor.height) : (sect->ceiling.height - lt->entity.z)) && (dz < 0.f)) {
      continue;
    }

    world_pos = entity_world_position(&lt->entity);

    if ((dsq = math_vec3_distance_squared(pos, world_pos)) > lt->radius_sq) {
      continue;
    }

#ifdef RAYCASTER_DYNAMIC_SHADOWS
    v = !map_cache_intersect_3d(&lt->entity.level->cache, pos, world_pos)
      ? math_max(v, lt->strength * math_min(1.f, dz / VERTICAL_FADE_DIST) * (1.f - (dsq * lt->radius_sq_inverse)))
      : v;
#else
    v = math_max(v, lt->strength * math_min(1.f, dz / VERTICAL_FADE_DIST) * (1.f - (dsq * lt->radius_sq_inverse)));
#endif
  }

  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(v * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    v - light_falloff
#endif
  );
}


M_INLINED float
calculate_vertical_surface_light(const sector *sect, vec3f pos, size_t num_lights, light **lights,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  size_t i;
  light *lt;
  vec3f world_pos;
  float v = sect->brightness, dsq;

  for (i = 0; i < num_lights; ++i) {
    lt = lights[i];
    world_pos = entity_world_position(&lt->entity);

    if ((dsq = math_vec3_distance_squared(pos, world_pos)) > lt->radius_sq) {
      continue;
    }

#ifdef RAYCASTER_DYNAMIC_SHADOWS
    v = !map_cache_intersect_3d(&lt->entity.level->cache, pos, world_pos)
      ? math_max(v, lt->strength * (1.f - (dsq * lt->radius_sq_inverse)))
      : v;
#else
    v = math_max(v, lt->strength * (1.f - (dsq * lt->radius_sq_inverse)));
#endif
  }

  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(v * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    v - light_falloff
#endif
  );
}

M_INLINED float
calculate_basic_brightness(const float base,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(base * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    base - light_falloff
#endif
  );
}

static void
draw_wall_segment(
  const renderer *this,
  const ray_intersection *intersection,
  column_info *column,
  uint32_t from,
  uint32_t to,
  float texture_start_y,
  texture_ref texture
) {
  if (from >= to || texture == TEXTURE_NONE) {
    return;
  }

  register uint32_t y;
  const float texture_step  = intersection->planar_distance / this->frame_info.unit_size;
  const float texture_x     = intersection->determinant * intersection->line->length;
  const uint16_t segment    = (uint16_t)floorf((intersection->line->segments - 1) * intersection->determinant);
  const struct linedef_side *side = &intersection->line->side[intersection->side];
  uint32_t *p               = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3];
  uint8_t mask;
  uint8_t lights_count      = side->segments[segment].lights_count;
  struct light **lights     = side->segments[segment].lights;
  register float light      = !lights_count ? calculate_basic_brightness(
      intersection->front_sector->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      intersection->distance_steps
#else
      intersection->light_falloff
#endif
  ) : 0.f, texture_y        = (texture_start_y * texture_step);

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from; y < to; ++y, p += column->buffer_stride, texture_y += texture_step) {
    texture_sampler_scaled(texture, texture_x, texture_y, 1 + intersection->distance_steps, &rgb[0], &mask);
 
    if (!mask) { continue; } /* Transparent - skip */

    light = lights_count ?
      calculate_vertical_surface_light(
        intersection->front_sector,
        VEC3F(intersection->point.x, intersection->point.y, -texture_y),
        lights_count,
        lights,
#if RAYCASTER_LIGHT_STEPS > 0
        intersection->distance_steps
#else
        intersection->light_falloff
#endif
      ) : light;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  }
}

static void
draw_floor_segment(
  const renderer *this,
  const ray_intersection *intersection,
  column_info *column,
  uint32_t from,
  uint32_t to
) {
  if (from >= to ||
      this->frame_info.view_z < intersection->front_sector->floor.height ||
      intersection->front_sector->floor.texture == TEXTURE_NONE) {
    return;
  }

  register uint32_t y, yz;
  register float light=-1, distance, weight, wx, wy;
  const float distance_from_view = (this->frame_info.view_z - intersection->front_sector->floor.height) * this->frame_info.unit_size;
  uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3], lights_count;
  map_cache_cell *cell;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from, yz = from - this->frame_info.half_h; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz++]);
    weight = math_min(1.f, distance * intersection->point_distance_inverse);
    wx = (weight * intersection->point.x) + ((1-weight) * intersection->ray.origin.x);
    wy = (weight * intersection->point.y) + ((1-weight) * intersection->ray.origin.y);
    cell = map_cache_cell_at(&this->frame_info.level->cache, VEC2F(wx, wy));
    lights_count = cell ? cell->lights_count : 0;

    texture_sampler_scaled(intersection->front_sector->floor.texture, wx, wy, 1 + (uint8_t)(distance * LIGHT_STEP_DISTANCE_INVERSE), &rgb[0], NULL);

    light = lights_count ? calculate_horizontal_surface_light(
      intersection->front_sector,
      VEC3F(wx, wy, intersection->front_sector->floor.height),
      true,
      lights_count,
      cell ? cell->lights : NULL,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    ) : calculate_basic_brightness(
      intersection->front_sector->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    );

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  } 
}

static void
draw_ceiling_segment(
  const renderer *this,
  const ray_intersection *intersection,
  column_info *column,
  uint32_t from,
  uint32_t to
) {
  /* Camera above the ceiling */
  if (from >= to || this->frame_info.view_z > intersection->front_sector->ceiling.height) {
    return;
  }

  register uint32_t y, yz;
  register float light=-1, distance, weight, wx, wy;
  const float distance_from_view = (intersection->front_sector->ceiling.height - this->frame_info.view_z) * this->frame_info.unit_size;
  uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3], lights_count;
  map_cache_cell *cell;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from, yz = this->frame_info.half_h - from - 1; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz--]);
    weight = math_min(1.f, distance * intersection->point_distance_inverse);
    wx = (weight * intersection->point.x) + ((1-weight) * intersection->ray.origin.x);
    wy = (weight * intersection->point.y) + ((1-weight) * intersection->ray.origin.y);
    cell = map_cache_cell_at(&this->frame_info.level->cache, VEC2F(wx, wy));
    lights_count = cell ? cell->lights_count : 0;

    texture_sampler_scaled(intersection->front_sector->ceiling.texture, wx, wy, 1 + (uint8_t)(distance * LIGHT_STEP_DISTANCE_INVERSE), &rgb[0], NULL);

    light = lights_count ? calculate_horizontal_surface_light(
      intersection->front_sector,
      VEC3F(wx, wy, intersection->front_sector->ceiling.height),
      false,
      lights_count,
      cell ? cell->lights : NULL,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    ) : calculate_basic_brightness(
      intersection->front_sector->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    );

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  }
}

static void
draw_sky_segment(const renderer *this, const ray_intersection *intersection, const column_info *column, uint32_t from, uint32_t to)
{
  if (from == to || this->frame_info.sky_texture == TEXTURE_NONE) {
    return;
  }

  register uint16_t y;
  uint8_t rgb[3];
  float angle = atan2f(intersection->ray.direction_normalized.x, intersection->ray.direction_normalized.y) * (180.0f / M_PI);
  if (angle < 0.0f) {
    angle += 360.0f;
  }
  float sky_x = angle / 360, h = (float)this->buffer_size.y; 
  uint32_t *p = column->buffer_start + (from * column->buffer_stride);

  for (y = from; y < to; ++y, p += column->buffer_stride) {
    texture_sampler_normalized(this->frame_info.sky_texture, sky_x, math_min(1.f, 0.5f+(y-this->frame_info.pitch_offset)/h), 1, &rgb[0], NULL);
    *p = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    INSERT_RENDER_BREAKPOINT
  }
}
