#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"
#include "env.h"
#include "interpolators.h"
#include "tasks.h"

#if 1
    #define CELL_COLOR ColorFromHSV(0, 0.0, 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 0.88)
#else
    #define CELL_COLOR ColorFromHSV(0, 0.0, 1 - 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 1 - 0.88)
#endif

#define FONT_SIZE 52
#define CELL_WIDTH 100.0f
#define CELL_HEIGHT 100.0f
#define CELL_PAD (CELL_WIDTH*0.15f)
#define START_AT_CELL_INDEX 10
#define HEAD_MOVING_DURATION 0.5f
#define HEAD_WRITING_DURATION 0.2f
#define INTRO_DURATION 1.0f
#define TAPE_SIZE 50

typedef enum {
    DIR_LEFT = -1,
    DIR_RIGHT = 1,
} Direction;

typedef struct {
    const char *state;
    const char *read;
    const char *write;
    Direction step;
    const char *next;
} Rule;

typedef struct {
    Rule *items;
    size_t count;
    size_t capacity;
} Table;

typedef struct {
    const char *symbol_a;
    const char *symbol_b;
    float t;
} Cell;

typedef struct {
    Cell *items;
    size_t count;
    size_t capacity;
} Tape;

typedef struct {
    int index;
    float offset;
} Head;

typedef struct {
    size_t size;

    // State (survives the plugin reload, resets on plug_reset)
    Arena arena_state;
    Head head;
    Tape tape;
    float scene_t;
    float tape_y_offset;
    Task task;
    bool finished;

    // Assets (reloads along with the plugin, does not change throughout the animation)
    Arena arena_assets;
    Table table;
    Font font;
    Sound write_sound;
    Wave write_wave;
    Texture2D eggplant;
} Plug;

static Plug *p = NULL;

static Tag TASK_INTRO_TAG = 0;
static Tag TASK_MOVE_HEAD_TAG = 0;
static Tag TASK_WRITE_HEAD_TAG = 0;
static Tag TASK_WRITE_ALL_TAG = 0;

typedef struct {
    Wait_Data wait;
    size_t head;
} Intro_Data;

bool task_intro_update(Intro_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;
    if (!data->wait.started) p->head.index = data->head;
    bool finished = wait_update(&data->wait, env);
    p->scene_t = smoothstep(wait_norm(&data->wait));
    return finished;
}

Intro_Data intro_data(size_t head)
{
    return (Intro_Data) {
        .wait = wait_data(INTRO_DURATION),
        .head = head,
    };
}

Task task_intro(Arena *a, size_t head)
{
    Intro_Data data = intro_data(head);
    return (Task) {
        .tag = TASK_INTRO_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    Direction dir;
} Move_Head_Data;

bool move_head_update(Move_Head_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (wait_update(&data->wait, env)) {
        p->head.offset = 0.0f;
        p->head.index += data->dir;
        return true;
    }

    p->head.offset = Lerp(0, data->dir, smoothstep(wait_norm(&data->wait)));
    return false;
}

Move_Head_Data move_head(Direction dir)
{
    return (Move_Head_Data) {
        .wait = wait_data(HEAD_MOVING_DURATION),
        .dir = dir,
    };
}

Task task_move_head(Arena *a, Direction dir)
{
    Move_Head_Data data = move_head(dir);
    return (Task) {
        .tag = TASK_MOVE_HEAD_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    const char *write;
} Write_Head_Data;

bool write_head_update(Write_Head_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    Cell *cell = NULL;
    if ((size_t)p->head.index < p->tape.count) {
        cell = &p->tape.items[(size_t)p->head.index];
    }

    if (!data->wait.started && cell) {
        cell->symbol_b = data->write;
        cell->t = 0.0;
    }

    float t1 = wait_norm(&data->wait);
    bool finished = wait_update(&data->wait, env);
    float t2 = wait_norm(&data->wait);

    if (t1 < 0.5 && t2 >= 0.5) {
        env.play_sound(p->write_sound, p->write_wave);
    }

    if (cell) cell->t = smoothstep(t2);

    if (finished && cell) {
        cell->symbol_a = cell->symbol_b;
        cell->t = 0.0;
    }

    return finished;
}

Write_Head_Data write_head_data(const char *write)
{
    return (Write_Head_Data) {
        .wait = wait_data(HEAD_WRITING_DURATION),
        .write = write,
    };
}

Task task_write_head(Arena *a, const char *write)
{
    Write_Head_Data data = write_head_data(arena_strdup(a, write));
    return (Task) {
        .tag = TASK_WRITE_HEAD_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    const char *write;
} Write_All_Data;

bool write_all_update(Write_All_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (!data->wait.started) {
        for (size_t i = 0; i < p->tape.count; ++i) {
            p->tape.items[i].t = 0.0f;
            p->tape.items[i].symbol_b = data->write;
        }
    }

    float t1 = wait_norm(&data->wait);
    bool finished = wait_update(&data->wait, env);
    float t2 = wait_norm(&data->wait);

    if (t1 < 0.5 && t2 >= 0.5) {
        env.play_sound(p->write_sound, p->write_wave);
    }

    for (size_t i = 0; i < p->tape.count; ++i) {
        p->tape.items[i].t = smoothstep(t2);
    }

    if (finished) {
        for (size_t i = 0; i < p->tape.count; ++i) {
            p->tape.items[i].t = 0.0f;
            p->tape.items[i].symbol_a = p->tape.items[i].symbol_b;
        }
    }

    return finished;
}

Write_All_Data write_all_data(const char *write)
{
    return (Write_All_Data) {
        .write = write,
        .wait = wait_data(HEAD_WRITING_DURATION),
    };
}

Task task_write_all(Arena *a, const char *write)
{
    Write_All_Data data = write_all_data(arena_strdup(a, write));
    return (Task) {
        .tag = TASK_WRITE_ALL_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

static void table(const char *state, const char *read, const char *write, Direction step, const char *next)
{
    Rule rule = {
        .state = state,
        .read = read,
        .write = write,
        .step = step,
        .next = next,
    };
    nob_da_append(&p->table, rule);
}

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/iosevka-regular.ttf", FONT_SIZE, NULL, 0);
    p->eggplant = LoadTexture("./assets/images/eggplant.png");
    p->write_wave = LoadWave("./assets/sounds/plant-bomb.wav");
    p->write_sound = LoadSoundFromWave(p->write_wave);

    // Table
    {
        table("Inc", "0", "1", DIR_RIGHT, "Halt");
        table("Inc", "1", "0", DIR_LEFT,  "Inc");
    }

    Arena *a = &p->arena_assets;
    arena_reset(a);
    task_vtable_rebuild(a);
    TASK_INTRO_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_intro_update,
    });
    TASK_MOVE_HEAD_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)move_head_update,
    });
    TASK_WRITE_HEAD_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)write_head_update,
    });
    TASK_WRITE_ALL_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)write_all_update,
    });
}

static void unload_assets(void)
{
    UnloadFont(p->font);
    UnloadSound(p->write_sound);
    UnloadWave(p->write_wave);
    UnloadTexture(p->eggplant);
    p->table.count = 0;
}

void plug_reset(void)
{
    Arena *a = &p->arena_state;
    arena_reset(a);

    p->head.index = 0;
    p->tape.count = 0;
    char *zero = arena_strdup(a, "0");
    char *one = arena_strdup(a, "1");
    for (size_t i = 0; i < TAPE_SIZE; ++i) {
        Cell cell = {.symbol_a = zero,};
        nob_da_append(&p->tape, cell);
    }

    p->tape.items[START_AT_CELL_INDEX + 0] = CLITERAL(Cell) { .symbol_a = one };
    p->tape.items[START_AT_CELL_INDEX + 1] = CLITERAL(Cell) { .symbol_a = one };
    p->tape.items[START_AT_CELL_INDEX + 2] = CLITERAL(Cell) { .symbol_a = one };
    p->scene_t = 0;
    p->tape_y_offset = 0.0f;

    p->task = task_seq(a,
        task_intro(a, START_AT_CELL_INDEX),
        task_wait(a, 0.25),
        task_write_all(a, "1"),
        task_wait(a, 0.25),
        task_write_all(a, "2"),
        task_wait(a, 0.25),
        task_write_all(a, "3"),
        task_wait(a, 0.25),
        task_write_head(a, "0"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "0"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "0"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "1"),
        task_wait(a, 1),
        task_move_scalar(a, &p->scene_t, 0.0, INTRO_DURATION),

        task_wait(a, 1),

        task_intro(a, START_AT_CELL_INDEX),
        task_wait(a, 0.25),
        task_write_head(a, "1"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "1"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "1"),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, "0"),
        task_wait(a, 1),
        task_move_scalar(a, &p->scene_t, 0.0, INTRO_DURATION));
    p->finished = false;
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

static void text_in_rec(Rectangle rec, const char *from_text, const char *to_text, float t, Color color)
{
    Vector2 cell_size = {rec.width, rec.height};

    {
        float font_size = FONT_SIZE*(1 - t);
        Vector2 text_size = MeasureTextEx(p->font, from_text, font_size, 0);
        Vector2 position = { .x = rec.x, .y = rec.y };
        position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
        position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
        DrawTextEx(p->font, from_text, position, font_size, 0, ColorAlpha(color, 1 - t));
    }

    {
        float font_size = FONT_SIZE*t;
        Vector2 text_size = MeasureTextEx(p->font, to_text, font_size, 0);
        Vector2 position = { .x = rec.x, .y = rec.y };
        position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
        position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
        DrawTextEx(p->font, to_text, position, font_size, 0, ColorAlpha(color, t));
    }
}

static void render_tape(Env env)
{
    float w = env.screen_width;
    float h = env.screen_height;
    float cell_width = CELL_WIDTH;
    float cell_height = CELL_HEIGHT;
    float cell_pad = CELL_PAD;

    float _t = (float)p->head.index + p->head.offset;

    for (size_t i = 0; i < p->tape.count; ++i) {
        Rectangle rec = {
            .x = i*(cell_width + cell_pad) + w/2 - cell_width/2 - Lerp(-20.0, _t, p->scene_t)*(cell_width + cell_pad),
            .y = h/2 - cell_height/2 - p->tape_y_offset,
            .width = cell_width,
            .height = cell_height,
        };
        DrawRectangleRec(rec, CELL_COLOR);

        text_in_rec(rec, p->tape.items[i].symbol_a, p->tape.items[i].symbol_b, p->tape.items[i].t, BACKGROUND_COLOR);
    }
}

static void render_head(Env env)
{
    float w = env.screen_width;
    float h = env.screen_height;
    float head_thick = 20.0;
    Rectangle rec = {
        .width = CELL_WIDTH + head_thick*3 + (1 - p->scene_t)*head_thick*3,
        .height = CELL_HEIGHT + head_thick*3 + (1 - p->scene_t)*head_thick*3,
    };
    rec.x = w/2 - rec.width/2;
    rec.y = h/2 - rec.height/2 - p->tape_y_offset;
    DrawRectangleLinesEx(rec, head_thick, ColorAlpha(HEAD_COLOR, p->scene_t));
}

void plug_update(Env env)
{
    ClearBackground(BACKGROUND_COLOR);

    p->finished = task_update(p->task, env);

    render_tape(env);
    render_head(env);
}

bool plug_finished(void)
{
    return p->finished;
}

#define ARENA_IMPLEMENTATION
#include "arena.h"
