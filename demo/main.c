#define SDL_MAIN_USE_CALLBACKS 1
#include "renderer.h"
#include "camera.h"
#include "level_data.h"
#include "map_builder.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SMALL_BRICKS_TEXTURE 0
#define LARGE_BRICKS_TEXTURE 1
#define FLOOR_TEXTURE 2
#define CEILING_TEXTURE 3
#define WOOD_TEXTURE 4
#define SKY_TEXTURE 5
#define METAL_GRATING 6
#define METAL_BARS 7
#define GRASS_TEXTURE 8
#define DIRT_TEXTURE 9
#define STONEWALL_TEXTURE 10
#define METAL_STONE_TEXTURE 11
#define MIRROR_TEXTURE 12

SDL_Window* window = NULL;
SDL_Renderer *sdl_renderer = NULL;
SDL_Texture *texture = NULL;

static renderer rend;
static camera cam;
static level_data *demo_level = NULL;
static light *dynamic_light = NULL;
static float light_z, light_movement_range = 48;
static uint64_t last_ticks;
static float delta_time;
static const int initial_window_width = 1024,
                 initial_window_height = 768;
static int scale = 1;
static bool fullscreen = false;
static bool nearest = true;
static bool lock_aspect_ratio = false;
static double aspect_ratio;
static bool info_text_visible = true;

static struct {
  sector *ref;
  int direction, distance;
  float timer;
} moving_sector;

static SDL_Surface *textures[32];

static struct {
  float forward, turn, raise, pitch;
} movement = { 0 };

static void create_demo_level();
static void create_grid_level();
static void create_big_one();
static void create_semi_intersecting_sectors();
static void create_crossing_and_splitting_sectors();
static void create_mirrors_and_large_sky();
static void load_level(int);
static void process_camera_movement(const float delta_time);

M_INLINED void
demo_texture_sampler_scaled(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);

M_INLINED void
demo_texture_sampler_normalized(texture_ref, float, float, uint8_t, uint8_t*, uint8_t*);

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
static void
demo_renderer_step(const renderer*);
#endif

M_INLINED vec2i
renderer_size_in_window(int wndw, int wndh)
{
  if (lock_aspect_ratio) {
    int h = wndh / scale;
    int w = h * aspect_ratio;
    if (w > wndw) {
      w = wndw;
      h = w / aspect_ratio;
    }
    return VEC2I(w, h);
  } else {
    return VEC2I(wndw / scale, wndh / scale);
  }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return -1;
  }

  int i;
  int level = 0;
  int vsync = 0;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-level")) {
      level = atoi(argv[i+1]);
    } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-fullscreen")) {
      fullscreen = true;
    } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-scale")) {
      scale = M_MAX(1, atoi(argv[i+1]));
    } else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "-aspect")) {
      lock_aspect_ratio = true;
      aspect_ratio = (double)atoi(argv[i+1]) / atoi(argv[i+2]);
    } else if (!strcmp(argv[i], "-vsync")) {
      vsync = atoi(argv[i+1]);
    }
  }

  SDL_CreateWindowAndRenderer(
    "Software Rendering Example",
    initial_window_width,
    initial_window_height,
    SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
    &window,
    &sdl_renderer
  );

  if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    return -1;
  }

  printf("sdl_renderer: %s\n", SDL_GetRendererName(sdl_renderer));

  SDL_SetRenderVSync(sdl_renderer, vsync);

  renderer_init(&rend, renderer_size_in_window(initial_window_width, initial_window_height));

  if (lock_aspect_ratio) {
    SDL_SetRenderLogicalPresentation(sdl_renderer, initial_window_height * aspect_ratio, initial_window_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  }

  if (!rend.buffer) {
    return -1;
  }

  texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
  
  if (!texture) return -1;

  SDL_SetTextureScaleMode(texture, nearest?SDL_SCALEMODE_NEAREST:SDL_SCALEMODE_LINEAR);

  textures[SMALL_BRICKS_TEXTURE] = IMG_Load("res/small_bricks.png");
  textures[LARGE_BRICKS_TEXTURE] = IMG_Load("res/large_bricks.png");
  textures[FLOOR_TEXTURE] = IMG_Load("res/floor.png");
  textures[CEILING_TEXTURE] = IMG_Load("res/ceiling.png");
  textures[WOOD_TEXTURE] = IMG_Load("res/wood.png");
  textures[SKY_TEXTURE] = IMG_Load("res/sky.png");
  textures[METAL_GRATING] = IMG_Load("res/grating.png");
  textures[METAL_BARS] = IMG_Load("res/bars.png");
  textures[GRASS_TEXTURE] = IMG_Load("res/grass.png");
  textures[DIRT_TEXTURE] = IMG_Load("res/dirt.png");
  textures[STONEWALL_TEXTURE] = IMG_Load("res/stonewall.png");
  textures[METAL_STONE_TEXTURE] = IMG_Load("res/metal_stone.png");
  textures[MIRROR_TEXTURE] = IMG_Load("res/mirror.png");

  load_level(level);

  last_ticks = SDL_GetTicks();

  texture_sampler_scaled = demo_texture_sampler_scaled;
  texture_sampler_normalized = demo_texture_sampler_normalized;

  return 0;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
  renderer_destroy(&rend);
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
      return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
      if (event->key.key == SDLK_W) { movement.forward = 1.f; }
      else if (event->key.key == SDLK_S) { movement.forward = -1.f; }
      
      if (event->key.key == SDLK_A) { movement.turn = 1.f; }
      else if (event->key.key == SDLK_D) { movement.turn = -1.f; }
      
      if (event->key.key == SDLK_Q) { movement.raise = 1.f; }
      else if (event->key.key == SDLK_Z) { movement.raise = -1.f; }

      if (event->key.key == SDLK_E) { movement.pitch = 1.f; }
      else if (event->key.key == SDLK_C) { movement.pitch = -1.f; }

      if (event->key.key == SDLK_PLUS ||event->key.key == SDLK_MINUS) {
        if (event->key.key == SDLK_PLUS) { scale += 1; }
        else if (scale > 1) { scale -= 1; }
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        printf("Resize buffer to %dx%d\n", w / scale, h / scale);
        renderer_resize(&rend, renderer_size_in_window(w, h));
        SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
        SDL_SetTextureScaleMode(texture, nearest?SDL_SCALEMODE_NEAREST:SDL_SCALEMODE_LINEAR);
      }

      if (event->key.key == SDLK_P) {
        camera_set_fov(&cam, M_MAX(0.1f, cam.fov*0.9));
      } else if (event->key.key == SDLK_O) {
        camera_set_fov(&cam, M_MIN(4.0f, cam.fov*1.1));
      }

      if (event->key.key == SDLK_HOME) {
        cam.entity.sector->ceiling.height += 2;
        sector_update_floor_ceiling_limits(cam.entity.sector);
      } else if (event->key.key == SDLK_END) {
        cam.entity.sector->ceiling.height = M_MAX(cam.entity.sector->floor.height, cam.entity.sector->ceiling.height - 2);
        sector_update_floor_ceiling_limits(cam.entity.sector);
      }

      if (event->key.key == SDLK_PAGEUP) {
        cam.entity.sector->floor.height = M_MIN(cam.entity.sector->ceiling.height, cam.entity.sector->floor.height + 2);
        sector_update_floor_ceiling_limits(cam.entity.sector);
      } else if (event->key.key == SDLK_PAGEDOWN) {
        cam.entity.sector->floor.height -= 2;
        sector_update_floor_ceiling_limits(cam.entity.sector);
      }

      if (event->key.key == SDLK_K) {
        cam.entity.sector->brightness = M_MAX(0.f, cam.entity.sector->brightness - 0.1f);
      } else if (event->key.key == SDLK_L) {
        cam.entity.sector->brightness = M_MIN(4.f, cam.entity.sector->brightness + 0.1f);
      }

      if (event->key.key == SDLK_M) {
        nearest = !nearest;
        SDL_SetTextureScaleMode(texture, nearest?SDL_SCALEMODE_NEAREST:SDL_SCALEMODE_LINEAR);
      } else if (event->key.key == SDLK_H) {
        info_text_visible = !info_text_visible;
      } else if (event->key.key == SDLK_F) {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(window, fullscreen);
      }
#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
      if (event->key.key == SDLK_R) {
        renderer_step = demo_renderer_step;
      }
#endif

      if (event->key.key == SDLK_0) { load_level(0); }
      else if (event->key.key == SDLK_1) { load_level(1); }
      else if (event->key.key == SDLK_2) { load_level(2); }
      else if (event->key.key == SDLK_3) { load_level(3); }
      else if (event->key.key == SDLK_4) { load_level(4); }
      else if (event->key.key == SDLK_5) { load_level(5); }
    } else if (event->type == SDL_EVENT_KEY_UP) {
      if (event->key.key == SDLK_W || event->key.key == SDLK_S) { movement.forward = 0.f; }
      if (event->key.key == SDLK_A || event->key.key == SDLK_D) { movement.turn = 0.f; }
      if (event->key.key == SDLK_Q || event->key.key == SDLK_Z) { movement.raise = 0.f; }
      if (event->key.key == SDLK_E || event->key.key == SDLK_C) { movement.pitch = 0.f; }

    } else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
      printf("Resize buffer to %dx%d\n", event->window.data1 / scale, event->window.data2 / scale);
      renderer_resize(&rend, renderer_size_in_window(event->window.data1, event->window.data2));
      SDL_DestroyTexture(texture);
      texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, rend.buffer_size.x, rend.buffer_size.y);
      SDL_SetTextureScaleMode(texture, nearest?SDL_SCALEMODE_NEAREST:SDL_SCALEMODE_LINEAR);
      if (lock_aspect_ratio) {
        SDL_SetRenderLogicalPresentation(sdl_renderer, event->window.data2*aspect_ratio, event->window.data2, SDL_LOGICAL_PRESENTATION_LETTERBOX);
      }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void *userdata)
{
  static char debug_buffer[64];
  static float fps_update_timer = 0.5f;

  uint64_t now_ticks = SDL_GetTicks();
  delta_time = (now_ticks - last_ticks) / 1000.0f;  // in seconds
  last_ticks = now_ticks;

  if (fps_update_timer >= 0.25f) {
    sprintf(debug_buffer, "%dx%d @ %dx, dt: %f, FPS: %i", rend.buffer_size.x, rend.buffer_size.y, scale, delta_time, (unsigned int)(1/delta_time));
    fps_update_timer = 0.f;
  } else {
    fps_update_timer += delta_time;
  }

  if (dynamic_light) {
    /* Light moves up and down */
    light_set_position(dynamic_light, VEC3F(
      dynamic_light->entity.position.x,
      dynamic_light->entity.position.y,
      light_z + sin((now_ticks/30) * M_PI / 180.0) * light_movement_range
    ));

    /* Circles the light around the camera */
    /*light_set_position(dynamic_light, VEC3F(
      cam.position.x + cos((now_ticks/30) * M_PI / 180.0) * 16,
      cam.position.y + sin((now_ticks/30) * M_PI / 180.0) * 16,
      cam.entity.z + sin((now_ticks/30) * M_PI / 180.0) * 16
    ));*/
  }

  if (moving_sector.ref) {
    moving_sector.timer += delta_time;

    if (moving_sector.timer >= (1.f / 30)) {
      moving_sector.timer = 0.f;

      if (moving_sector.direction == 1) {
        if (moving_sector.ref->floor.height < moving_sector.ref->ceiling.height) {
          moving_sector.ref->floor.height += moving_sector.direction;
        }

        if (moving_sector.ref->ceiling.height > moving_sector.ref->floor.height) {
          moving_sector.ref->ceiling.height -= moving_sector.direction;
        }

        if (moving_sector.ref->floor.height == moving_sector.ref->ceiling.height) {
          moving_sector.direction = (moving_sector.direction == 1) ? -1 : 1;
        }
      } else {
        moving_sector.ref->floor.height += moving_sector.direction;
        moving_sector.ref->ceiling.height -= moving_sector.direction;
        moving_sector.distance ++;

        if (moving_sector.distance >= 200) {
          moving_sector.direction = 1;
          moving_sector.distance = 0;
        }
      }

      sector_update_floor_ceiling_limits(moving_sector.ref);
    }
  }

  process_camera_movement(delta_time);
  renderer_draw(&rend, &cam);

  SDL_UpdateTexture(texture, NULL, rend.buffer, rend.buffer_size.x*sizeof(pixel_type));

#ifdef RAYCASTER_DEBUG
  SDL_SetRenderDrawColor(sdl_renderer, 255, 0, 255, SDL_ALPHA_OPAQUE);
#else
  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
#endif

  SDL_RenderClear(sdl_renderer);
  SDL_RenderTexture(sdl_renderer, texture, NULL, NULL);

  if (info_text_visible) {
    int y = 4, h = 10;
    SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(sdl_renderer, 4, y, debug_buffer); y+=h;
    SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "[Camera] Pos: (%.1f, %.1f, %.1f) | Dir: (%.3f, %.3f) | Plane: (%.3f, %.3f) | FOV: %.2f",
      cam.entity.position.x, cam.entity.position.y, cam.entity.z,
      cam.entity.direction.x, cam.entity.direction.y,
      cam.plane.x, cam.plane.y,
      cam.fov); y+=h;
    SDL_RenderDebugTextFormat(sdl_renderer, 4, y, "[Sector] Ptr: 0x%p | Floor: %d | Ceiling: %d | Bright: %.2f",
      (void*)cam.entity.sector,
      cam.entity.sector->floor.height,
      cam.entity.sector->ceiling.height,
      cam.entity.sector->brightness); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[WASD] - Move & turn"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[Q Z] - Go up/down"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[E C] - Pitch up/down"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[M] - Toggle nearest/linear scaling"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[+ -] - Increase/decrease scale factor"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[O P] - Zoom out/in"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[Home End] - Raise/lower sector ceiling"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[PgUp PgDn] - Raise/lower sector floor"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[K L] - Change sector brightness"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[H] - Toggle on-screen info"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[F] - Toggle fullscreen"); y+=h;
    SDL_RenderDebugText(sdl_renderer, 4, y, "[0 ... 5] - Change level"); y+=h;
  }

  SDL_RenderPresent(sdl_renderer);

  return SDL_APP_CONTINUE;
}

static void process_camera_movement(const float delta_time)
{
  if ((int)movement.forward != 0) {
    camera_move(&cam, 400 * movement.forward * delta_time);
  }

  if ((int)movement.turn != 0) {
    camera_rotate(&cam, 2.f * movement.turn * delta_time);
  }

  if ((int)movement.raise != 0) {
    cam.entity.z += 88 * movement.raise * delta_time;
  }

  if ((int)movement.pitch != 0) {
    cam.pitch = math_clamp(cam.pitch+2*movement.pitch*delta_time, MIN_CAMERA_PITCH, MAX_CAMERA_PITCH);
  } else {
    cam.pitch *= 0.98f;
  }
}

static void create_grid_level()
{
  const int w = 24;
  const int h = 24;
  const int size = 256;

  register int x, y, c, f;

  map_builder builder = { 0 };

  srand(1311858591);

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      if (rand() % 20 == 5) {
        c = f = 0;
      } else {
        f = 8 * (rand() % 16);
        c = 1024 - 32 * (rand() % 24);
      }

      map_builder_add_polygon(&builder, f, c, 1.f, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
        VEC2F(x*size, y*size),
        VEC2F(x*size + size, y*size),
        VEC2F(x*size + size, y*size + size),
        VEC2F(x*size, y*size + size)
      ));
    }
  }

  demo_level = map_builder_build(&builder);

  // TODO: Vertices could be moved real-time but related linedefs need to be updated too
  /*for (x = 0; x < demo_level->vertices_count; ++x) {
    demo_level->vertices[x].point.x += (-24 + rand() % 48);
    demo_level->vertices[x].point.y += (-24 + rand() % 48);
  }*/
  
  map_builder_free(&builder);
}

static void create_demo_level()
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 144, 0.8f, WALLTEX(STONEWALL_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(0, 0),
    VEC2F(400, 0),
    VEC2F(400, 400),
    VEC2F(200, 300),
    VEC2F(0, 400)
  ));

  map_builder_add_polygon(&builder, -32, 176, 1.1f, WALLTEX(STONEWALL_TEXTURE), FLOOR_TEXTURE, TEXTURE_NONE, VERTICES(
    VEC2F(50, 50),
    VEC2F(50, 200),
    VEC2F(200, 200),
    VEC2F(200, 50)
  ));

  map_builder_add_polygon(&builder, 128, 128, 1.f, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(100, 100),
    VEC2F(125, 100),
    VEC2F(125, 125),
    VEC2F(100, 125)
  ));

  map_builder_add_polygon(&builder, 32, 128, 0.5f, WALLTEX(STONEWALL_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(0, 0),
    VEC2F(400, 0),
    VEC2F(300, -256),
    VEC2F(0, -128)
  ));

  map_builder_add_polygon(&builder, -128, 256, 0.15f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(400, 400),
    VEC2F(200, 300),
    VEC2F(100, 1000),
    VEC2F(500, 1000)
  ));

  map_builder_add_polygon(&builder, 0, 214, 0.15f, WALLTEX(METAL_STONE_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(260, 500),
    VEC2F(324, 500),
    VEC2F(324, 700),
    VEC2F(260, 700)
  ));

  demo_level = map_builder_build(&builder);
  demo_level->sky_texture = SKY_TEXTURE;

  moving_sector.ref = &demo_level->sectors[5];
  moving_sector.direction = rand() % 2 ? 1 : -1;
  moving_sector.ref->linedefs[0]->side[1].flags |= LINEDEF_PIN_BOTTOM_TEXTURE | LINEDEF_PIN_TOP_TEXTURE;
  moving_sector.ref->linedefs[1]->side[1].flags |= LINEDEF_PIN_BOTTOM_TEXTURE | LINEDEF_PIN_TOP_TEXTURE;
  moving_sector.ref->linedefs[2]->side[1].flags |= LINEDEF_PIN_BOTTOM_TEXTURE | LINEDEF_PIN_TOP_TEXTURE;
  moving_sector.ref->linedefs[3]->side[1].flags |= LINEDEF_PIN_BOTTOM_TEXTURE | LINEDEF_PIN_TOP_TEXTURE;

  level_data_find_linedef(demo_level, VEC2F(200, 300), VEC2F(100, 1000))->side[0].flags |= LINEDEF_MIRROR;
  level_data_find_linedef(demo_level, VEC2F(0, -128), VEC2F(300, -256))->side[0].flags |= LINEDEF_MIRROR;

  dynamic_light = level_data_add_light(demo_level, VEC3F(200, 600, 64), 300, 1.0f);
  light_z = dynamic_light->entity.z;
  light_movement_range = 48;

  /* Configure some transparent textures */
  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(0, 0), VEC2F(400, 0)),
    METAL_BARS
  );

  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(200, 300), VEC2F(100, 1000)),
    MIRROR_TEXTURE
  );

  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(0, -128), VEC2F(300, -256)),
    MIRROR_TEXTURE
  );
  
  map_builder_free(&builder);
}

static void create_big_one()
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 2048, 0.25f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(0, 0),
    VEC2F(6144, 0),
    VEC2F(6144, 6144),
    VEC2F(0, 6144)
  ));

  const int w = 20;
  const int h = 20;
  const int size = 256;

  register int x, y, c, f;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      if (rand() % 20 == 5) {
        c = f = 0;
      } else {
        f = 256 + 8 * (rand() % 16);
        c = 1440 - 32 * (rand() % 24);
      }

      map_builder_add_polygon(&builder, f, c, 0.5f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
        VEC2F(512+x*size,        512+y*size),
        VEC2F(512+x*size + size, 512+y*size),
        VEC2F(512+x*size + size, 512+y*size + size),
        VEC2F(512+x*size,        512+y*size + size)
      ));
    }
  }

  demo_level = map_builder_build(&builder);

  dynamic_light = level_data_add_light(demo_level, VEC3F(460, 460, 512), 1024, 1.0f);
  light_z = dynamic_light->entity.z;
  light_movement_range = 400;

  map_builder_free(&builder);
}

static void create_semi_intersecting_sectors()
{
  const float base_light = 0.25f;

  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, base_light, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(0, 0),
    VEC2F(500, 0),
    VEC2F(500, 500),
    VEC2F(0, 500)
  ));

  map_builder_add_polygon(&builder, 40, 86, base_light, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(0, 200),
    VEC2F(50, 200),
    VEC2F(50, 400),
    VEC2F(0, 400)
  ));

  map_builder_add_polygon(&builder, -20, 192, 0.35, WALLTEX(SMALL_BRICKS_TEXTURE), DIRT_TEXTURE, TEXTURE_NONE, VERTICES(
    VEC2F(250, 250),
    VEC2F(2000, 250),
    VEC2F(2000, 350),
    VEC2F(250, 350)
  ));

  map_builder_add_polygon(&builder, 0, 86, base_light, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, SMALL_BRICKS_TEXTURE, VERTICES(
    VEC2F(512, 350),
    VEC2F(640, 350),
    VEC2F(640, 364),
    VEC2F(512, 364)
  ));

  map_builder_add_polygon(&builder, 0, 128, base_light, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(512, 364),
    VEC2F(640, 364),
    VEC2F(640, 480),
    VEC2F(512, 480)
  ));

  map_builder_add_polygon(&builder, 56, 96, base_light, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(240, 240),
    VEC2F(260, 240),
    VEC2F(260, 260),
    VEC2F(240, 260)
  ));

  map_builder_add_polygon(&builder, 56, 88, base_light, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(240, 340),
    VEC2F(260, 340),
    VEC2F(260, 360),
    VEC2F(240, 360)
  ));

  map_builder_add_polygon(&builder, 56, 96, base_light, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(400, 350),
    VEC2F(420, 350),
    VEC2F(420, 370),
    VEC2F(400, 370)
  ));

  map_builder_add_polygon(&builder, 16, 96, base_light, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(400, 250),
    VEC2F(420, 250),
    VEC2F(420, 270),
    VEC2F(400, 270)
  ));

  map_builder_add_polygon(&builder, 20, 108, base_light, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(240, 250),
    VEC2F(250, 260),
    VEC2F(250, 350),
    VEC2F(240, 350)
  ));

  map_builder_add_polygon(&builder, -128, 256, base_light, WALLTEX(SMALL_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(-100, 500),
    VEC2F(100, 100),
    VEC2F(100, -100),
    VEC2F(-100, -100)
  ));

  demo_level = map_builder_build(&builder);
  demo_level->sky_texture = SKY_TEXTURE;

  dynamic_light = level_data_add_light(demo_level, VEC3F(300, 400, 64), 300, 1.0f);
  light_z = dynamic_light->entity.z;
  light_movement_range = 48;

  /* Configure some transparent textures */
  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(512, 364), VEC2F(640, 364)),
    METAL_GRATING
  );

  map_builder_free(&builder);
}

static void
create_crossing_and_splitting_sectors()
{
  map_builder builder = { 0 };

  map_builder_add_polygon(&builder, 0, 128, 0.1f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(-500, 0),
    VEC2F(1000, 0),
    VEC2F(1000, 100),
    VEC2F(-500, 100)
  ));

  /* This sector will split the first one so you end up with 3 sectors */
  map_builder_add_polygon(&builder, -32, 96, 0.1f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(225, -250),
    VEC2F(325, -250),
    VEC2F(325, 250),
    VEC2F(225, 250)
  ));

  demo_level = map_builder_build(&builder);

  dynamic_light = level_data_add_light(demo_level, VEC3F(250, 50, 50), 200, 0.5f);
  light_z = dynamic_light->entity.z;
  light_movement_range = 24;

  map_builder_free(&builder);
}

static void
create_mirrors_and_large_sky()
{
  map_builder builder = { 0 };

  /* First area */
  map_builder_add_polygon(&builder, 0, 256, 0.5f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(-500, -500),
    VEC2F(500, -500),
    VEC2F(500, 500),
    VEC2F(-500, 500)
  ));

  map_builder_add_polygon(&builder, 32, 512, 0.75f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(-100, -100),
    VEC2F(100, -100),
    VEC2F(100, 100),
    VEC2F(-100, 100)
  ));

  map_builder_add_polygon(&builder, 192, 256, 1.0f, WALLTEX(WOOD_TEXTURE), WOOD_TEXTURE, WOOD_TEXTURE, VERTICES(
    VEC2F(-10, -10),
    VEC2F(10, -10),
    VEC2F(10, 10),
    VEC2F(-10, 10)
  ));

  /* Second area */
  map_builder_add_polygon(&builder, 0, 256, 0.85f, WALLTEX(LARGE_BRICKS_TEXTURE), GRASS_TEXTURE, TEXTURE_NONE, VERTICES(
    VEC2F(1000, -500),
    VEC2F(2000, -500),
    VEC2F(2000, 500),
    VEC2F(1000, 500)
  ));

  /* Corridor between them */
  map_builder_add_polygon(&builder, 32, 128, 0.25f, WALLTEX(LARGE_BRICKS_TEXTURE), FLOOR_TEXTURE, CEILING_TEXTURE, VERTICES(
    VEC2F(500, -50),
    VEC2F(1000, -50),
    VEC2F(1000, 50),
    VEC2F(500, 50)
  ));

  demo_level = map_builder_build(&builder);
  demo_level->sky_texture = SKY_TEXTURE;

  level_data_find_linedef(demo_level, VEC2F(-500, -500), VEC2F(500, -500))->side[0].flags |= LINEDEF_MIRROR;
  level_data_find_linedef(demo_level, VEC2F(-500, 500), VEC2F(500, 500))->side[0].flags |= LINEDEF_MIRROR;
  level_data_find_linedef(demo_level, VEC2F(-500, -500), VEC2F(-500, 500))->side[0].flags |= LINEDEF_MIRROR;

  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(-500, -500), VEC2F(500, -500)),
    MIRROR_TEXTURE
  );

  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(-500, 500), VEC2F(500, 500)),
    MIRROR_TEXTURE
  );
  
  linedef_set_middle_texture(
    level_data_find_linedef(demo_level, VEC2F(-500, -500), VEC2F(-500, 500)),
    MIRROR_TEXTURE
  );

  dynamic_light = level_data_add_light(demo_level, VEC3F(-450, 400, 96), 250, 1.2f);
  light_z = dynamic_light->entity.z;
  light_movement_range = 88;

  map_builder_free(&builder);
}

static void
load_level(int n)
{
  if (demo_level) {
    free(demo_level);
  }

  dynamic_light = NULL;
  moving_sector.ref = NULL;
  moving_sector.timer = 0.f;
  moving_sector.distance = 0;

  switch (n) {
  case 1: create_demo_level(); break;
  case 2: create_big_one(); break;
  case 3: create_semi_intersecting_sectors(); break;
  case 4: create_crossing_and_splitting_sectors(); break;
  case 5: create_mirrors_and_large_sky(); break;
  default: create_grid_level(); break;
  }
  
  camera_init(&cam, demo_level);
}

/*
 * Texture sampler accepting world space texture coordinates
 * and expects texture to repeat.
 */
M_INLINED void
demo_texture_sampler_scaled(
  texture_ref texture,
  float fx,
  float fy,
  uint8_t mip_level,
  uint8_t *pixel,
  uint8_t *mask
) {
  M_UNUSED(mip_level);

  const SDL_Surface *surface = textures[texture];
  const int32_t x = (int32_t)floorf(fx) & (surface->w-1); // / mip_level) * mip_level;
  const int32_t y = (int32_t)floorf(fy) & (surface->h-1); // / mip_level) * mip_level;
  const Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * SDL_BYTESPERPIXEL(surface->format);
  
  if (pixel)
    memcpy(pixel, p, 3);

  /*
   * Only masked textures are supported for now, so any pixel
   * with a non-zero mask value will be drawn.
   */
  if (mask)
    *mask = p[3];
}

/*
 * Texture sampler accepting normalized texture coordinates
 * that should be clamped at the edges.
 */
M_INLINED void
demo_texture_sampler_normalized(
  texture_ref texture,
  float fx,
  float fy,
  uint8_t mip_level,
  uint8_t *pixel,
  uint8_t *mask
) {
  M_UNUSED(mip_level);

  const SDL_Surface *surface = textures[texture];
  int32_t x = (int32_t)(fx * (surface->w-1)); // / mip_level) * mip_level;
  int32_t y = (int32_t)(fy * (surface->h-1)); // / mip_level) * mip_level;
  const Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * SDL_BYTESPERPIXEL(surface->format);
  
  if (pixel)
    memcpy(pixel, p, 3);

  /*
   * Only masked textures are supported for now, so any pixel
   * with a non-zero mask value will be drawn.
   */
  if (mask)
    *mask = p[3];
}

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
static void
demo_renderer_step(const renderer *r)
{
  SDL_UpdateTexture(texture, NULL, r->buffer, r->buffer_size.x*sizeof(pixel_type));
  SDL_SetRenderDrawColor(sdl_renderer, 0, 128, 255, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(sdl_renderer);
  SDL_RenderTexture(sdl_renderer, texture, NULL, NULL);
  SDL_RenderPresent(sdl_renderer);
  SDL_Delay(4);
}
#endif
