#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "nob.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"

#define FONT_SIZE 68

#define SQUARES_COUNT 3
#define SQUARE_SIZE 100.0f
#define SQUARE_PAD (SQUARE_SIZE*0.2f)
#define SQUARE_MOVE_DURATION 0.25
#define SQUARE_COLOR_DURATION 0.25
#define BACKGROUND_COLOR ColorFromHSV(0, 0, 0.05)
#define FOREGROUND_COLOR ColorFromHSV(0, 0, 0.95)

static float smoothstep(float x)
{
    if (x < 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return 3*x*x - 2*x*x*x;
}

typedef struct {
    void (*reset)(Env, void*);
    bool (*update)(Env, void*);
    void *data;
} Task;

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} Tasks;

typedef struct {
    Vector2 position;
    Vector4 color;
} Square;

typedef struct {
    size_t size;
    Font font;
    Arena state_arena;
    Tasks tasks;
    Square squares[SQUARES_COUNT];

    size_t it;
} Plug;

static Plug *p = NULL;

Vector2 grid_to_world(size_t row, size_t col)
{
    Vector2 world;
    world.x = col*(SQUARE_SIZE + SQUARE_PAD);
    world.y = row*(SQUARE_SIZE + SQUARE_PAD);
    return world;
}

typedef struct {
    Vector2 *value;
    float t;
    Vector2 begin, end;
    bool init;
} Move_V2_Data;

static void task_move_v2_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V2_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

static bool task_move_v2_update(Env env, void *raw_data)
{
    Move_V2_Data *data = raw_data;
    if (data->t >= 1.0f) return true; // task is done

    if (!data->init) {
        // First update of the task
        if (data->value) data->begin = *data->value;
        data->init = true;
    }

    data->t = (data->t*SQUARE_MOVE_DURATION + env.delta_time)/SQUARE_MOVE_DURATION;
    if (data->value) *data->value = Vector2Lerp(data->begin, data->end, smoothstep(data->t));
    return data->t >= 1.0f;
}

static Task task_move_v2(Vector2 *value, Vector2 target)
{
    Move_V2_Data *data = arena_alloc(&p->state_arena, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->end = target;
    return (Task) {
        .reset = task_move_v2_reset,
        .update = task_move_v2_update,
        .data = data,
    };
}

typedef struct {
    Tasks tasks;
} Group_Data;

static void task_group_reset(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        it->reset(env, it->data);
    }
}

static bool task_group_update(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    bool finished = true;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        if (!it->update(env, it->data)) {
            finished = false;
        }
    }
    return finished;
}

static Task task_group(size_t n, ...)
{
    Group_Data *data = arena_alloc(&p->state_arena, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, n);
    for (size_t i = 0; i < n; ++i) {
        arena_da_append(&p->state_arena, &data->tasks, va_arg(args, Task));
    }
    va_end(args);

    return (Task) {
        .reset = task_group_reset,
        .update = task_group_update,
        .data = data,
    };
}

typedef struct {
    float t;
    Vector4 *value;
    Vector4 begin, end;
    bool init;
} Move_V4_Data;

static void task_move_v4_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V4_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

static bool task_move_v4_update(Env env, void *raw_data)
{
    Move_V4_Data *data = raw_data;
    if (data->t >= 1.0f) return true;

    if (!data->init) {
        // First update of the task
        if (data->value) data->begin = *data->value;
        data->init = true;
    }

    data->t = (data->t*SQUARE_COLOR_DURATION + env.delta_time)/SQUARE_COLOR_DURATION;
    if (data->value) *data->value = QuaternionLerp(data->begin, data->end, smoothstep(data->t));
    return data->t >= 1.0f;
}

static Task task_move_v4(Vector4 *value, Color target)
{
    Move_V4_Data *data = arena_alloc(&p->state_arena, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->end = ColorNormalize(target);
    return (Task) {
        .reset = task_move_v4_reset,
        .update = task_move_v4_update,
        .data = data,
    };
}

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/Vollkorn-Regular.ttf", FONT_SIZE, NULL, 0);
}

static void unload_assets(void)
{
    UnloadFont(p->font);
}

void plug_reset(void)
{
    for (size_t i = 0; i < SQUARES_COUNT; ++i) {
        p->squares[i].position = grid_to_world(i/2, i%2);
        p->squares[i].color = ColorNormalize(FOREGROUND_COLOR);
    }
    p->it = 0;

    memset(&p->tasks, 0, sizeof(p->tasks));
    arena_reset(&p->state_arena);

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v2(&p->squares[0].position, grid_to_world(1, 1)),
                        task_move_v2(&p->squares[1].position, grid_to_world(0, 0)),
                        task_move_v2(&p->squares[2].position, grid_to_world(0, 1))));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[0].color, RED),
                        task_move_v4(&p->squares[1].color, GREEN),
                        task_move_v4(&p->squares[2].color, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move_v2(&p->squares[0].position, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[0].color, WHITE),
                        task_move_v4(&p->squares[1].color, WHITE),
                        task_move_v4(&p->squares[2].color, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v2(&p->squares[1].position, grid_to_world(1, 1)),
                        task_move_v2(&p->squares[2].position, grid_to_world(0, 0)),
                        task_move_v2(&p->squares[0].position, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[1].color, RED),
                        task_move_v4(&p->squares[2].color, GREEN),
                        task_move_v4(&p->squares[0].color, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move_v2(&p->squares[1].position, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[1].color, WHITE),
                        task_move_v4(&p->squares[2].color, WHITE),
                        task_move_v4(&p->squares[0].color, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v2(&p->squares[2].position, grid_to_world(1, 1)),
                        task_move_v2(&p->squares[0].position, grid_to_world(0, 0)),
                        task_move_v2(&p->squares[1].position, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[2].color, RED),
                        task_move_v4(&p->squares[0].color, GREEN),
                        task_move_v4(&p->squares[1].color, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move_v2(&p->squares[2].position, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[2].color, WHITE),
                        task_move_v4(&p->squares[0].color, WHITE),
                        task_move_v4(&p->squares[1].color, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v2(&p->squares[0].position, grid_to_world(1, 1)),
                        task_move_v2(&p->squares[1].position, grid_to_world(0, 0)),
                        task_move_v2(&p->squares[2].position, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[0].color, RED),
                        task_move_v4(&p->squares[1].color, GREEN),
                        task_move_v4(&p->squares[2].color, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move_v2(&p->squares[0].position, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[0].color, WHITE),
                        task_move_v4(&p->squares[1].color, WHITE),
                        task_move_v4(&p->squares[2].color, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v2(&p->squares[1].position, grid_to_world(1, 1)),
                        task_move_v2(&p->squares[2].position, grid_to_world(0, 0)),
                        task_move_v2(&p->squares[0].position, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[1].color, RED),
                        task_move_v4(&p->squares[2].color, GREEN),
                        task_move_v4(&p->squares[0].color, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move_v2(&p->squares[1].position, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move_v4(&p->squares[1].color, WHITE),
                        task_move_v4(&p->squares[2].color, WHITE),
                        task_move_v4(&p->squares[0].color, WHITE)));
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
    if (p->it < p->tasks.count) {
        Task task = p->tasks.items[p->it];
        if (task.update(env, task.data)) {
            p->it += 1;
            if (p->it < p->tasks.count) {
                task = p->tasks.items[p->it];
                task.reset(env, task.data);
            }
        }
    }

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
    return p->it >= p->tasks.count;
}
