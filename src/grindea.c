#include "hammer/hammer.h"

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

#if 0
typedef enum {
    SpriteAnimLoop_None,
    SpriteAnimLoop_Loop,
} SpriteAnimLoop;

#define MAX_SPRITE_ANIM_FRAME_COUNT 16
typedef struct {
    HM_Sprite frames[MAX_SPRITE_ANIM_FRAME_COUNT];
    u32 frame_count;
    f32 current_frame;

    f32 fps;

    SpriteAnimLoop loop;
    u32 loop_count;
} SpriteAnim;
#endif

typedef enum {
    Direction_Up = 0,
    Direction_Down,
    Direction_Left,
    Direction_Right,
    Direction_Count,
} Direction;

typedef struct {
    HM_Texture2 *idle_texture;

    HM_Sprite *idles[Direction_Count];
} HeroSprites;

typedef struct {
    // Center point
    HM_V2 pos;
    HM_V2 size;
} Camera;

static Camera
camera_pos_size(HM_V2 pos, HM_V2 size) {
    Camera result = { pos, size };

    return result;
}

typedef struct {
} World;

static HM_Transform2
pixel_space_to_world_space(f32 pixels_to_meters) {
    HM_Transform2 result = hm_transform2_scale(hm_v2(pixels_to_meters, pixels_to_meters));

    return result;
}

static HM_Transform2
world_space_to_camera_space(Camera *camera) {
    HM_Transform2 result = hm_transform2_translation(hm_v2_neg(camera->pos));

    return result;
}

static HM_Transform2
camera_space_to_screen_space(Camera *camera, i32 minx, i32 maxx, i32 miny, i32 maxy) {
    HM_V2 inv_size = hm_v2(1.0f / camera->size.width,
                           1.0f / camera->size.height);
    HM_Transform2 result = hm_transform2_scale(inv_size);
    result = hm_transform2_translate_by(hm_v2(0.5f, 0.5f), result);
    result = hm_transform2_scale_by(hm_v2(maxx - minx, maxy - miny), result);
    result = hm_transform2_translate_by(hm_v2(minx, miny), result);

    return result;
}

static HeroSprites
load_hero_sprites(HM_GameMemory *memory) {
    HeroSprites result;

    result.idle_texture = hm_load_bitmap(&memory->perm, "assets/sprites/hero/idle.bmp");

    int sprite_width = 64;
    int sprite_height = 96;
    for (int i = 0; i < 4; ++i) {
        result.idles[i] = hm_sprite_from_texture(&memory->perm,
                                                 result.idle_texture,
                                                 hm_bbox2_min_size(hm_v2(i * sprite_width, 0), hm_v2(sprite_width, sprite_height)),
                                                 hm_v2(32, 22));
    }

    return result;
}

typedef struct {
    f32 time;

    HeroSprites hero_sprites;
    Direction hero_direction;

    HM_V2 camera_pos;
    HM_V2 hero_pos;
} GameState;

static void
init_game_state(HM_GameMemory *memory, GameState *gamestate) {
    gamestate->hero_sprites = load_hero_sprites(memory);
    gamestate->camera_pos = hm_v2(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);
    gamestate->hero_pos = hm_v2(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);
}

static HM_INIT_GAME(init_game) {
    GameState *gamestate = HM_PUSH_STRUCT(&memory->perm, GameState);
    init_game_state(memory, gamestate);
}

static HM_UPDATE_AND_RENDER(update_and_render) {
    HM_DEBUG_BEGIN_BLOCK("update_and_render");

    GameState *gamestate = memory->perm.base;
    f32 dt = input->dt;

    gamestate->time += dt;

    f32 meters_to_pixels = 48.0f;
    f32 pixels_to_meters = 1.0f / meters_to_pixels;

    f32 aspect_ratio = (f32)framebuffer->width / (f32)framebuffer->height;
    f32 camera_height = 12.0;
    Camera camera = camera_pos_size(hm_v2(0, 0), hm_v2(aspect_ratio * camera_height, camera_height));

    if (input->keyboard.keys[HM_Key_W].is_down) {
        gamestate->hero_direction = Direction_Up;
    }

    if (input->keyboard.keys[HM_Key_S].is_down) {
        gamestate->hero_direction = Direction_Down;
    }

    if (input->keyboard.keys[HM_Key_A].is_down) {
        gamestate->hero_direction = Direction_Left;
    }

    if (input->keyboard.keys[HM_Key_D].is_down) {
        gamestate->hero_direction = Direction_Right;
    }

    //
    // Render
    //
    HM_Transform2 world_to_screen_transform =
        hm_transform2_dot(camera_space_to_screen_space(&camera, 0, framebuffer->width, 0, framebuffer->height),
                          world_space_to_camera_space(&camera));

    hm_clear_texture(framebuffer, hm_v4(0.5f, 0.5f, 0.5f, 0));

    {
        HM_Transform2 transform = pixel_space_to_world_space(pixels_to_meters);
        transform = hm_transform2_translate_by(hm_v2(1, 1), transform);
        transform = hm_transform2_dot(world_to_screen_transform, transform);
        hm_draw_sprite(framebuffer, transform, gamestate->hero_sprites.idles[gamestate->hero_direction], hm_v4(1, 1, 1, 1));
    }

    HM_DEBUG_END_BLOCK("update_and_render");
}

int main(void) {
    HM_Config config = {};
    config.window.title = "Grindea";
    config.window.width = WINDOW_WIDTH;
    config.window.height = WINDOW_HEIGHT;
    config.memory.size.perm = HM_MEMORY_SIZE_MB(64);
    config.memory.size.tran = HM_MEMORY_SIZE_MB(128);
    config.debug.is_exit_on_esc = true;
    config.callback.init_game = init_game;
    config.callback.update_and_render = update_and_render;

    hm_run(&config);
}
