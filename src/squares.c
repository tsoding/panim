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
    return 3*x*x - 2*x*x*x;
}

typedef struct {
    void (*setup)(Env, void*);
    bool (*update)(Env, void*);
    void (*teardown)(Env, void*);
    void *data;
} Task;

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} Tasks;

typedef struct {
    Rectangle boundary;
    Vector2 position_offset;

    Vector4 color;
    Vector4 color_offset;
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

static void task_dummy_setup(Env env, void *data)
{
    (void) env;
    (void) data;
}

typedef struct {
    float t;
    size_t square_id;
    Vector2 target;
} Move_Data;

static bool task_move_update(Env env, void *raw_data)
{
    Move_Data *data = raw_data;
    if (data->t >= 1.0f) return true;

    data->t = (data->t*SQUARE_MOVE_DURATION + env.delta_time)/SQUARE_MOVE_DURATION;

    if (data->square_id < SQUARES_COUNT) {
        Square *square = &p->squares[data->square_id];

        Vector2 position = {
            square->boundary.x,
            square->boundary.y,
        };

        square->position_offset = Vector2Subtract(data->target, position);
        square->position_offset = Vector2Scale(square->position_offset, smoothstep(data->t));
    }

    return data->t >= 1.0f;
}

static void task_move_teardown(Env env, void *data)
{
    (void) env;
    Move_Data *move_data = data;
    if (move_data->square_id < SQUARES_COUNT) {
        Square *square = &p->squares[move_data->square_id];

        square->boundary.x += square->position_offset.x;
        square->boundary.y += square->position_offset.y;
        square->position_offset.x = 0;
        square->position_offset.y = 0;
    }
}

static Task task_move(size_t square_id, Vector2 target)
{
    Move_Data *data = arena_alloc(&p->state_arena, sizeof(*data));
    data->square_id = square_id;
    data->target = target;
    data->t = 0.0;
    return (Task) {
        .setup = task_dummy_setup,
        .update = task_move_update,
        .teardown = task_move_teardown,
        .data = data,
    };
}

typedef struct {
    Tasks tasks;
} Group_Data;

static void task_group_setup(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        it->setup(env, it->data);
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

static void task_group_teardown(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        it->teardown(env, it->data);
    }
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
        .setup = task_group_setup,
        .update = task_group_update,
        .teardown = task_group_teardown,
        .data = data,
    };
}

typedef struct {
    float t;
    size_t square_id;
    Vector4 target;
} Color_Data;

static bool task_color_update(Env env, void *raw_data)
{
    Color_Data *data = raw_data;
    if (data->t >= 1.0f) return true;

    data->t = (data->t*SQUARE_COLOR_DURATION + env.delta_time)/SQUARE_COLOR_DURATION;

    if (data->square_id < SQUARES_COUNT) {
        Square *square = &p->squares[data->square_id];

        square->color_offset = QuaternionSubtract(data->target, square->color);
        square->color_offset = QuaternionScale(square->color_offset, smoothstep(data->t));
    }

    return data->t >= 1.0f;
}

static void task_color_teardown(Env env, void *raw_data)
{
    (void) env;
    Color_Data *data = raw_data;
    if (data->square_id < SQUARES_COUNT) {
        Square *square = &p->squares[data->square_id];
        square->color = QuaternionAdd(square->color, square->color_offset);
        square->color_offset = (Vector4){0};
    }
}

static Task task_color(size_t square_id, Color rgba)
{
    Color_Data *data = arena_alloc(&p->state_arena, sizeof(*data));
    data->square_id = square_id;
    data->target = ColorNormalize(rgba);
    data->t = 0.0f;
    return (Task) {
        .setup = task_dummy_setup,
        .update = task_color_update,
        .teardown = task_color_teardown,
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
        Vector2 world = grid_to_world(i/2, i%2);
        p->squares[i].boundary.x = world.x;
        p->squares[i].boundary.y = world.y;
        p->squares[i].boundary.width = SQUARE_SIZE;
        p->squares[i].boundary.height = SQUARE_SIZE;
        p->squares[i].position_offset = Vector2Zero();

        p->squares[i].color = ColorNormalize(FOREGROUND_COLOR);
        p->squares[i].color_offset = CLITERAL(Vector4){0};
    }
    p->it = 0;

    memset(&p->tasks, 0, sizeof(p->tasks));
    arena_reset(&p->state_arena);

#if 0
    arena_da_append(&p->state_arena, &p->tasks, task_move(0, grid_to_world(1, 1)));
    arena_da_append(&p->state_arena, &p->tasks, task_color(0, RED));
    arena_da_append(&p->state_arena, &p->tasks, task_move(1, grid_to_world(0, 0)));
    arena_da_append(&p->state_arena, &p->tasks, task_color(1, GREEN));
    arena_da_append(&p->state_arena, &p->tasks, task_move(2, grid_to_world(0, 1)));
    arena_da_append(&p->state_arena, &p->tasks, task_color(2, BLUE));
#else
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move(0, grid_to_world(1, 1)),
                        task_move(1, grid_to_world(0, 0)),
                        task_move(2, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(0, RED),
                        task_color(1, GREEN),
                        task_color(2, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move(0, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(0, WHITE),
                        task_color(1, WHITE),
                        task_color(2, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move(1, grid_to_world(1, 1)),
                        task_move(2, grid_to_world(0, 0)),
                        task_move(0, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(1, RED),
                        task_color(2, GREEN),
                        task_color(0, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move(1, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(1, WHITE),
                        task_color(2, WHITE),
                        task_color(0, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move(2, grid_to_world(1, 1)),
                        task_move(0, grid_to_world(0, 0)),
                        task_move(1, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(2, RED),
                        task_color(0, GREEN),
                        task_color(1, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move(2, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(2, WHITE),
                        task_color(0, WHITE),
                        task_color(1, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move(0, grid_to_world(1, 1)),
                        task_move(1, grid_to_world(0, 0)),
                        task_move(2, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(0, RED),
                        task_color(1, GREEN),
                        task_color(2, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move(0, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(0, WHITE),
                        task_color(1, WHITE),
                        task_color(2, WHITE)));

    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_move(1, grid_to_world(1, 1)),
                        task_move(2, grid_to_world(0, 0)),
                        task_move(0, grid_to_world(0, 1))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(1, RED),
                        task_color(2, GREEN),
                        task_color(0, BLUE)));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(1,
                        task_move(1, grid_to_world(1, 0))));
    arena_da_append(&p->state_arena, &p->tasks,
                    task_group(3,
                        task_color(1, WHITE),
                        task_color(2, WHITE),
                        task_color(0, WHITE)));
#endif
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
            task.teardown(env, task.data);
            p->it += 1;
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
        Rectangle boundary = p->squares[i].boundary;
        boundary.x += p->squares[i].position_offset.x;
        boundary.y += p->squares[i].position_offset.y;
        Vector4 color = QuaternionAdd(p->squares[i].color, p->squares[i].color_offset);
        DrawRectangleRec(boundary, ColorFromNormalized(color));
    }
    EndMode2D();
}

bool plug_finished(void)
{
    return p->it >= p->tasks.count;
}
