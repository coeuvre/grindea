#include "hammer/hammer.h"

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

typedef struct {
    f32 time;

    HM_Texture2 hero_idle_down_texture;
    HM_Texture2 hero_walk_down_texture;

    HM_Sprite hero_idle_down;
    HM_Sprite hero_walk_down_anim[8];

    HM_V2 camera_pos;
    HM_V2 hero_pos;
} GameState;

static void
init_game_state(HM_GameMemory *memory, GameState *gamestate) {
    gamestate->hero_idle_down_texture = hm_load_bitmap(&memory->perm, "assets/hero/idle_down.bmp");
    gamestate->hero_walk_down_texture = hm_load_bitmap(&memory->perm, "assets/hero/walk_down.bmp");

    gamestate->hero_idle_down = hm_sprite_from_texture(&gamestate->hero_idle_down_texture,
                                                       hm_bbox2_zero(),
                                                       hm_v2(24, 10));

    HM_ARRAY_FOR(gamestate->hero_walk_down_anim, i) {
        HM_Sprite *sprite = gamestate->hero_walk_down_anim + i;
        *sprite = hm_sprite_from_texture(&gamestate->hero_walk_down_texture,
                                         hm_bbox2_min_size(hm_v2(48 * i, 0), hm_v2(48, 64)),
                                         hm_v2(24, 10));
    }

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

    gamestate->time += dt;

    HM_V2 screen_center = hm_v2(framebuffer->width / 2.0f, framebuffer->height / 2.0f);

    /*gamestate->camera_pos = hm_v2(input->mouse.x, WINDOW_HEIGHT - input->mouse.y);*/

    hm_clear_texture(framebuffer, hm_v4(0.5f, 0.5f, 0.5f, 0));

    /*for (int i = 0; i < 10; ++i)*/
    {
        /*HM_V2 hero_pos_in_camera_space = hm_v2_sub(gamestate->hero_pos, gamestate->camera_pos);*/

        HM_Transform2 transform = hm_transform2_identity();
        transform = hm_transform2_rotate_by(transform, gamestate->time);
        transform = hm_transform2_scale_by(transform, hm_v2(2, 2));
        /*transform = hm_transform2_translate_by(transform, hm_v2(input->mouse.x, WINDOW_HEIGHT - input->mouse.y));*/
        /*transform = hm_transform2_translate_by(transform, hero_pos_in_camera_space);*/

        transform = hm_transform2_translate_by(transform, screen_center);
        HM_Sprite *frame = gamestate->hero_walk_down_anim + ((u32)(15 * gamestate->time) % HM_ARRAY_COUNT(gamestate->hero_walk_down_anim));
        /*frame = gamestate->hero_walk_down_anim;*/
        hm_draw_sprite(framebuffer, transform, frame, hm_v4(1, 1, 1, 1));
        /*hm_draw_bitmap(framebuffer, transform, &gamestate->hero_walk_down_texture, hm_v4(1, 1, 1, 1));*/

    }

    {
        HM_Transform2 transform = hm_transform2_identity();
        transform = hm_transform2_translate_by(transform, hm_v2(-50, -50));
        transform = hm_transform2_rotate_by(transform, gamestate->time);
        transform = hm_transform2_translate_by(transform, hm_v2(300, 300));

        hm_draw_bbox2(framebuffer, transform, hm_bbox2(hm_v2(0, 0), hm_v2(100, 100)), hm_v4(1, 0, 0, 1));
    }
}

int main(void) {
    HM_Config config = {};
    config.window.title = "Grindea";
    config.window.width = WINDOW_WIDTH;
    config.window.height = WINDOW_HEIGHT;
    config.memory.size.perm = HM_MB(64);
    config.memory.size.tran = HM_MB(128);
    config.debug.is_exit_on_esc = true;
    config.callback.init_game = init_game;
    config.callback.update_and_render = update_and_render;

    hm_run(&config);
}
