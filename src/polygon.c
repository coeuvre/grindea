typedef struct Vertex Vertex;
struct Vertex {
    HM_V2 pos;

    Vertex *prev;
    Vertex *next;
};

typedef struct {
    HM_MemoryArena arena;
    Vertex *first_free_vertex;
} PolygonPool;

#define VERTEX_DRAG_REGION_SIZE 8
#define VERTEX_THRESHOLD 16

typedef struct {
    u32 vertex_count;
    Vertex *first;
    Vertex *last;

    Vertex *selected;
    HM_V2 drag_pos;
    bool is_dragging;
} EditingPolygon;

static PolygonPool *
make_polygon_pool(HM_MemoryArena *arena, usize size) {
    PolygonPool *result = HM_PUSH_STRUCT(arena, PolygonPool);
    HM_CLEAR_MEMORY(result);

    result->arena = HM_SUB_MEMORY_ARENA(arena, size);

    return result;
}

static EditingPolygon *
make_polygon(HM_MemoryArena *arena) {
    EditingPolygon *result = HM_PUSH_STRUCT(arena, EditingPolygon);
    HM_CLEAR_MEMORY(result);

    return result;
}

static Vertex *
push_vertex_after(PolygonPool *pool, EditingPolygon *polygon, Vertex *vertex, HM_V2 pos) {
    Vertex *result = HM_PUSH_STRUCT(&pool->arena, Vertex);
    result->pos = pos;

    result->next = vertex->next;
    result->next->prev = result;

    result->prev = vertex;
    result->prev->next = result;

    ++polygon->vertex_count;

    return result;
}


static void
push_vertex(PolygonPool *pool, EditingPolygon *polygon, HM_V2 pos) {
    if (polygon->last) {
        push_vertex_after(pool, polygon, polygon->last, pos);
    } else {
        // The first one
        Vertex *vertex = HM_PUSH_STRUCT(&pool->arena, Vertex);

        vertex->pos = pos;

        polygon->first = vertex;
        polygon->last = vertex;

        vertex->next = polygon->first;
        vertex->prev = polygon->last;

        ++polygon->vertex_count;
    }
}

static bool
is_diagonal(EditingPolygon *polygon, Vertex *s1, Vertex *s2) {
    HM_Line2 test = hm_line2(s1->pos, s2->pos);
    Vertex *a = polygon->first;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        Vertex *b = a->next;

        if (a != s1 && a != s2 && b != s1 && b != s2 &&
            hm_is_line2_intersect(hm_line2(a->pos, b->pos), test))
        {
            return false;
        }

        a = a->next;
    }

    return true;
}


static void
update_polygon(EditingPolygon *polygon, PolygonPool *pool, Hammer *hammer) {
    HM_Input *input = hammer->input;
    HM_Texture2 *framebuffer = hammer->framebuffer;

    HM_V2 mouse_pos = hm_v2(input->mouse.x, framebuffer->height - input->mouse.y);

    if (!input->mouse.left.is_down) {
        polygon->is_dragging = false;
    }

    if (polygon->is_dragging) {
        polygon->drag_pos = mouse_pos;
        polygon->selected->pos = polygon->drag_pos;
    } else {
        // Check if the mouse have been moved
        if (input->mouse.is_moved) {
            // Find closest vertex
            f32 min_distance = HM_F32_MAX;
            Vertex *min_vertex = 0;
            Vertex *a = polygon->first;
            Vertex *b = a->next;
            for (u32 i = 0; i < polygon->vertex_count; ++i) {
                HM_V2 ac = hm_v2_sub(mouse_pos, a->pos);
                f32 distance = hm_get_v2_len(ac);

                HM_Line2 line = hm_line2(a->pos, b->pos);
                f32 proj = hm_get_line2_proj_p(line, mouse_pos);
                if (proj >= 0.0f && proj < 1.0f) {
                    distance = HM_MIN(hm_get_line2_distance(line, mouse_pos), distance);
                }

                if (distance < min_distance) {
                    min_distance = distance;
                    min_vertex = a;
                }

                a = a->next;
                b = b->next;
            }

            // Move drag point along edge
            if (min_vertex) {
                polygon->selected = min_vertex;
                polygon->drag_pos = polygon->selected->pos;

                Vertex *a = polygon->selected->prev;
                Vertex *b = polygon->selected;

                f32 min_distance = HM_F32_MAX;

                for (int i = 0; i < 2; ++i) {
                    f32 distance = hm_get_line2_distance(hm_line2(a->pos, b->pos),
                                                         mouse_pos);
                    if (distance < min_distance) {
                        min_distance = distance;

                        f32 proj = hm_get_line2_proj_p(hm_line2(a->pos, b->pos), mouse_pos);
                        if (proj >= 0.0f && proj < 1.0f) {
                            polygon->drag_pos = hm_v2_add(
                                a->pos,
                                hm_v2_mul(proj, hm_v2_sub(b->pos, a->pos))
                            );
                        }
                    }

                    a = a->next;
                    b = b->next;
                }
            }

            // Snap drag point to vertex
            a = polygon->first;
            for (u32 i = 0; i < polygon->vertex_count; ++i) {
                f32 distance = hm_get_v2_len(hm_v2_sub(polygon->drag_pos, a->pos));
                if (distance < VERTEX_THRESHOLD) {
                    polygon->selected = a;
                    polygon->drag_pos = a->pos;
                    break;
                }
                a = a->next;
            }
        }

        if (polygon->selected && input->mouse.left.is_pressed) {
            HM_BBox2 bbox = hm_bbox2_cen_size(
                polygon->drag_pos,
                hm_v2(VERTEX_DRAG_REGION_SIZE, VERTEX_DRAG_REGION_SIZE)
            );

            if (hm_is_bbox2_contains_point(bbox, mouse_pos)) {
                // Add new vertex at drag point
                if (!hm_is_v2_equal(polygon->selected->pos, polygon->drag_pos)) {
                    polygon->selected = push_vertex_after(pool, polygon,
                                                          polygon->selected,
                                                          polygon->drag_pos);
                }

                polygon->is_dragging = true;
            }
        }
    }
}

static void
render_polygon(EditingPolygon *polygon, HM_RenderContext *context) {
    hm_render_push(context);

    hm_set_render_trans2(context, hm_trans2_identity());

    HM_V4 selected_color = hm_v4(0.0f, 1.0f, 0.0f, 1.0f);

    Vertex *a = polygon->first;
    Vertex *b = a->next;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        HM_V4 color = hm_v4(1.0f, 1.0f, 1.0f, 1.0f);

        if (hm_is_v2_equal(a->pos, polygon->drag_pos) ||
            hm_is_v2_equal(b->pos, polygon->drag_pos))
        {
            color = selected_color;
        } else {
            HM_V2 ab = hm_v2_sub(b->pos, a->pos);
            HM_V2 ac = hm_v2_sub(polygon->drag_pos, a->pos);

            HM_Line2 line = hm_line2(a->pos, b->pos);
            f32 proj = hm_get_line2_proj_p(line, polygon->drag_pos);
            if (proj >= 0.0f && proj < 1.0f &&
                hm_get_v2_rad_between(ab, ac) < 0.001f)
            {
                color = selected_color;
            }
        }

        hm_set_render_color(context, color);
        hm_render_line2(context, hm_line2(a->pos, b->pos), 2.0f);

        a = a->next;
        b = b->next;
    }

    if (polygon->selected) {
        hm_set_render_color(context, selected_color);
        hm_render_bbox2(context,
                        hm_bbox2_cen_size(polygon->drag_pos,
                                          hm_v2(VERTEX_DRAG_REGION_SIZE,
                                                VERTEX_DRAG_REGION_SIZE)));
    }

    // Draw diagonal
    {
        hm_set_render_color(context, hm_v4(0.5f, 0.5f, 0.5f, 1.0f));

        a = polygon->first;
        for (u32 i = 0; i < polygon->vertex_count; ++i) {
            b = a->next->next;
            for (u32 j = 3; j < polygon->vertex_count; ++j) {
                if (is_diagonal(polygon, a, b)) {
                    hm_render_line2(context, hm_line2(a->pos, b->pos), 2.0f);
                }
                b = b->next;
            }

            a = a->next;
        }
    }

    hm_render_pop(context);
}
