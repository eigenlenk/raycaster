cmake_minimum_required(VERSION 3.20)
project(raycaster C)

option(RAYCASTER_DEBUG "Enable raycaster debug mode" ON)
option(RAYCASTER_PRERENDER_VISCHECK "Enable linedef visibility checks before rendering a frame" ON)
option(RAYCASTER_PARALLEL_RENDERING "Enable OpenMP parallel rendering" ON)
option(RAYCASTER_SIMD_PIXEL_LIGHTING "Enables SIMD codepath when multiplying texture RGB with light value" ON)
option(RAYCASTER_DYNAMIC_SHADOWS "Enable raytraced shadows" ON)
set(RAYCASTER_LIGHT_STEPS 0 CACHE STRING "Number of light steps [0...255] (0 = smooth lighting, higher values = less banding)")

set(RAYCASTER_DEFINES
  $<$<BOOL:${RAYCASTER_DEBUG}>:RAYCASTER_DEBUG>
  $<$<BOOL:${RAYCASTER_PRERENDER_VISCHECK}>:RAYCASTER_PRERENDER_VISCHECK>
  $<$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>:RAYCASTER_PARALLEL_RENDERING>
  $<$<BOOL:${RAYCASTER_SIMD_PIXEL_LIGHTING}>:RAYCASTER_SIMD_PIXEL_LIGHTING>
  $<$<BOOL:${RAYCASTER_DYNAMIC_SHADOWS}>:RAYCASTER_DYNAMIC_SHADOWS>
  RAYCASTER_LIGHT_STEPS=${RAYCASTER_LIGHT_STEPS}
)

if (APPLE)
  set(OpenMP_C_FLAGS "-I/opt/homebrew/opt/libomp/include")
  set(OpenMP_C_LIB_NAMES "omp")
  set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
  find_package(OpenMP)
endif()

# Does the demo target need these flags, or just the renderer?
if (CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  list(APPEND RAYCASTER_FLAGS
    -Wall
    # -Wextra
    -Wpedantic
    -Wfatal-errors
    -g
    -no-pie
    -ffast-math
    -funroll-loops
    -fomit-frame-pointer
    -O3
    -msse2
    -mfpmath=sse
    $<$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>:-fopenmp>
    $<$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>:-fopenmp-simd>
  )

elseif (CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
  list(APPEND RAYCASTER_FLAGS
    -Wall
    -Wpedantic
    -Wfatal-errors
    -g
    -ffast-math
    -funroll-loops
    -fomit-frame-pointer
    -O3
    -march=native
    -mcpu=native
    -flto
    # -Rpass=loop-vectorize
    # -Rpass-analysis=loop-vectorize
    $<$<AND:$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>,$<BOOL:${OpenMP_C_FOUND}>>:-Xclang -fopenmp ${OpenMP_C_FLAGS}>
  )

elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  list(APPEND RAYCASTER_FLAGS
    /W3
    /Zi
    # /Od
    /Oy
    /Oi
    $<$<CONFIG:Debug>:/Ot>
    $<$<CONFIG:Release>:/O2>
    /fp:fast
    /arch:SSE2
    $<$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>:/openmp>
  )

endif()

# CMake had me separate the renderer from the demo/tests, so I'm unable to get LTO working at this time,
# since they're built separately, but I think it made no difference anyway

# if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
#   set(CMAKE_AR gcc-ar)
#   set(CMAKE_RANLIB gcc-ranlib)
#   set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
#   set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
# endif()

set(CMAKE_C_STANDARD 99)

find_package(SDL3 3.2.16 CONFIG COMPONENTS SDL3-shared)
find_package(SDL3_image 3.2.4 CONFIG COMPONENTS SDL3_image-shared)

if(NOT SDL3_FOUND OR NOT SDL3_image_FOUND)
  include(FetchContent)
  if(NOT SDL3_FOUND)
    # SDL3
    FetchContent_Declare(
      SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.2.16
    )
    FetchContent_MakeAvailable(SDL3)
  endif()
  if(NOT SDL3_image_FOUND)
    # SDL3_image
    FetchContent_Declare(
      SDL3_image
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
      GIT_TAG release-3.2.4
    )
    FetchContent_MakeAvailable(SDL3_image)
  endif()
endif()

# get libm on unix-like systems
if(UNIX OR NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  find_library(libmath m)
endif()


############
# RENDERER #
############

file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS src/*.c deps/gpc/gpc.c)
add_library(renderer STATIC ${CORE_SRC})
if(libmath)
  target_link_libraries(renderer PUBLIC ${libmath})
endif()
if(OpenMP_C_FOUND AND RAYCASTER_PARALLEL_RENDERING)
  target_link_libraries(renderer PUBLIC OpenMP::OpenMP_C)
endif()
target_include_directories(renderer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/map_builder/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/gpc
)
target_compile_definitions(renderer PRIVATE ${RAYCASTER_DEFINES})
target_compile_options(renderer
  PRIVATE
    ${RAYCASTER_FLAGS}
    # Uncomment if profiling with gprof
    # -pg
)


###############
# DEMO TARGET #
###############

file(GLOB DEMO_SOURCES CONFIGURE_DEPENDS demo/*.c)
add_executable(demo ${DEMO_SOURCES})
target_link_libraries(demo PRIVATE renderer SDL3::SDL3 SDL3_image::SDL3_image)

if (CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  target_link_options(demo PRIVATE $<$<BOOL:${RAYCASTER_PARALLEL_RENDERING}>:-fopenmp>)
endif()

if(OpenMP_C_FOUND AND RAYCASTER_PARALLEL_RENDERING)
  target_link_libraries(demo PRIVATE OpenMP::OpenMP_C)
endif()

target_compile_definitions(demo PRIVATE ${RAYCASTER_DEFINES})
target_compile_options(demo PRIVATE ${RAYCASTER_FLAGS})

if($<BOOL:$<TARGET_RUNTIME_DLLS:demo>>)
  add_custom_command(TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:demo>
      $<TARGET_FILE_DIR:demo>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying DLL files"
  )
endif()

add_custom_command(TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/demo/res"
    $<TARGET_FILE_DIR:demo>/res
  COMMENT "Copying demo resources"
)


##############
# UNIT TESTS #
##############

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.c deps/unity/src/*.c deps/unity/extras/fixture/src/*.c)
add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE renderer)
target_include_directories(tests
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/unity/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/unity/extras/fixture/src
)
target_compile_definitions(tests PRIVATE
  ${RAYCASTER_DEFINES}
  UNITY_INCLUDE_PRINT_FORMATTED
  UNITY_INCLUDE_DOUBLE
)

enable_testing()
add_test(NAME tests COMMAND tests)
