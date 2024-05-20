#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#define NOB_IMPLEMENTATION
#include "nob.h"
#include "interpolators.h"
#include "plug.h"

#define PLUG(name, ret, ...) ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

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
#define CURVE_FILE_PATH "assets/curves/sigmoid.txt"

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

static bool save_curve_to_file(const char *file_path, Nob_String_Builder *sb, Vector2 curve[COUNT_NODES])
{
    sb->count = 0;
    for (size_t i = 0; i < COUNT_NODES; ++i) {
        nob_sb_append_cstr(sb, TextFormat("%f %f\n", curve[i].x/AXIS_LENGTH, -p->nodes[i].y/AXIS_LENGTH));
    }
    bool ok = nob_write_entire_file(file_path, sb->items, sb->count);
    if (ok) TraceLog(LOG_INFO, "Saved curve to %s", file_path);
    return ok;
}

static bool load_curve_from_file(const char *file_path, Nob_String_Builder *sb, Vector2 curve[COUNT_NODES])
{
    sb->count = 0;
    if (!nob_read_entire_file(file_path, sb)) return false;
    nob_sb_append_null(sb); // NULL-terminator is needed for strtof below
    Nob_String_View content = {
        .data = sb->items,
        .count = sb->count - 1, // Minus the NULL-terminator
    };
    size_t curve_count = 0;
    size_t row = 1;
    for (; content.count > 0 && curve_count < COUNT_NODES; ++row) {
        Nob_String_View line = nob_sv_chop_by_delim(&content, '\n');
        const char *line_start = line.data;

        line = nob_sv_trim_left(line);
        if (line.count == 0) continue; // Silently skipping empty lines

        char *endptr = NULL;
        Nob_String_View arg = nob_sv_chop_by_delim(&line, ' ');
        curve[curve_count].x = strtof(arg.data, &endptr);
        if (endptr == arg.data) {
            TraceLog(LOG_WARNING, "%s:%zu:%zu: x value of node %zu is not a value float "SV_Fmt, file_path, row, arg.data - line_start + 1, curve_count, SV_Arg(arg));
            continue;
        }
        curve[curve_count].x *= AXIS_LENGTH;

        line = nob_sv_trim_left(line);
        if (line.count == 0) {
            TraceLog(LOG_WARNING, "%s:%zu:%zu: y value of node %zu is missing", file_path, row, line.data - line_start + 1, curve_count);
            continue;
        }

        arg = nob_sv_chop_by_delim(&line, ' ');
        curve[curve_count].y = strtof(arg.data, &endptr);
        if (endptr == arg.data) {
            TraceLog(LOG_WARNING, "%s:%zu:%zu: y value of node %zu is not a value float "SV_Fmt, file_path, row, arg.data - line_start + 1, curve_count, SV_Arg(arg));
            continue;
        }
        curve[curve_count].y *= -AXIS_LENGTH;

        TraceLog(LOG_INFO, "Parse field %f %f", curve[curve_count].x, curve[curve_count].y);
        curve_count += 1;

        line = nob_sv_trim_left(line);
        if (line.count > 0) {
            TraceLog(LOG_WARNING, "%s:%zu:%zu: garbage at the end of the line", file_path, row, line.data - line_start + 1);
        }
    }

    content = nob_sv_trim_left(content);
    if (content.count > 0) {
        TraceLog(LOG_WARNING, "%s:%zu:1: garbage at the end of the file "SV_Fmt" %zu", file_path, row, SV_Arg(content), content.data[0]);
    }

    return true;
}

void plug_reset(void)
{
    p->dragged_node = -1;
    if (load_curve_from_file(CURVE_FILE_PATH, &p->sb, p->nodes)) {
        TraceLog(LOG_INFO, "Loaded curve from %s", CURVE_FILE_PATH);
    }
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

void plug_update(Env env)
{
    Color background_color = ColorFromHSV(0, 0, 0.05);
    Color foreground_color = ColorFromHSV(0, 0, 0.95);

    ClearBackground(background_color);

    Camera2D camera = {
        .zoom = 0.8,
        .target = {
            AXIS_LENGTH/2,
            -AXIS_LENGTH/2,
        },
        .offset = {
            .x = env.screen_width/2,
            .y = env.screen_height/2,
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
        }

        size_t res = 30;
        for (size_t i = 0; i <= res; ++i) {
            float t = (float)i/res;
            DrawCircleV(
                cubic_bezier(t, p->nodes),
                BEZIER_SAMPLE_RADIUS,
                BEZIER_SAMPLE_COLOR);
        }
        for (size_t i = 0; i < COUNT_NODES; ++i) {
            bool hover = CheckCollisionPointCircle(mouse, p->nodes[i], NODE_RADIUS);
            DrawCircleV(p->nodes[i], NODE_RADIUS, hover ? NODE_HOVER_COLOR : NODE_COLOR);
            const char *label = TextFormat("{%.2f, %.2f}", p->nodes[i].x/AXIS_LENGTH, p->nodes[i].y/AXIS_LENGTH);
            Vector2 label_position = Vector2Add(p->nodes[i], (Vector2){NODE_RADIUS, NODE_RADIUS});
            DrawTextEx(p->font, label, label_position, FONT_SIZE, 0, foreground_color);
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

        {
            float x = Clamp(mouse.x, 0, AXIS_LENGTH);
            Vector2 start_pos = {
                .x = x,
                .y = 0,
            };
            Vector2 end_pos = {
                .x = x,
                .y = -AXIS_LENGTH,
            };
            DrawLineEx(start_pos, end_pos, HANDLE_THICCNESS, RED);
            float t = cuber_bezier_newton(x, p->nodes, 5);
            DrawCircleV(cubic_bezier(t, p->nodes), NODE_RADIUS, PURPLE);
        }

        if (IsKeyPressed(KEY_S)) {
            if (save_curve_to_file(CURVE_FILE_PATH, &p->sb, p->nodes)) {
                TraceLog(LOG_INFO, "Saved curve to %s", CURVE_FILE_PATH);
            }
        }
    }
    EndMode2D();
}

bool plug_finished(void)
{
    return true;
}

#define ARENA_IMPLEMENTATION
#include "arena.h"
