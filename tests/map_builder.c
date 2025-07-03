#include "unity.h"
#include "fixture.h"
#include "map_builder.h"
#include "level_data.h"

TEST_GROUP(map_builder);

TEST_SETUP(map_builder) {}
TEST_TEAR_DOWN(map_builder) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(map_builder, convex_polygon)
{
  int i;
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  TEST_ASSERT_EQUAL_INT(0, builder.polygons[0].floor_height);
  TEST_ASSERT_EQUAL_INT(128, builder.polygons[0].ceiling_height);
  TEST_ASSERT_EQUAL_DOUBLE(1, builder.polygons[0].brightness);

  TEST_ASSERT_EQUAL_PTR(&builder.polygons[0], map_builder_polygon_at_point(&builder, VEC2F(50, 75)));
  TEST_ASSERT_NULL(map_builder_polygon_at_point(&builder, VEC2F(-10, -10)));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(4, level->vertices_count);
  TEST_ASSERT_EQUAL(4, level->linedefs_count);
  TEST_ASSERT_EQUAL(1, level->sectors_count);

  for (i = 0; i < level->linedefs_count; ++i) {
    TEST_ASSERT_NOT_NULL(level->sectors[0].linedefs[i]->side_sector[0]); /* Front */
    TEST_ASSERT_NULL(level->sectors[0].linedefs[i]->side_sector[1]); /* Back */
  }

  TEST_ASSERT_TRUE(sector_point_inside(&level->sectors[0], VEC2F(50, 75)));
  TEST_ASSERT_FALSE(sector_point_inside(&level->sectors[0], VEC2F(-10, -10)));

  free(level);
  map_builder_free(&builder);
}

TEST(map_builder, concave_polygon)
{
  int i;
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 0.5, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(50, 50),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  TEST_ASSERT_NULL(map_builder_polygon_at_point(&builder, VEC2F(50, 75)));
  TEST_ASSERT_EQUAL_PTR(&builder.polygons[0], map_builder_polygon_at_point(&builder, VEC2F(10, 10)));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(5, level->vertices_count);
  TEST_ASSERT_EQUAL(5, level->linedefs_count);
  TEST_ASSERT_EQUAL(1, level->sectors_count);

  TEST_ASSERT_EQUAL_INT(0, level->sectors[0].floor_height);
  TEST_ASSERT_EQUAL_INT(128, level->sectors[0].ceiling_height);
  TEST_ASSERT_EQUAL_DOUBLE(0.5, level->sectors[0].brightness);

  for (i = 0; i < level->linedefs_count; ++i) {
    TEST_ASSERT_NOT_NULL(level->sectors[0].linedefs[i]->side_sector[0]); /* Front */
    TEST_ASSERT_NULL(level->sectors[0].linedefs[i]->side_sector[1]); /* Back */
  }

  TEST_ASSERT_FALSE(sector_point_inside(&level->sectors[0], VEC2F(50, 75)));
  TEST_ASSERT_TRUE(sector_point_inside(&level->sectors[0], VEC2F(10, 10)));

  free(level);
  map_builder_free(&builder);
}

/*
 * There are a few ways two or more sectors can be connected:
 * 
 * 1) Non-overlapping sectors can connect simply by sharing a linedef.
 * The front side references the original sector; the back references the other.
 */
TEST(map_builder, neighbouring_sectors)
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 100, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  map_builder_add_polygon(&builder, 10, 90, 1, VERTICES(
    VEC2F(100, 0),
    VEC2F(100, 100),
    VEC2F(200, 100),
    VEC2F(200, 0)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(6, level->vertices_count);
  TEST_ASSERT_EQUAL(7, level->linedefs_count);
  TEST_ASSERT_EQUAL(2, level->sectors_count);

  /* Same linedef is referenced by both sectors */
  TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[0].linedefs[2]->side_sector[0]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[1], level->sectors[1].linedefs[0]->side_sector[1]);
  TEST_ASSERT_EQUAL_PTR(level->sectors[0].linedefs[2], level->sectors[1].linedefs[0]);

  free(level);
  map_builder_free(&builder);
}

/*
 * 2) If a sector is fully inside another, its linedefs are added with the outer sector as the back.
 */
TEST(map_builder, fully_contained_sector)
{
  int i;
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 100, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  map_builder_add_polygon(&builder, 10, 90, 1, VERTICES(
    VEC2F(25, 25),
    VEC2F(75, 25),
    VEC2F(75, 75),
    VEC2F(25, 75)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(8, level->vertices_count);
  TEST_ASSERT_EQUAL(8, level->linedefs_count);
  TEST_ASSERT_EQUAL(2, level->sectors_count);
  TEST_ASSERT_EQUAL(8, level->sectors[0].linedefs_count);
  TEST_ASSERT_EQUAL(4, level->sectors[1].linedefs_count);

  for (i = 0; i < level->sectors[1].linedefs_count; ++i) {
    /* All linedefs of sector 1 refer to sector 0 */
    TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[1].linedefs[i]->side_sector[1]);
  }

  TEST_ASSERT_FALSE(sector_point_inside(&level->sectors[0], VEC2F(50, 50)));
  TEST_ASSERT_TRUE(sector_point_inside(&level->sectors[1], VEC2F(50, 50)));

  free(level);
  map_builder_free(&builder);
}

/*
 * 3) Sector can be fully inside another while still sharing a linedef wholly
 * or just partially, in which case the encompassing sector gets additional vertices.
 */
TEST(map_builder, fully_contained_sector_sharing_linedef)
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 100, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  map_builder_add_polygon(&builder, 10, 90, 1, VERTICES(
    VEC2F(50, 25),
    VEC2F(100, 25),
    VEC2F(100, 75),
    VEC2F(50, 75)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL(8, level->vertices_count);
  TEST_ASSERT_EQUAL(9, level->linedefs_count);
  TEST_ASSERT_EQUAL(2, level->sectors_count);

  /*
   * Here's how our sectors are supposed to look like
   *
   * ┌────────────┐
   * │            │
   * │            │
   * │      ┌─────┘
   * │      │
   * │      │
   * │      └─────┐
   * │            │
   * │            │
   * └────────────┘
   *
   * ┌            ┐
   * 
   * 
   *        ┌─────┐
   *        │     │
   *        │     │
   *        └─────┘
   *
   *
   * └            ┘
   */

  TEST_ASSERT_EQUAL(8, level->sectors[0].linedefs_count);
  TEST_ASSERT_EQUAL(4, level->sectors[1].linedefs_count);

  TEST_ASSERT_NULL(level->sectors[1].linedefs[1]->side_sector[1]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[1].linedefs[0]->side_sector[0]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[1].linedefs[2]->side_sector[0]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[0], level->sectors[1].linedefs[3]->side_sector[0]);

  TEST_ASSERT_EQUAL_PTR(&level->sectors[1], level->sectors[0].linedefs[0]->side_sector[1]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[1], level->sectors[0].linedefs[1]->side_sector[1]);
  TEST_ASSERT_EQUAL_PTR(&level->sectors[1], level->sectors[0].linedefs[2]->side_sector[1]);

  free(level);
  map_builder_free(&builder);
}

/*
 * 4) Partially overlapping sectors create new vertices and linedefs at intersections,
 * replacing existing ones. The earlier-defined sector is carved out.
 */
TEST(map_builder, intersecting_sectors)
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 100, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(0, 100),
    VEC2F(100, 100),
    VEC2F(100, 0)
  ));

  map_builder_add_polygon(&builder, 10, 90, 1, VERTICES(
    VEC2F(50, 25),
    VEC2F(150, 25),
    VEC2F(150, 75),
    VEC2F(50, 75)
  ));

  level_data *level = map_builder_build(&builder);

  /*
   * Here's how that's supposed to look like
   *
   * ┌────────────┐
   * │ A          │
   * │      ╔═════v3════╗
   * │      ║ B         ║
   * │      ║           ║
   * │      ╚═════v1════╝
   * │            │
   * └────────────┘
   *
   */

  TEST_ASSERT_EQUAL(10, level->vertices_count);
  TEST_ASSERT_EQUAL(2, level->sectors_count);

  TEST_ASSERT_EQUAL(8, level->sectors[0].linedefs_count);
  TEST_ASSERT_EQUAL(6, level->sectors[1].linedefs_count);

  TEST_ASSERT_FALSE(sector_connects_vertices(&level->sectors[0], &level->vertices[3], &level->vertices[1]));

  free(level);
  map_builder_free(&builder);
}

TEST(map_builder, polygon_splitting)
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(500, 0),
    VEC2F(500, 100),
    VEC2F(0, 100)
  ));

  /* This sector will split the first one so you end up with 3 sectors */
  map_builder_add_polygon(&builder, 16, 112, 1, VERTICES(
    VEC2F(225, -250),
    VEC2F(325, -250),
    VEC2F(325, 250),
    VEC2F(225, 250)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL_INT(3, level->sectors_count);

  free(level);
  map_builder_free(&builder);
}

TEST(map_builder, optimize_polygon)
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 1, VERTICES(
    VEC2F(0, 0),
    VEC2F(250, 0), /* Unless any other polygon uses that point as well, it's unnecessary */
    VEC2F(500, 0),
    VEC2F(500, 100),
    VEC2F(0, 100)
  ));

  level_data *level = map_builder_build(&builder);

  TEST_ASSERT_EQUAL_INT(4, level->vertices_count);

  free(level);
  map_builder_free(&builder);
}

TEST_GROUP_RUNNER(map_builder)
{
  RUN_TEST_CASE(map_builder, convex_polygon);
  RUN_TEST_CASE(map_builder, concave_polygon);
  RUN_TEST_CASE(map_builder, neighbouring_sectors);
  RUN_TEST_CASE(map_builder, fully_contained_sector);
  RUN_TEST_CASE(map_builder, fully_contained_sector_sharing_linedef);
  RUN_TEST_CASE(map_builder, intersecting_sectors);
  RUN_TEST_CASE(map_builder, polygon_splitting);
  RUN_TEST_CASE(map_builder, optimize_polygon);
}
