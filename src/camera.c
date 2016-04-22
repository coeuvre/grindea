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

static HM_Trans2
pixel_space_to_world_space(f32 pixels_to_meters) {
    HM_Trans2 result = hm_trans2_scale(hm_v2(pixels_to_meters, pixels_to_meters));

    return result;
}

static HM_Trans2
world_space_to_camera_space(Camera *camera) {
    HM_Trans2 result = hm_trans2_translation(hm_v2_neg(camera->pos));

    return result;
}

static HM_Trans2
camera_space_to_screen_space(Camera *camera, i32 minx, i32 maxx, i32 miny, i32 maxy) {
    HM_V2 inv_size = hm_v2(1.0f / camera->size.w,
                           1.0f / camera->size.h);
    HM_Trans2 result = hm_trans2_scale(inv_size);
    result = hm_trans2_translate_by(hm_v2(0.5f, 0.5f), result);
    result = hm_trans2_scale_by(hm_v2(maxx - minx, maxy - miny), result);
    result = hm_trans2_translate_by(hm_v2(minx, miny), result);

    return result;
}

