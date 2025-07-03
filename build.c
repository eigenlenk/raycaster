#define NOB_IMPLEMENTATION
#include "deps/nob.h"
#include <string.h>

#define BIN_FOLDER    "bin/"
#define SRC_FOLDER    "src/"
#define DEPS_FOLDER   "deps/"
#define TESTS_FOLDER  "tests/"
#define DEMO_FOLDER   "demo/"
/* SDL3 sub-directory in DEPS folder */
#define SDL3_FOLDER   "SDL3-devel-3.2.16-mingw/i686-w64-mingw32/"

/* Common source files between test and demo target */
#define COMMON_SRC_FILES \
  DEPS_FOLDER"gpc/gpc.c", \
  SRC_FOLDER"renderer.c", \
  SRC_FOLDER"camera.c", \
  SRC_FOLDER"sector.c", \
  SRC_FOLDER"level_data.c", \
  SRC_FOLDER"map_builder/map_builder.c", \
  SRC_FOLDER"map_builder/polygon.c",

int main(int argc, char **argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);
  
  enum targets {
    TARGET_TESTS = 1,
    TARGET_SDL3_DEMO = 2
  };
  
  unsigned int targets_included = 0;
  
  if (argc == 1) {
    printf("Usage: build <target>\n\n");
    printf("Builds the specified target.\n\n");
    printf("Arguments:\n");
    printf("\ttests\tBuilds the test target\n");
    printf("\tdemo\tBuilds the demo target\n");
    printf("\tall\tBuilds both test and demo targets\n");
    return 0;
  } else {
    int i;
    for (i = 1; i < argc; ++i) {
      if (!strcmp(argv[i], "all")) {
        targets_included = TARGET_TESTS | TARGET_SDL3_DEMO;
      } else if (!strcmp(argv[i], "tests")) {
        targets_included |= TARGET_TESTS;
      } else if (!strcmp(argv[i], "demo")) {
        targets_included |= TARGET_SDL3_DEMO;
      } else {
        printf("Unknown target '%s'!\n", argv[i]);
        return 1;
      }
    }
  }

  if (!nob_mkdir_if_not_exists(BIN_FOLDER)) {
    return 1;
  }

  Nob_Cmd cmd = { 0 };

  if (targets_included & TARGET_TESTS) {
    nob_cmd_append(
      &cmd,
      "gcc",
      "-std=gnu99",
      "-Wall",
      "-Wfatal-errors",
      
      "-I"SRC_FOLDER,
      "-I"SRC_FOLDER"include",
      "-I"SRC_FOLDER"map_builder/include",
      "-I"TESTS_FOLDER,
      "-I"DEPS_FOLDER"unity/src/",
      "-I"DEPS_FOLDER"unity/extras/fixture/src/",
      "-I"DEPS_FOLDER"gpc",

      "-DUNITY_INCLUDE_PRINT_FORMATTED",
      "-DUNITY_INCLUDE_DOUBLE",
      // "-DDEBUG",

      "-o", BIN_FOLDER"tests",

      /* Input files */
      COMMON_SRC_FILES
      DEPS_FOLDER"unity/src/unity.c",
      DEPS_FOLDER"unity/extras/fixture/src/fixture.c",
      TESTS_FOLDER"main.c",
      TESTS_FOLDER"math.c",
      TESTS_FOLDER"polygon.c",
      TESTS_FOLDER"sector.c",
      TESTS_FOLDER"map_builder.c",

      /* Linked libraries */
      "-lm",
      "-fopenmp"
    );

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
  }
  
  if (targets_included & TARGET_SDL3_DEMO) {
    nob_cmd_append(
      &cmd,
      "gcc",
      "-std=gnu99",
      "-Wall",
      "-Wfatal-errors",
      "-g",
      // "-pg", /* For GPROF */
      "-no-pie",
      "-flto",
      "-ffast-math",
      "-funroll-loops",
      "-fomit-frame-pointer",
      "-O3",
      
      "-I"SRC_FOLDER,
      "-I"SRC_FOLDER"include",
      "-I"SRC_FOLDER"map_builder/include",
      "-I"DEMO_FOLDER,
      "-I"DEPS_FOLDER""SDL3_FOLDER"include",
      "-I"DEPS_FOLDER"gpc",
      
      "-DDEBUG",
      "-DLIGHT_STEPS=0", /* 0 = smooth light falloff */
      "-DLINE_VIS_CHECK",
      "-DPARALLEL_RENDERING", /* Uses OpenMP to render the screen columns in parallel */
      "-DVECTORIZED_LIGHT_MUL", /* Vectorizes light multiplication for walls, floors & ceilings */
      "-DDYNAMIC_SHADOWS", 

      "-o", BIN_FOLDER"demo",

      /* Input files */
      DEMO_FOLDER"main.c",
      COMMON_SRC_FILES

      /* Linked libraries */
      DEPS_FOLDER""SDL3_FOLDER"lib/libSDL3.dll.a",
      "-lm",
      "-msse2",
      "-mfpmath=sse",
      "-fopenmp",
      "-fopenmp-simd"
    );

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    nob_copy_directory_recursively(DEPS_FOLDER""SDL3_FOLDER"bin/SDL3.dll", BIN_FOLDER"SDL3.dll");
  }

  return 0;
}
