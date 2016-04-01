#include "hammer/hammer.h"

#include "camera.c"

#define WINDOW_WIDTH 967
#define WINDOW_HEIGHT 547
#define METERS_TO_PIXELS 48.0f
#define PIXELS_TO_METERS (1.0f / METERS_TO_PIXELS)
#define HERO_SPEED 150
#define ENTITY_DRAG 20

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
    EntityType_None,
    EntityType_Hero,
} EntityType;

typedef struct {
    EntityType type;

    HM_V2 pos;

    HM_V2 vel;
    HM_V2 acc;
} Entity;

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
    HM_Sprite *sprite;

    i32 x;
    i32 y;
} GroundChunk;

#define MAX_GROUND_CHUNK_COUNT 32
#define MAX_ENTITY_COUNT 1024
typedef struct {
    u32 ground_chunk_count;
    GroundChunk ground_chunks[MAX_GROUND_CHUNK_COUNT];

    HM_V2 ground_chunk_size;

    u32 entity_count;
    Entity entities[MAX_ENTITY_COUNT];

    Entity *hero;
} World;

static Entity *
add_entity(World *world, EntityType type) {
    HM_ASSERT(world->entity_count < HM_ARRAY_COUNT(world->entities));

    Entity *result = world->entities + world->entity_count++;
    result->type = type;

    return result;
}

static Entity *
add_hero(World *world, HM_V2 pos) {
    Entity *hero = add_entity(world, EntityType_Hero);

    hero->pos = pos;

    return hero;
}

static HeroSprites
load_hero_sprites(HM_Memory *memory) {
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

    HM_RenderCommandBuffer *buffer;

    HM_Texture2 *test_texture;

    HM_Texture2 *background;

    HeroSprites hero_sprites;
    Direction hero_direction;

    HM_V2 camera_pos;
    HM_V2 hero_pos;

    Camera camera;
    HM_BBox2 camera_bounds;

    World world;

    u32 loaded_ground_chunk_count;
    GroundChunk *loaded_ground_chunks;
} GameState;

static HM_INIT_GAME(init_game) {
    HM_Memory *memory = hammer->memory;

    GameState *gamestate = HM_PUSH_STRUCT(&memory->perm, GameState);

    gamestate->buffer = hm_make_render_command_buffer(&memory->tran,
                                                      HM_MEMORY_SIZE_MB(1));

    gamestate->test_texture = hm_load_bitmap(&memory->tran, "assets/test.bmp");

    gamestate->background = hm_load_bitmap(&memory->tran, "assets/scene1.bmp");

    gamestate->hero_sprites = load_hero_sprites(memory);

    gamestate->hero_pos = hm_v2_zero();

    f32 aspect_ratio = (f32)hammer->framebuffer->width /
                       (f32)hammer->framebuffer->height;
    f32 camera_height = hammer->framebuffer->height * PIXELS_TO_METERS;
    HM_V2 camera_size = hm_v2(aspect_ratio * camera_height, camera_height);
    gamestate->camera = camera_pos_size(hm_v2_zero(), camera_size);

    gamestate->camera_bounds = hm_bbox2_min_size(
        hm_v2_zero(),
        hm_v2(gamestate->background->width * PIXELS_TO_METERS,
             gamestate->background->height * PIXELS_TO_METERS)
    );

    // Manually load ground chunks
    {
        i32 ground_width_in_pixels = gamestate->background->width;
        i32 ground_height_in_pixels = gamestate->background->height;
        i32 ground_chunk_count_x = 6;
        i32 ground_chunk_count_y = 4;

        f32 ground_chunk_width_in_pixels = (f32)ground_width_in_pixels /
                                           (f32)ground_chunk_count_x;
        f32 ground_chunk_height_in_pixels = (f32)ground_height_in_pixels /
                                            (f32)ground_chunk_count_y;

        gamestate->world.ground_chunk_size =
            hm_v2(ground_chunk_width_in_pixels * PIXELS_TO_METERS,
                  ground_chunk_height_in_pixels * PIXELS_TO_METERS);

        gamestate->loaded_ground_chunk_count = ground_chunk_count_x *
                                               ground_chunk_count_y;
        gamestate->loaded_ground_chunks =
            HM_PUSH_ARRAY(&memory->tran, GroundChunk,
                          gamestate->loaded_ground_chunk_count);

        for (i32 y = 0; y < ground_chunk_count_y; ++y) {
            for (i32 x = 0; x < ground_chunk_count_x; ++x) {
                i32 ground_chunk_index = y * ground_chunk_count_x + x;
                GroundChunk *ground_chunk = gamestate->loaded_ground_chunks +
                                            ground_chunk_index;

                ground_chunk->x = x;
                ground_chunk->y = y;
                ground_chunk->sprite = hm_sprite_from_texture(
                    &memory->tran, gamestate->background,
                    hm_bbox2_min_size(hm_v2(x * ground_chunk_width_in_pixels,
                                            y * ground_chunk_height_in_pixels),
                                      hm_v2(ground_chunk_width_in_pixels,
                                            ground_chunk_height_in_pixels)),
                    hm_v2_zero()
                );
            }
        }
    }

    gamestate->world.hero = add_hero(&gamestate->world, hm_v2_zero());
}

static void
move_entity(Entity *entity, f32 dt) {
    HM_V2 drag = hm_v2_mul(ENTITY_DRAG, hm_v2_neg(entity->vel));

    HM_V2 acc = hm_v2_add(drag, entity->acc);

    // entity->vel * dt + 0.5f * acc * dt * dt;
    HM_V2 movement = hm_v2_add(hm_v2_mul(dt, entity->vel),
                               hm_v2_mul(0.5f * dt * dt, acc));

    entity->vel = hm_v2_add(entity->vel, hm_v2_mul(dt, acc));
    if (hm_get_v2_len_sq(entity->vel) < 0.01f) {
        entity->vel = hm_v2_zero();
    }

    entity->pos = hm_v2_add(entity->pos, movement);
}

static HM_UPDATE_AND_RENDER(update_and_render) {
    HM_DEBUG_BEGIN_BLOCK("update_and_render");

    HM_Memory *memory = hammer->memory;
    HM_Input *input = hammer->input;
    HM_Texture2 *framebuffer = hammer->framebuffer;

    GameState *gamestate = (GameState *)memory->perm.base;
    f32 dt = input->dt;

    gamestate->time += dt;

    {
        HM_V2 acc = hm_v2_zero();
        if (input->keyboard.keys[HM_Key_W].is_down) {
            gamestate->hero_direction = Direction_Up;
            acc.y = 1.0f;
        }

        if (input->keyboard.keys[HM_Key_S].is_down) {
            gamestate->hero_direction = Direction_Down;
            acc.y = -1.0f;
        }

        if (input->keyboard.keys[HM_Key_A].is_down) {
            gamestate->hero_direction = Direction_Left;
            acc.x = -1.0f;
        }

        if (input->keyboard.keys[HM_Key_D].is_down) {
            gamestate->hero_direction = Direction_Right;
            acc.x = 1.0f;
        }

        acc = hm_v2_normalize(acc);
        acc = hm_v2_mul(HERO_SPEED, acc);

        gamestate->world.hero->acc = acc;
    }

    move_entity(gamestate->world.hero, dt);

    f32 aspect_ratio = (f32)framebuffer->width / (f32)framebuffer->height;
    if (input->keyboard.keys[HM_Key_UP].is_down) {
        gamestate->camera.size.h *= 1.1f;
        gamestate->camera.size.w = aspect_ratio * gamestate->camera.size.h;
    }

    if (input->keyboard.keys[HM_Key_DOWN].is_down) {
        gamestate->camera.size.h *= 0.9f;
        gamestate->camera.size.w = aspect_ratio * gamestate->camera.size.h;
    }

    // Update camera position based on hero
    gamestate->camera.pos = gamestate->world.hero->pos;

    // Limit camera in bounds
    {
        HM_BBox2 camera_bbox = hm_bbox2_cen_size(gamestate->camera.pos,
                                                 gamestate->camera.size);

        if (camera_bbox.min.x < gamestate->camera_bounds.min.x) {
            camera_bbox.min.x = gamestate->camera_bounds.min.x;
        }

        if (camera_bbox.min.y < gamestate->camera_bounds.min.y) {
            camera_bbox.min.y = gamestate->camera_bounds.min.y;
        }

        camera_bbox = hm_bbox2_min_size(camera_bbox.min, gamestate->camera.size);

        if (camera_bbox.max.x > gamestate->camera_bounds.max.x) {
            camera_bbox.max.x = gamestate->camera_bounds.max.x;
        }

        if (camera_bbox.max.y > gamestate->camera_bounds.max.y) {
            camera_bbox.max.y = gamestate->camera_bounds.max.y;
        }

        camera_bbox = hm_bbox2_max_size(camera_bbox.max, gamestate->camera.size);

        gamestate->camera.pos = hm_get_bbox2_cen(camera_bbox);
    }

    // update active ground chunks
    {
        World *world = &gamestate->world;

        HM_BBox2 camera_bbox = hm_bbox2_cen_size(gamestate->camera.pos,
                                                 gamestate->camera.size);
        i32 min_x = hm_f32_floor(camera_bbox.min.x /
                                 gamestate->world.ground_chunk_size.w);
        i32 min_y = hm_f32_floor(camera_bbox.min.y /
                                 gamestate->world.ground_chunk_size.h);
        i32 max_x = hm_f32_ceil(camera_bbox.max.x /
                                 gamestate->world.ground_chunk_size.w);
        i32 max_y = hm_f32_ceil(camera_bbox.max.y /
                                 gamestate->world.ground_chunk_size.h);

        world->ground_chunk_count = 0;
        for (i32 y = min_y; y <= max_y; ++y) {
            for (i32 x = min_x; x <= max_x; ++x) {
                if (world->ground_chunk_count < HM_ARRAY_COUNT(world->ground_chunks)) {
                    GroundChunk *ground_chunk = world->ground_chunks +
                                                world->ground_chunk_count;

                    GroundChunk *found_ground_chunk = 0;

                    // Find loaded ground chunk at (x, y)
                    for (u32 loaded_ground_chunk_index = 0;
                         loaded_ground_chunk_index <
                         gamestate->loaded_ground_chunk_count;
                         ++loaded_ground_chunk_index)
                    {
                        GroundChunk *loaded_ground_chunk =
                            gamestate->loaded_ground_chunks + loaded_ground_chunk_index;

                        if (loaded_ground_chunk->x == x &&
                            loaded_ground_chunk->y == y)
                        {
                            found_ground_chunk = loaded_ground_chunk;
                            break;
                        }
                    }

                    if (found_ground_chunk) {
                        *ground_chunk = *found_ground_chunk;
                        ++world->ground_chunk_count;
                    }
                }
            }
        }
    }

    //
    // Render
    //
    //hm_clear_texture(framebuffer, hm_v4(0.5f, 0.5f, 0.5f, 0));

    HM_Transform2 world_to_screen_transform = hm_transform2_dot(
        camera_space_to_screen_space(&gamestate->camera, 0, framebuffer->width, 0, framebuffer->height),
        world_space_to_camera_space(&gamestate->camera)
    );

    // Render ground chunk
    {
        World *world = &gamestate->world;

        for (u32 ground_chunk_index = 0;
             ground_chunk_index < world->ground_chunk_count;
             ++ground_chunk_index)
        {
            GroundChunk *ground_chunk = world->ground_chunks + ground_chunk_index;
            HM_V2 pos = hm_v2(ground_chunk->x * world->ground_chunk_size.w,
                              ground_chunk->y * world->ground_chunk_size.h);

            HM_Transform2 transform = pixel_space_to_world_space(PIXELS_TO_METERS);
            transform = hm_transform2_translate_by(pos, transform);
            transform = hm_transform2_dot(world_to_screen_transform, transform);
            hm_render_sprite(gamestate->buffer, transform,
                             ground_chunk->sprite, hm_v4(1, 1, 1, 1));

            HM_BBox2 bbox = hm_bbox2_min_size(
                hm_v2_zero(),
                hm_get_bbox2_size(ground_chunk->sprite->bbox)
            );

#if 1
            // Render ground chunk outline
            {
                HM_Transform2 inv_transform = hm_transform2_invert(transform);
                f32 thickness = 2.0f * hm_get_transform2_scale(inv_transform).x;
                hm_render_bbox2_outline(gamestate->buffer, transform,
                                        bbox, thickness, hm_v4(1, 1, 1, 1));
            }
#endif
        }
    }

    {
        HM_Transform2 transform = pixel_space_to_world_space(PIXELS_TO_METERS);
        /*transform = hm_transform2_rotate_by(gamestate->time, transform);*/
        transform = hm_transform2_translate_by(gamestate->world.hero->pos, transform);
        transform = hm_transform2_dot(world_to_screen_transform, transform);
        hm_render_sprite(gamestate->buffer, transform, gamestate->hero_sprites.idles[gamestate->hero_direction], hm_v4(1, 1, 1, 1));
    }

    hm_present_render_commands(gamestate->buffer, framebuffer);

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
