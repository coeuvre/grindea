typedef struct Vertex Vertex;
struct Vertex {
    f32 x, y;

    Vertex *prev;
    Vertex *next;
};

typedef struct {
    HM_MemoryArena arena;
    Vertex *first_free_vertex;
} PolygonPool;

#define VERTEX_REGION_SIZE 8

typedef struct {
    Vertex *first;
    Vertex *last;

    Vertex *selected;
    bool is_dragging;
} EditingPolygon;

static PolygonPool *
make_polygon_pool(HM_MemoryArena *arena, usize size) {
    PolygonPool *result = HM_PUSH_STRUCT(arena, PolygonPool);

    result->arena = HM_SUB_MEMORY_ARENA(arena, size);
    result->first_free_vertex = 0;

    return result;
}

static EditingPolygon *
make_polygon(HM_MemoryArena *arena) {
    EditingPolygon *result = HM_PUSH_STRUCT(arena, EditingPolygon);

    result->first = 0;
    result->last = 0;

    return result;
}

static void
push_vertex(PolygonPool *pool, EditingPolygon *polygon, f32 x, f32 y) {
    Vertex *vertex = HM_PUSH_STRUCT(&pool->arena, Vertex);

    vertex->x = x;
    vertex->y = y;
    vertex->next = 0;

    vertex->prev = polygon->last;

    if (polygon->last) {
        polygon->last->next = vertex;
    }

    polygon->last = vertex;

    if (!polygon->first) {
        polygon->first = vertex;
    }
}

static void
update_polygon(EditingPolygon *polygon, PolygonPool *pool, Hammer *hammer) {
    (void)pool;

    HM_Input *input = hammer->input;
    HM_Texture2 *framebuffer = hammer->framebuffer;

    HM_V2 mouse_pos = hm_v2(input->mouse.x, framebuffer->height - input->mouse.y);

    if (!input->mouse.left.is_down) {
        polygon->is_dragging = false;
    }

    if (polygon->is_dragging) {
        polygon->selected->x = mouse_pos.x;
        polygon->selected->y = mouse_pos.y;
    } else {
        // Check if the mouse have been moved
        if (input->mouse.is_moved) {
            f32 min_distance = HM_F32_MAX;
            Vertex *min_vertex = 0;
            for (Vertex *a = polygon->first; a; a = a->next) {
                HM_V2 ab = hm_v2_sub(mouse_pos, hm_v2(a->x, a->y));

                f32 distance = hm_get_v2_len_sq(ab);
                if (distance < min_distance) {
                    min_distance = distance;
                    min_vertex = a;
                }
            }

            polygon->selected = min_vertex;
        }

        if (polygon->selected && input->mouse.left.is_pressed) {
            HM_V2 pos = hm_v2(polygon->selected->x, polygon->selected->y);
            HM_BBox2 bbox = hm_bbox2_cen_size(pos, hm_v2(VERTEX_REGION_SIZE,
                                                         VERTEX_REGION_SIZE));
            if (hm_is_bbox2_contains_point(bbox, mouse_pos)) {
                polygon->is_dragging = true;
            }
        }
    }
}

static void
render_polygon(EditingPolygon *polygon, HM_RenderCommandBuffer *buffer) {
    for (Vertex *a = polygon->first; a; a = a->next) {
        Vertex *b = a->next;
        if (b == 0) {
            b = polygon->first;
        }

        HM_V4 color = hm_v4(1.0f, 1.0f, 1.0f, 1.0f);

        if (a == polygon->selected || b == polygon->selected) {
            color = hm_v4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        hm_render_line2(buffer, hm_transform2_identity(),
                        hm_line2(hm_v2(a->x, a->y), hm_v2(b->x, b->y)),
                        2.0f, color);
    }

    if (polygon->selected) {
        HM_V2 pos = hm_v2(polygon->selected->x, polygon->selected->y);

        hm_render_bbox2(buffer, hm_transform2_identity(),
                        hm_bbox2_cen_size(pos, hm_v2(VERTEX_REGION_SIZE,
                                                     VERTEX_REGION_SIZE)),
                        hm_v4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}
