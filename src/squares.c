#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "nob.h"
#include "tasks.h"

#define FONT_SIZE 68

#define SQUARES_COUNT 3
#define SQUARE_SIZE 100.0f
#define SQUARE_PAD (SQUARE_SIZE*0.2f)
#define SQUARE_MOVE_DURATION 0.25
#define SQUARE_COLOR_DURATION 0.25
#define BACKGROUND_COLOR ColorFromHSV(0, 0, 0.05)
#define FOREGROUND_COLOR ColorFromHSV(0, 0, 0.95)

size_t TASK_WAIT_TAG = 0;

typedef struct {
    float t;
    float duration;
} Wait_Data;

bool task_wait_update(Env env, void *raw_data)
{
    Wait_Data *data = raw_data;
    if (data->t >= data->duration) return true;
    data->t += env.delta_time;
    return data->t >= data->duration;
}

void task_wait_reset(Env env, void *raw_data)
{
    (void) env;
    Wait_Data *data = raw_data;
    data->t = 0.0f;
}

Task task_wait(Arena *a, float duration)
{
    Wait_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->duration = duration;
    return (Task) {
        .tag = TASK_WAIT_TAG,
        .data = data,
    };
}

typedef struct {
    Vector2 position;
    Vector4 color;
} Square;

typedef struct {
    size_t size;
    Font font;
    Arena state_arena;
    Arena asset_arena;
    Square squares[SQUARES_COUNT];
    Task task;
    bool finished;
} Plug;

static Plug *p = NULL;

Vector2 grid(size_t row, size_t col)
{
    Vector2 world;
    world.x = col*(SQUARE_SIZE + SQUARE_PAD);
    world.y = row*(SQUARE_SIZE + SQUARE_PAD);
    return world;
}

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/Vollkorn-Regular.ttf", FONT_SIZE, NULL, 0);
    Arena *a = &p->asset_arena;
    arena_reset(a);
    task_vtable_rebuild(a);
    TASK_WAIT_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_wait_update,
        .reset = task_wait_reset,
    });
}

static void unload_assets(void)
{
    UnloadFont(p->font);
}

Task shuffle_squares(Arena *a, Square *s1, Square *s2, Square *s3)
{
    return task_seq(a,
        task_group(a,
            task_move_v2(a, &s1->position, grid(1, 1), 0.25),
            task_move_v2(a, &s2->position, grid(0, 0), 0.25),
            task_move_v2(a, &s3->position, grid(0, 1), 0.25)),
        task_group(a,
            task_move_v4(a, &s1->color, RED, 0.25),
            task_move_v4(a, &s2->color, GREEN, 0.25),
            task_move_v4(a, &s3->color, BLUE, 0.25)),
        task_move_v2(a, &s1->position, grid(1, 0), 0.25),
        task_group(a,
            task_move_v4(a, &s1->color, FOREGROUND_COLOR, 0.25),
            task_move_v4(a, &s2->color, FOREGROUND_COLOR, 0.25),
            task_move_v4(a, &s3->color, FOREGROUND_COLOR, 0.25)));
}

Task loading(Arena *a)
{
    Square *s1 = &p->squares[0];
    Square *s2 = &p->squares[1];
    Square *s3 = &p->squares[2];
    return task_seq(a,
        shuffle_squares(a, s1, s2, s3),
        task_wait(a, 1.0f),
        shuffle_squares(a, s2, s3, s1),
        task_wait(a, 1.0f),
        shuffle_squares(a, s3, s1, s2),
        task_wait(a, 1.0f)
    );
}

void plug_reset(void)
{
    for (size_t i = 0; i < SQUARES_COUNT; ++i) {
        p->squares[i].position = grid(i/2, i%2);
        p->squares[i].color = ColorNormalize(FOREGROUND_COLOR);
    }
    p->finished = false;
    arena_reset(&p->state_arena);

    Arena *a = &p->state_arena;
    p->task = loading(a);
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
    p->finished = task_update(p->task, env);

    ClearBackground(BACKGROUND_COLOR);

    Camera2D camera = {0};
    camera.zoom = 1.0f;
    camera.target = CLITERAL(Vector2) {
        -env.screen_width/2 + SQUARE_SIZE + SQUARE_PAD*0.5,
        -env.screen_height/2 + SQUARE_SIZE + SQUARE_PAD*0.5,
    };
    BeginMode2D(camera);
    for (size_t i = 0; i < SQUARES_COUNT; ++i) {
        Rectangle boundary = {
            .x = p->squares[i].position.x,
            .y = p->squares[i].position.y,
            .width = SQUARE_SIZE,
            .height = SQUARE_SIZE,
        };
        DrawRectangleRec(boundary, ColorFromNormalized(p->squares[i].color));
    }
    EndMode2D();
}

bool plug_finished(void)
{
    return p->finished;
}

#define ARENA_IMPLEMENTATION
#include "arena.h"
