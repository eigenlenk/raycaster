#include "renderer.h"
#include "maths.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifdef PARALLEL_RENDERING
  #include <omp.h>
#endif

#ifdef VECTORIZED_LIGHT_MUL
  #include <xmmintrin.h>
#endif

#define MAX_SECTOR_HISTORY 64

static uint8_t debug_colors[16][3] = {
  { 195, 235, 233 },
  { 123, 45, 67 },
  { 34, 200, 123 },
  { 87, 156, 231 },
  { 12, 89, 190 },
  { 250, 210, 100 },
  { 10, 240, 180 },
  { 76, 89, 240 },
  { 55, 200, 60 },
  { 190, 33, 90 },
  { 44, 66, 77 },
  { 100, 255, 200 },
  { 199, 123, 44 },
  { 155, 155, 155 },
  { 0, 123, 255 },
  { 231, 99, 178 }
};

static uint8_t debug_colors_dark[16][3] = {
  { 120, 110, 100 },
  { 123, 67, 45 },
  { 34, 100, 123 },
  { 87, 126, 111 },
  { 12, 89, 90 },
  { 128, 110, 100 },
  { 70, 120, 80 },
  { 76, 89, 120 },
  { 55, 100, 60 },
  { 90, 33, 90 },
  { 44, 66, 77 },
  { 100, 128, 100 },
  { 99, 123, 44 },
  { 88, 88, 88 },
  { 10, 123, 100 },
  { 111, 99, 78 }
};

/* Common frame info all column renderers can share */
typedef struct {
  vec2f view_position,
        near_left,
        near_right,
        far_left,
        far_right;
  float unit_size, view_z;
  int32_t half_w, half_h;
} frame_info;

/* Column-specific data */
typedef struct {
  const sector *sector_history[MAX_SECTOR_HISTORY];
  vec2f ray_start,
        ray_end,
        ray_direction;
  float theta, top_limit, bottom_limit;
  uint32_t index, sector_depth, buffer_stride;
  pixel_type *buffer_start;
  bool finished;
} column_info;

typedef struct {
  vec2f point;
  float planar_distance,
        planar_distance_inv,
        point_distance;
  linedef *line;
  sector *back_sector;
#if LIGHT_STEPS > 0
  uint8_t light_steps;
#else
  float light_falloff;
#endif
} line_hit;

typedef enum {
  SURFACE_WALL,
  SURFACE_FLOOR,
  SURFACE_CEILING
} surface_type;

#define DIMMING_DISTANCE 4096.f
#define LIGHT_RESOLUTION 1

#if LIGHT_STEPS > 0
static const float LIGHT_STEP_DISTANCE = DIMMING_DISTANCE / LIGHT_STEPS;
static const float LIGHT_STEP_VALUE_CHANGE = 1.f / LIGHT_STEPS;
#endif

#ifdef LINE_VIS_CHECK
static void
check_sector_visibility(renderer*, const frame_info*, sector*);
#endif

static float
calculate_light(const sector *sect, vec3f pos, surface_type, size_t, light**,
#if LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
);

static void check_sector_column(renderer*, const frame_info*, column_info*, const sector*);
static void draw_wall_segment(const frame_info*, column_info*, const sector*, const line_hit*, int32_t from, int32_t to, float, float);
static void draw_floor_segment(renderer*, const frame_info*, column_info*, const sector*, const line_hit*, float, uint32_t from, uint32_t to);
static void draw_ceiling_segment(renderer*, const frame_info*, column_info*, const sector*, const line_hit*, float, uint32_t from, uint32_t to);
static void draw_column(renderer*, const frame_info*, column_info*, const sector*, line_hit const*);

M_INLINED void init_depth_values(renderer *this) {
  register size_t y, h = 1+(this->buffer_size.y>>1);
  this->depth_values = malloc(h*sizeof(float));
  for (y = 0; y < h; ++y) {
    this->depth_values[y] = !y ? 1.f : 1.f / y;
  }
}

void renderer_init(
  renderer *this,
  vec2u size
) {
  this->buffer_size = size;
  this->buffer = malloc(size.x * size.y * sizeof(pixel_type));
  init_depth_values(this);
}

void renderer_resize(
  renderer *this,
  vec2u new_size
) {
  this->buffer_size = new_size;
  this->buffer = realloc(this->buffer, new_size.x * new_size.y * sizeof(pixel_type));
  free((float*)this->depth_values);
  init_depth_values(this);
}

void renderer_destroy(renderer *this) {
  if (this->buffer) {
    free(this->buffer);
    this->buffer = NULL;
  }
}

void renderer_draw(
  renderer *this,
  camera *camera
) {
  uint32_t x;
  frame_info info;

  assert(this->buffer);
  memset(this->buffer, 0, this->buffer_size.x * this->buffer_size.y * sizeof(pixel_type));
  
  this->tick++;

  info.view_position = camera->position;
  info.near_left = vec2f_sub(camera->position, camera->plane),
  info.near_right = vec2f_add(camera->position, camera->plane);
  info.far_left = vec2f_add(camera->position, vec2f_mul(vec2f_sub(camera->direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.far_right = vec2f_add(camera->position, vec2f_mul(vec2f_add(camera->direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.half_w = this->buffer_size.x >> 1;
  info.half_h = this->buffer_size.y >> 1;
  info.unit_size = (this->buffer_size.x >> 1) / camera->fov;
  info.view_z = camera->z;

#ifdef LINE_VIS_CHECK
  check_sector_visibility(this, &info, camera->in_sector);
#endif

#ifdef PARALLEL_RENDERING
  #pragma omp parallel for simd
#endif
  for (x = 0; x < this->buffer_size.x; ++x) {
    const float cam_x = ((x << 1) / (float)this->buffer_size.x) - 1;
    const vec2f ray = VEC2F(
      camera->direction.x + (camera->plane.x * cam_x),
      camera->direction.y + (camera->plane.y * cam_x)
    );

    column_info column = (column_info) {
      .ray_start = camera->position,
      .ray_end = VEC2F(
        camera->position.x + (ray.x * RENDERER_DRAW_DISTANCE),
        camera->position.y + (ray.y * RENDERER_DRAW_DISTANCE)
      ),
      .ray_direction = ray,
      .index = x,
      .sector_depth = 0,
      .buffer_stride = this->buffer_size.x,
      .theta = math_dot2(camera->direction, ray) / math_length(ray),
      .top_limit = 0.f,
      .bottom_limit = this->buffer_size.y - 1,
      .buffer_start = &this->buffer[x],
      .finished = false
    };

    check_sector_column(this, &info, &column, camera->in_sector);
  }
}

/* ----- */

#ifdef LINE_VIS_CHECK

static void
check_sector_visibility(
  renderer *this,
  const frame_info *info,
  sector *sect
) {
  register size_t i;
  linedef *line;
  sector *back_sector;

  sect->last_visibility_check_tick = this->tick;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];

    if (line->last_visible_tick == this->tick) {
      continue;
    }

    if (line->v0->last_visibility_check_tick != this->tick) {
      line->v0->last_visibility_check_tick = this->tick;
      line->v0->visible = math_point_in_triangle(line->v0->point, info->view_position, info->far_left, info->far_right);
    }

    if (line->v1->last_visibility_check_tick != this->tick) {
      line->v1->last_visibility_check_tick = this->tick;
      line->v1->visible = math_point_in_triangle(line->v1->point, info->view_position, info->far_left, info->far_right);
    }

    if (line->v0->visible || line->v1->visible
      || math_line_segments_intersect(line->v0->point, line->v1->point, info->view_position, info->far_left)
      || math_line_segments_intersect(line->v0->point, line->v1->point, info->view_position, info->far_right)) {
      line->last_visible_tick = this->tick;

      back_sector = line->side_sector[0] == sect ? line->side_sector[1] : line->side_sector[0];

      if (back_sector && back_sector->last_visibility_check_tick != this->tick) {
        check_sector_visibility(this, info, back_sector);
      }
    }
  }
}

#endif

static float
calculate_light(const sector *sect, vec3f pos, surface_type surface_type, size_t num_lights, light **lights,
#if LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  size_t i;
  light *lt;
  float v = sect->light, dsq;
  for (i = 0; i < num_lights; ++i) {
    lt = lights[i];
    if (surface_type != SURFACE_WALL &&
        ((surface_type == SURFACE_FLOOR && lt->position.z < sect->floor_height)
          || (surface_type == SURFACE_CEILING && lt->position.z > sect->ceiling_height))) {
      continue;
    }

    dsq = math_vec3_distance_squared(pos, lt->position);

    if (dsq > lt->radius_sq) {
      continue;
    }

#ifdef DYNAMIC_SHADOWS
    if (level_data_intersect_3d(lt->level, pos, lt->position, sect)) {
      continue;
    }
#endif

    v = math_max(v, lt->strength * math_min(1.f, 1.f - (dsq / lt->radius_sq)));
  }

#if LIGHT_STEPS > 0
  return ((uint8_t)(v / LIGHT_STEP_VALUE_CHANGE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE);
#else
  return v - light_falloff;
#endif
}

M_INLINED void sort_nearest(line_hit *arr, int n) {
  register int i, j;
  line_hit hit;
  for (i = 1; i < n; ++i) {
    hit = arr[i];
    j = i-1;
    while (j >= 0 && arr[j].planar_distance > hit.planar_distance) {
      arr[j+1] = arr[j];
      j = j-1;
    }
    arr[j+1] = hit;
  }
}

static void check_sector_column(
  renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect
) {
  register size_t i;
  size_t hits_count = 0;
  float planar_distance, point_distance;
  vec2f intersection;
  float intersectiond;
  linedef *line;
  line_hit hits[16];

  if (column->sector_depth == MAX_SECTOR_HISTORY) {
    return;
  }

  for (i = 0; i < column->sector_depth; ++i) {
    if (column->sector_history[i] == sect) {
      return;
    }
  }

  column->sector_history[column->sector_depth++] = sect;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];

#ifdef LINE_VIS_CHECK
    if (line->last_visible_tick != this->tick) {
      continue;
    }
#endif

    if (math_find_line_intersection(line->v0->point, line->v1->point, column->ray_start, column->ray_end, &intersection, &intersectiond)) {
      planar_distance = math_line_segment_point_perpendicular_distance(info->near_left, info->near_right, intersection);
      point_distance = planar_distance / column->theta;

      hits[hits_count++] = (line_hit) {
        .point = intersection,
        .planar_distance = planar_distance,
        .planar_distance_inv = 1.f / planar_distance,
        .point_distance = point_distance,
        .line = line,
        .back_sector = line->side_sector[0] == sect ? line->side_sector[1] : line->side_sector[0],
#if LIGHT_STEPS > 0
        .light_steps = (uint8_t)(point_distance / LIGHT_STEP_DISTANCE)
#else
        .light_falloff = point_distance / DIMMING_DISTANCE
#endif
      };
    }
  }

  switch (hits_count) {
  case 0: return;
  case 1: break;
  default: sort_nearest(hits, hits_count); break;
  }

  i = 0;
  do {
    draw_column(this, info, column, sect, &hits[i]);
  } while(++i < hits_count && !column->finished);
}

static void draw_column(
  renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  line_hit const *hit
) {
  const float depth_scale_factor = info->unit_size * hit->planar_distance_inv;
  const float ceiling_z_scaled   = sect->ceiling_height * depth_scale_factor;
  const float floor_z_scaled     = sect->floor_height * depth_scale_factor;
  const float view_z_scaled      = info->view_z * depth_scale_factor;
  const float ceiling_z_local    = info->half_h - ceiling_z_scaled + view_z_scaled;
  const float floor_z_local      = info->half_h - floor_z_scaled + view_z_scaled;
  const float wall_texture_step  = hit->planar_distance / info->unit_size;

  sector *back_sector = hit->back_sector;

  if (!back_sector || (back_sector && back_sector->floor_height == back_sector->ceiling_height)) {
    /* Draw a full wall */
    const float start_y = ceilf(M_MAX(ceiling_z_local, column->top_limit));
    const float end_y = M_CLAMP(floor_z_local, column->top_limit, column->bottom_limit);

    draw_wall_segment(
      info,
      column,
      sect,
      hit,
      start_y,
      end_y,
      view_z_scaled,
      wall_texture_step
    );

    draw_ceiling_segment(
      this,
      info,
      column,
      sect,
      hit,
      (sect->ceiling_height - info->view_z) * info->unit_size,
      column->top_limit,
      M_MIN(start_y, column->bottom_limit)
    );

    draw_floor_segment(
      this,
      info,
      column,
      sect,
      hit,
      (info->view_z - sect->floor_height) * info->unit_size,
      end_y+1,
      column->bottom_limit+1
    );

    column->finished = true;
  } else {
    /* Draw top and bottom segments of the wall and the sector behind */
    const float top_segment = math_max(sect->ceiling_height - back_sector->ceiling_height, 0) * depth_scale_factor;
    const float bottom_segment = math_max(back_sector->floor_height - sect->floor_height, 0) * depth_scale_factor;

    const float top_start_y = ceilf(math_clamp(ceiling_z_local, column->top_limit, column->bottom_limit));
    const float top_end_y = floorf(math_clamp(ceiling_z_local + top_segment, column->top_limit, column->bottom_limit));
    const float bottom_end_y = floorf(math_clamp(floor_z_local, column->top_limit, column->bottom_limit));
    const float bottom_start_y = math_clamp(floor_z_local - bottom_segment, column->top_limit, column->bottom_limit);

    if (top_segment > 0) {
      draw_wall_segment(info, column, sect, hit, top_start_y, top_end_y, view_z_scaled, wall_texture_step);
    }

    if (bottom_segment > 0) {
      draw_wall_segment(info, column, sect, hit, floorf(bottom_start_y), bottom_end_y, view_z_scaled, wall_texture_step);
    }

    draw_ceiling_segment(
      this,
      info,
      column,
      sect,
      hit,
      (sect->ceiling_height - info->view_z) * info->unit_size,
      column->top_limit,
      M_MAX(top_start_y, column->top_limit)
    );
    
    draw_floor_segment(
      this,
      info,
      column,
      sect,
      hit,
      (info->view_z - sect->floor_height) * info->unit_size,
      M_MIN(bottom_end_y, column->bottom_limit)+1,
      column->bottom_limit+1
    );

    column->top_limit = top_end_y;
    column->bottom_limit = bottom_start_y;

    if ((int)column->top_limit == (int)column->bottom_limit) {
      column->finished = true;
      return;
    }
    
    /* Render back sector */
    check_sector_column(this, info, column, back_sector);
  }
}

static void draw_wall_segment(
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const line_hit *hit,
  int32_t from,
  int32_t to,
  float view_z_scaled,
  float texture_step
) {
  if (from == to) {
    return;
  }

  register uint32_t y, res_counter = from % LIGHT_RESOLUTION;
  register uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  uint8_t c[3];
  float light=-1, tex_pos = ((from - info->half_h - view_z_scaled /*+ floor_z_scaled*/) * texture_step);

  memcpy(c, debug_colors[hit->line->color % 16], sizeof(uint8_t)*3);

  const int side = hit->line->side_sector[0] == sect ? 0 : 1;

#ifdef VECTORIZED_LIGHT_MUL
  int32_t temp[4];
#else
  register uint8_t r, g, b;
#endif

  for (y = from; y <= to; ++y, p += column->buffer_stride, tex_pos += texture_step) {
    c[0] = (int)floorf(tex_pos) & 127;

    if (!res_counter || light < 0) {
      light = math_max(
        calculate_light(
          sect,
          VEC3F(hit->point.x, hit->point.y, -tex_pos),
          SURFACE_WALL,
          hit->line->lights_count[side],
          hit->line->lights[side],
  #if LIGHT_STEPS > 0
          hit->light_steps
  #else
          hit->light_falloff
  #endif
        ),
        0.f
      );
    }

    res_counter = (res_counter+1)&(LIGHT_RESOLUTION-1);

#ifdef VECTORIZED_LIGHT_MUL
    __m128i result_i32 = _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, c[2], c[1], c[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f)));
    _mm_storeu_si128((__m128i*)temp, result_i32);
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    r = math_min((c[0] * light), 255);
    g = math_min((c[1] * light), 255);
    b = math_min((c[2] * light), 255);
    *p = 0xFF000000 | (r << 16) | (g << 8) | b;
#endif
  }
}

static void draw_floor_segment(
  renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const line_hit *hit,
  float distance_from_view,
  uint32_t from,
  uint32_t to
) {
  /* Camera below the floor */
  if (from == to || info->view_z < sect->floor_height) {
    return;
  }

  register uint32_t y, yz, res_counter = from % LIGHT_RESOLUTION;
  register uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  register float light=-1, distance, weight, wx, wy;

  uint8_t c[3];
  memcpy(c, debug_colors_dark[sect->color % 16], sizeof(uint8_t)*3);

#ifdef VECTORIZED_LIGHT_MUL
  int32_t temp[4];
#else
  register uint8_t r, g, b;
#endif

  for (y = from, yz = from - info->half_h; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz++]) / column->theta;
    weight = math_min(1.f, distance / hit->point_distance);
    wx = (weight * hit->point.x) + ((1-weight) * column->ray_start.x);
    wy = (weight * hit->point.y) + ((1-weight) * column->ray_start.y);

    c[1] = (int)truncf(wx) & 127;
    c[2] = (int)truncf(wy) & 127;

    if (!res_counter || light < 0) {
      light = math_max(
        calculate_light(
          sect,
          VEC3F(wx, wy, sect->floor_height),
          SURFACE_FLOOR,
          sect->lights_count,
          ((sector*)sect)->lights,
  #if LIGHT_STEPS > 0
          distance / LIGHT_STEP_DISTANCE
  #else
          distance / DIMMING_DISTANCE
  #endif
        ),
        0.f
      );
    }
    res_counter = (res_counter+1)&(LIGHT_RESOLUTION-1);

#ifdef VECTORIZED_LIGHT_MUL
    __m128i result_i32 = _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, c[2], c[1], c[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f)));
    _mm_storeu_si128((__m128i*)temp, result_i32);
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    r = math_min((c[0] * light), 255);
    g = math_min((c[1] * light), 255);
    b = math_min((c[2] * light), 255);
    *p = 0xFF000000 | (r << 16) | (g << 8) | b;
#endif
  } 
}

static void draw_ceiling_segment(
  renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const line_hit *hit,
  float distance_from_view,
  uint32_t from,
  uint32_t to
) {
  /* Camera above the ceiling */
  if (from == to || info->view_z > sect->ceiling_height) {
    return;
  }

  register uint32_t y, yz, res_counter = from % LIGHT_RESOLUTION;
  register uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  register float light=-1, distance, weight, wx, wy;
  uint8_t c[3];
  memcpy(c, debug_colors_dark[sect->color % 16], sizeof(uint8_t)*3);

#ifdef VECTORIZED_LIGHT_MUL
  int32_t temp[4];
#else
  register uint32_t r, g, b;
#endif

  for (y = from, yz = info->half_h - from; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz--]) / column->theta;
    weight = math_min(1.f, distance / hit->point_distance);
    wx = (weight * hit->point.x) + ((1-weight) * column->ray_start.x);
    wy = (weight * hit->point.y) + ((1-weight) * column->ray_start.y);

    c[0] = (int)truncf(wx) & 127;
    c[1] = (int)truncf(wy) & 127;

    if (!res_counter || light < 0) {
      light = math_max(
        calculate_light(
          sect,
          VEC3F(wx, wy, sect->ceiling_height),
          SURFACE_CEILING,
          sect->lights_count,
          ((sector*)sect)->lights,
  #if LIGHT_STEPS > 0
          distance / LIGHT_STEP_DISTANCE
  #else
          distance / DIMMING_DISTANCE
  #endif
        ),
        0.f
      );
    }
    res_counter = (res_counter+1)&(LIGHT_RESOLUTION-1);

#ifdef VECTORIZED_LIGHT_MUL
    __m128i result_i32 = _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, c[2], c[1], c[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f)));
    _mm_storeu_si128((__m128i*)temp, result_i32);
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    r = math_min((c[0] * light), 255);
    g = math_min((c[1] * light), 255);
    b = math_min((c[2] * light), 255);
    *p = 0xFF000000 | (r << 16) | (g << 8) | b;
#endif
  }
}
