#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "nob.h"
#include "interpolators.h"

#define FONT_SIZE 32
#define AXIS_THICCNESS 5.0
#define AXIS_COLOR BLUE
#define AXIS_LENGTH 500.0f
#define NODE_RADIUS 15.0f
#define NODE_COLOR RED
#define NODE_HOVER_COLOR YELLOW
#define HANDLE_THICCNESS (AXIS_THICCNESS/2)
#define HANDLE_COLOR YELLOW
#define BEZIER_SAMPLE_RADIUS 5
#define BEZIER_SAMPLE_COLOR YELLOW
#define LABEL_PADDING 100.0

#define COUNT_NODES 4

typedef struct {
    size_t size;
    Font font;
    Vector2 nodes[COUNT_NODES];
    int dragged_node;
    Nob_String_Builder sb;
} Plug;

static Plug *p;

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/iosevka-regular.ttf", FONT_SIZE, NULL, 0);
    GenTextureMipmaps(&p->font.texture);
    SetTextureFilter(p->font.texture, TEXTURE_FILTER_BILINEAR);
}

static void unload_assets(void)
{
    UnloadFont(p->font);
}

void plug_reset(void)
{
    p->dragged_node = -1;
    p->nodes[0] = (Vector2){0.0, 0.0};
    p->nodes[1] = (Vector2){AXIS_LENGTH*0.5, AXIS_LENGTH*-0.5};
    p->nodes[2] = (Vector2){AXIS_LENGTH*0.75, AXIS_LENGTH*-0.75};
    p->nodes[3] = (Vector2){AXIS_LENGTH, -AXIS_LENGTH};
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->size = sizeof(*p);

    load_assets();
    plug_reset();
}

void *plug_pre_reload(void)
{
    unload_assets();
    return p;
}

void plug_post_reload(void *state)
{
    p = state;
    if (p->size < sizeof(*p)) {
        TraceLog(LOG_INFO, "Migrating plug state schema %zu bytes -> %zu bytes", p->size, sizeof(*p));
        p = realloc(p, sizeof(*p));
        p->size = sizeof(*p);
    }

    load_assets();
}

// {{0.00, 0.00}, {0.63, 0.23}, {0.84, 1.66}, {1.00, 1.00}}

void plug_update(Env env)
{
    Color background_color = ColorFromHSV(0, 0, 0.05);
    Color foreground_color = ColorFromHSV(0, 0, 0.95);

    ClearBackground(background_color);

    Camera2D camera = {
        .zoom = 0.8,
        .offset = {
            .x = env.screen_width/2 - AXIS_LENGTH/2,
            .y = env.screen_height/2 + AXIS_LENGTH/2,
        },
    };

    BeginMode2D(camera);
    {
        Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);

        DrawLineEx((Vector2) {0.0, 0.0}, (Vector2) {0.0, -AXIS_LENGTH}, AXIS_THICCNESS, AXIS_COLOR);
        DrawLineEx((Vector2) {0.0, 0.0}, (Vector2) {AXIS_LENGTH, 0.0}, AXIS_THICCNESS, AXIS_COLOR);
        DrawLineEx(p->nodes[0], p->nodes[1], HANDLE_THICCNESS, HANDLE_COLOR);
        DrawLineEx(p->nodes[2], p->nodes[3], HANDLE_THICCNESS, HANDLE_COLOR);

        bool dragging = 0 <= p->dragged_node && (size_t)p->dragged_node < COUNT_NODES;
        if (dragging) {
            Vector2 *node = &p->nodes[p->dragged_node];
            *node = mouse;
            // node->x = Clamp(node->x, 0.0, AXIS_LENGTH);
            // node->y = Clamp(node->y, -AXIS_LENGTH, 0.0);
        }

        for (size_t i = 0; i < COUNT_NODES; ++i) {
            bool hover = CheckCollisionPointCircle(mouse, p->nodes[i], NODE_RADIUS);
            DrawCircleV(p->nodes[i], NODE_RADIUS, hover ? NODE_HOVER_COLOR : NODE_COLOR);
            if (dragging) {
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    p->dragged_node = -1;
                }
            } else {
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    p->dragged_node = i;
                }
            }
        }

        size_t res = 30;
        for (size_t i = 0; i < res; ++i) {
            DrawCircleV(
                cubic_bezier((float)i/res, p->nodes),
                BEZIER_SAMPLE_RADIUS,
                BEZIER_SAMPLE_COLOR);
        }

        const char *label =
            TextFormat("{{%.2f, %.2f}, {%.2f, %.2f}, {%.2f, %.2f}, {%.2f, %.2f}}",
                       p->nodes[0].x/AXIS_LENGTH, p->nodes[0].y/AXIS_LENGTH,
                       p->nodes[1].x/AXIS_LENGTH, p->nodes[1].y/AXIS_LENGTH,
                       p->nodes[2].x/AXIS_LENGTH, p->nodes[2].y/AXIS_LENGTH,
                       p->nodes[3].x/AXIS_LENGTH, p->nodes[3].y/AXIS_LENGTH);

        if (IsKeyPressed(KEY_C)) {
            SetClipboardText(label);
        }

        Vector2 label_size = MeasureTextEx(p->font, label, FONT_SIZE, 0);
        Vector2 label_position = {0.0, 0.0};
        label_position.y -= AXIS_LENGTH + LABEL_PADDING;
        label_position.x += AXIS_LENGTH/2;
        label_position.x -= label_size.x/2;
        DrawTextEx(p->font, label, label_position, FONT_SIZE, 0, foreground_color);
    }
    EndMode2D();
}

bool plug_finished(void)
{
    return true;
}

#define ARENA_IMPLEMENTATION
#include "arena.h"
// {{0.00, 0.00}, {0.81, 0.11}, {0.93, 3.00}, {1, 0}}
