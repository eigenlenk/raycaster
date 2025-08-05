#include "unity.h"
#include "fixture.h"
#include "map_builder.h"
#include "level_data.h"
#include "map_cache.h"
#include <time.h>

static level_data*
create_level();

TEST_GROUP(level_data);

TEST_SETUP(level_data) {}
TEST_TEAR_DOWN(level_data) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(level_data, intersect_3d)
{
  level_data *level = create_level();

  map_cache_intersect_3d(
    &level->cache,
    VEC3F(0, 0, 128),
    VEC3F(4095, 4095, 128)
  );

  map_cache_intersect_3d(
    &level->cache,
    VEC3F(155.333328, 155.333328, 128),
    VEC3F(300.000000, 400.000000, 156)
  );
}

TEST_GROUP_RUNNER(level_data)
{
  RUN_TEST_CASE(level_data, intersect_3d);
}

static level_data*
create_level()
{
  const int w = 32;
  const int h = 32;
  const int size = 128;
  register int x, y, c, f;

  map_builder builder = { 0 };

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      if ((x^y) % 20 == 5) {
        c = f = 0;
      } else {
        f = 8 * (rand() % 16);
        c = 1024 - 32 * (rand() % 24);
      }

      map_builder_add_polygon(&builder, f, c, 1.f, TEXTURE_NONE, TEXTURE_NONE, VERTICES(
        VEC2F(-100+x*size, -100+y*size),
        VEC2F(-100+x*size + size, -100+y*size),
        VEC2F(-100+x*size + size, -100+y*size + size),
        VEC2F(-100+x*size, -100+y*size + size)
      ), SIDE_CONFIG(TEXLIST(TEXTURE_NONE)), UNIFORM_LINES());
    }
  }

  level_data *level = map_builder_build(&builder);

  for (x = 0; x < level->vertices_count; ++x) {
    level->vertices[x].point.x += (-24 + rand() % 48);
    level->vertices[x].point.y += (-24 + rand() % 48);
  }
  
  map_builder_free(&builder);

  return level;
}
