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
    f32 dt = input->dt;

    GameState *gamestate = memory->perm.base;

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

    gamestate->time += dt;

    HM_V2 screen_center = hm_v2(framebuffer->width / 2.0f, framebuffer->height / 2.0f);

    /*gamestate->camera_pos = hm_v2(input->mouse.x, WINDOW_HEIGHT - input->mouse.y);*/

    hm_clear_texture(framebuffer, hm_v4(0.5f, 0.5f, 0.5f, 0));

    /*for (int i = 0; i < 10; ++i)*/
    {
        /*HM_V2 hero_pos_in_camera_space = hm_v2_sub(gamestate->hero_pos, gamestate->camera_pos);*/

        HM_Transform2 transform = hm_transform2_identity();
        /*transform = hm_transform2_rotate_by(transform, gamestate->time);*/
        /*transform = hm_transform2_scale_by(transform, hm_v2(2, 2));*/
        /*transform = hm_transform2_translate_by(transform, hm_v2(input->mouse.x, WINDOW_HEIGHT - input->mouse.y));*/
        /*transform = hm_transform2_translate_by(transform, hero_pos_in_camera_space);*/

        transform = hm_transform2_translate_by(transform, screen_center);
        HM_Sprite *frame = gamestate->hero_sprites.idles[gamestate->hero_direction];
        /*frame = gamestate->hero_walk_down_anim;*/
        hm_draw_sprite(framebuffer, transform, frame, hm_v4(1, 1, 1, 1));
        /*hm_draw_bitmap(framebuffer, transform, &gamestate->hero_walk_down_texture, hm_v4(1, 1, 1, 1));*/

    }

#if 0
    {
        HM_Transform2 transform = hm_transform2_identity();
        transform = hm_transform2_translate_by(transform, hm_v2(-50, -50));
        transform = hm_transform2_rotate_by(transform, gamestate->time);
        transform = hm_transform2_translate_by(transform, hm_v2(300, 300));

        hm_draw_bbox2(framebuffer, transform, hm_bbox2(hm_v2(0, 0), hm_v2(100, 100)), hm_v4(1, 0, 0, 1));
    }
#endif
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
