#include "unity.h"
#include "fixture.h"
#include "maths.h"

TEST_GROUP(math);

TEST_SETUP(math) {}
TEST_TEAR_DOWN(math) {}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(math, find_line_intersection)
{
  vec2f r;
  float d;

  if (math_find_line_intersection(
    vec2f_make(0, 5),
    vec2f_make(10, 5),
    vec2f_make(5, 0),
    vec2f_make(5, 10),
    &r,
    &d
  )) {
    TEST_ASSERT(vec2f_equals(vec2f_make(5, 5), r));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, d);
  } else {
    TEST_FAIL_MESSAGE("Lines should have intersected");
  }

  if (math_find_line_intersection(
    vec2f_make(5, 10),
    vec2f_make(5, 0),
    vec2f_make(0, 0),
    vec2f_make(10, 0),
    &r,
    &d
  )) {
    TEST_ASSERT(vec2f_equals(vec2f_make(5, 0), r));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, d);
  } else {
    TEST_FAIL_MESSAGE("Lines should have intersected");
  }

  if (math_find_line_intersection(
    vec2f_make(0, 0),
    vec2f_make(10, 0),
    vec2f_make(15, -10),
    vec2f_make(15, 10),
    &r,
    &d
  )) {
    TEST_FAIL_MESSAGE("Lines should not have intersected");
  }

  if (math_find_line_intersection(VEC2F(0,0), VEC2F(256,0), VEC2F(512,0), VEC2F(768,0), &r, &d)) {
    TEST_FAIL_MESSAGE("Lines should not have intersected");
  }

  if (math_find_line_intersection(VEC2F(0,250), VEC2F(200,250), VEC2F(100,250), VEC2F(300,250), &r, &d)) {
    TEST_FAIL_MESSAGE("Lines should not have intersected");
  }
}

TEST(math, line_segment_point_perpendicular_distance)
{
  const float d0 = math_line_segment_point_perpendicular_distance(vec2f_make(0, 0), vec2f_make(10, 0), vec2f_make(5, 5));
  const float d1 = math_line_segment_point_perpendicular_distance(vec2f_make(0, 0), vec2f_make(10, 0), vec2f_make(1, 1));
  const float d2 = math_line_segment_point_perpendicular_distance(vec2f_make(0, 0), vec2f_make(10, 0), vec2f_make(10, 0));

  TEST_ASSERT_EQUAL_DOUBLE(5.0, d0);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, d1);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, d2);
}

TEST(math, sign)
{
  int32_t s0 = math_sign(VEC2F(0, 0), VEC2F(10, 10), VEC2F(2, 5));
  int32_t s1 = math_sign(VEC2F(0, 0), VEC2F(10, 10), VEC2F(7, 5));
  int32_t s2 = math_sign(VEC2F(0, 0), VEC2F(10, 10), VEC2F(5, 5));

  TEST_ASSERT(s0 > 0);
  TEST_ASSERT(s1 < 0);
  TEST_ASSERT(s2 == 0);
}

TEST(math, point_in_triangle)
{
  TEST_ASSERT_TRUE(math_point_in_triangle(VEC2F(0, 0), VEC2F(0, -5), VEC2F(-5, 5), VEC2F(5, 5)));
  TEST_ASSERT_TRUE(math_point_in_triangle(VEC2F(1, 3), VEC2F(0, -5), VEC2F(-5, 5), VEC2F(5, 5)));
  TEST_ASSERT_FALSE(math_point_in_triangle(VEC2F(0, -6), VEC2F(0, -5), VEC2F(-5, 5), VEC2F(5, 5)));
}

TEST(math, line_segments_intersect)
{
  TEST_ASSERT_TRUE(math_line_segments_intersect(VEC2F(0, 0), VEC2F(100, 0), VEC2F(50, 0), VEC2F(150, 0)));
  TEST_ASSERT_FALSE(math_line_segments_intersect(VEC2F(0, 0), VEC2F(90, 0), VEC2F(91, 0), VEC2F(150, 0)));
  TEST_ASSERT_TRUE(math_line_segments_intersect(VEC2F(-10, 0), VEC2F(10, 0), VEC2F(0, -10), VEC2F(0, 10)));
  TEST_ASSERT_TRUE(math_line_segments_intersect(VEC2F(0, 0), VEC2F(10, 0), VEC2F(0, -10), VEC2F(0, 0)));
}

TEST_GROUP_RUNNER(math)
{
  RUN_TEST_CASE(math, find_line_intersection);
  RUN_TEST_CASE(math, line_segment_point_perpendicular_distance);
  RUN_TEST_CASE(math, sign);
  RUN_TEST_CASE(math, point_in_triangle);
  RUN_TEST_CASE(math, line_segments_intersect);
}
