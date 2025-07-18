#include "camera.h"
#include "level_data.h"
#include <stdio.h>

static void
find_current_sector(camera *this);

void
camera_init(camera *this, level_data *level)
{
  this->entity = (entity) {
    .level = level,
    .sector = NULL,
    .position = VEC2F(70, 70),
    .z = 64,
    .direction = vec2f_make(1, 0),
    .data = (void*)this,
    .type = ENTITY_CAMERA
  };

  this->fov = 1.0; /* ~90 degrees */
  this->pitch = 0.f;
  this->plane = vec2f_make(this->entity.direction.y * this->fov, -this->entity.direction.x * this->fov);

  find_current_sector(this);
}

void
camera_move(camera *this, float distance)
{
  this->entity.position = vec2f_add(this->entity.position, vec2f_mul(this->entity.direction, distance));

  if (!sector_point_inside(this->entity.sector, this->entity.position)) {
    find_current_sector(this);
  }
}

void
camera_rotate(camera *this, float rotation)
{
  const float oldDirX = this->entity.direction.x;
  const float oldPlaneX = this->plane.x;

  this->entity.direction.x = (this->entity.direction.x * cos(rotation)) - (this->entity.direction.y * sin(rotation));
  this->entity.direction.y = (oldDirX * sin(rotation)) + (this->entity.direction.y * cos(rotation));
  this->plane.x = (this->plane.x * cos(rotation)) - (this->plane.y * sin(rotation));
  this->plane.y = (oldPlaneX * sin(rotation)) + (this->plane.y * cos(rotation));
}

void
camera_set_fov(camera *this, float fov)
{
  this->fov = fov;
  this->plane = vec2f_make(this->entity.direction.y*fov, -this->entity.direction.x*fov);
}

static void
find_current_sector(camera *this)
{
  register size_t i;

  for (i = 0; i < this->entity.level->sectors_count; ++i) {
    if (&this->entity.level->sectors[i] == this->entity.sector) { continue; }
    // printf("Check sector %d\n", i);
    if (sector_point_inside(&this->entity.level->sectors[i], this->entity.position)) {
      this->entity.sector = (sector *)&this->entity.level->sectors[i];
      printf("Camera entered sector: %d\n", i);
      break;
    }
  }
}
