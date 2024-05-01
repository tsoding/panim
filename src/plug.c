#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "env.h"

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

static inline float sinstep(float t)
{
    return (sinf(PI*t - PI*0.5) + 1)*0.5;
}

static inline float smoothstep(float t)
{
    return 3*t*t - 2*t*t*t;
}

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
    const char *state;
} Head;

typedef enum {
    ACTION_MOVE,
    ACTION_WRITE,
    ACTION_WRITE_ALL,
    ACTION_SWITCH,
    ACTION_INTRO,
    ACTION_OUTRO,
    ACTION_WAIT,
} Action_Kind;

typedef union {
    Direction move;
    const char *write;
    const char *write_all;
    const char *sweetch;
    size_t intro; // index of the tape to settle on
    size_t jump;  // action to jump to
    float wait;
} Action_As;

typedef struct {
    Action_Kind kind;
    Action_As as;
} Action;

typedef struct {
    Action *items;
    size_t count;
    size_t capacity;
} Script;

typedef struct {
    size_t size;

    // State (survives the plugin reload, resets on plug_reset)
    size_t ip;
    float action_t;
    float action_init;
    Arena tape_strings;
    Head head;
    Tape tape;
    float scene_t;
    float tape_y_offset;

    // Assets (reloads along with the plugin, does not change throughout the animation)
    Script script;
    Table table;
    Font font;
    Sound write_sound;
    Wave write_wave;
} Plug;

static Plug *p = NULL;

static void action(Action_Kind kind, ...)
{
    Action action = { .kind = kind };
    va_list args;
    va_start(args, kind);
    switch (kind) {
    case ACTION_MOVE:      action.as.move      = va_arg(args, Direction);     break;
    case ACTION_WRITE:     action.as.write     = va_arg(args, const char *);  break;
    case ACTION_WRITE_ALL: action.as.write_all = va_arg(args, const char *);  break;
    case ACTION_SWITCH:    action.as.sweetch   = va_arg(args, const char *);  break;
    case ACTION_INTRO:     action.as.intro     = va_arg(args, size_t);        break;
    case ACTION_OUTRO:                                                        break;
    case ACTION_WAIT:      action.as.wait      = (float)va_arg(args, double); break;
    default:
        fprintf(stderr, "UNREACHABLE\n");
        abort();
    }
    va_end(args);
    nob_da_append(&p->script, action);
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
    p->write_wave = LoadWave("./assets/sounds/plant-bomb.wav");
    p->write_sound = LoadSoundFromWave(p->write_wave);

    // Table
    {
        table("Inc", "0", "1", DIR_RIGHT, "Halt");
        table("Inc", "1", "0", DIR_LEFT,  "Inc");
    }

    // Script
    {
        action(ACTION_INTRO, START_AT_CELL_INDEX);
        action(ACTION_WAIT, 0.25f);
        action(ACTION_SWITCH, "Inc");
        action(ACTION_WRITE_ALL, "1");
        action(ACTION_WAIT, 0.25f);
        action(ACTION_WRITE_ALL, "2");
        action(ACTION_WAIT, 0.25f);
        action(ACTION_WRITE_ALL, "3");
        action(ACTION_WAIT, 0.25f);
        action(ACTION_WRITE, "0");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "0");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "0");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "1");
        action(ACTION_SWITCH, "Halt");
        action(ACTION_WAIT, 1.0f);
        action(ACTION_SWITCH, "");
        action(ACTION_OUTRO);

        action(ACTION_WAIT, 1.0f);

        action(ACTION_INTRO, START_AT_CELL_INDEX);
        action(ACTION_WAIT, 0.25f);
        action(ACTION_SWITCH, "Dec");
        action(ACTION_WRITE, "1");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "1");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "1");
        action(ACTION_MOVE, DIR_RIGHT);
        action(ACTION_WRITE, "0");
        action(ACTION_SWITCH, "Halt");
        action(ACTION_WAIT, 1.0f);
        action(ACTION_SWITCH, "");
        action(ACTION_OUTRO);
    }
}

static void unload_assets(void)
{
    UnloadFont(p->font);
    UnloadSound(p->write_sound);
    UnloadWave(p->write_wave);
    p->script.count = 0;
    p->table.count = 0;
}

void plug_reset(void)
{
    p->ip = 0;
    p->action_t = 0.0f;
    arena_reset(&p->tape_strings);
    p->head.index = 0;
    p->head.state = arena_strdup(&p->tape_strings, "");
    p->tape.count = 0;
    char *zero = arena_strdup(&p->tape_strings, "0");
    char *one = arena_strdup(&p->tape_strings, "1");
    for (size_t i = 0; i < TAPE_SIZE; ++i) {
        Cell cell = {.symbol_a = zero,};
        nob_da_append(&p->tape, cell);
    }

    p->tape.items[START_AT_CELL_INDEX + 0] = CLITERAL(Cell) { .symbol_a = one };
    p->tape.items[START_AT_CELL_INDEX + 1] = CLITERAL(Cell) { .symbol_a = one };
    p->tape.items[START_AT_CELL_INDEX + 2] = CLITERAL(Cell) { .symbol_a = one };
    p->scene_t = 0;
    p->tape_y_offset = 0.0f;

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

static void render_tape(float w, float h, float t)
{
    float cell_width = CELL_WIDTH;
    float cell_height = CELL_HEIGHT;
    float cell_pad = CELL_PAD;

    for (size_t i = 0; i < p->tape.count; ++i) {
        Rectangle rec = {
            .x = i*(cell_width + cell_pad) + w/2 - cell_width/2 - Lerp(-20.0, t, p->scene_t)*(cell_width + cell_pad),
            .y = h/2 - cell_height/2 - p->tape_y_offset,
            .width = cell_width,
            .height = cell_height,
        };
        DrawRectangleRec(rec, CELL_COLOR);

        text_in_rec(rec, p->tape.items[i].symbol_a, p->tape.items[i].symbol_b, p->tape.items[i].t, BACKGROUND_COLOR);
    }
}

static void render_head(float w, float h, float state_t)
{
    float head_thick = 20.0;
    Rectangle rec = {
        .width = CELL_WIDTH + head_thick*3 + (1 - p->scene_t)*head_thick*3,
        .height = CELL_HEIGHT + head_thick*3 + (1 - p->scene_t)*head_thick*3,
    };
    rec.x = w/2 - rec.width/2;
    rec.y = h/2 - rec.height/2 - p->tape_y_offset;
    DrawRectangleLinesEx(rec, head_thick, ColorAlpha(HEAD_COLOR, p->scene_t));

    const char *text = p->head.state;
    float font_size = FONT_SIZE;
    Rectangle text_rec = {
        .x = rec.x,
        .y = rec.y + rec.height,
        .width = rec.width,
        .height = font_size*1.5,
    };
    if (
        // there is a currently executing instruction
        p->ip < p->script.count &&
        // the instruction is ACTION_SWITCH
        p->script.items[p->ip].kind == ACTION_SWITCH
    ) {
        text_in_rec(text_rec, text, p->script.items[p->ip].as.sweetch, state_t, ColorAlpha(CELL_COLOR, p->scene_t));
    } else {
        text_in_rec(text_rec, text, "", 0.0, ColorAlpha(CELL_COLOR, p->scene_t));
    }
}

static void next_action(void)
{
    p->action_t = 0.0f;
    p->action_init = false;
    p->ip += 1;
}

void plug_update(Env env)
{
    float dt = env.delta_time;
    float w = env.screen_width;
    float h = env.screen_height;

    ClearBackground(BACKGROUND_COLOR);

    if (p->ip < p->script.count) {
        Action action = p->script.items[p->ip];
        switch (action.kind) {
            case ACTION_WRITE_ALL: {
                if (!p->action_init) {
                    p->action_init = true;
                    char *symbol_b = arena_strdup(&p->tape_strings, action.as.write_all);
                    for (size_t i = 0; i < p->tape.count; ++i) {
                        p->tape.items[i].symbol_b = symbol_b;
                        p->tape.items[i].t = 0.0f;
                    }
                }

                float t1 = p->action_t;
                p->action_t = (p->action_t*HEAD_WRITING_DURATION + dt)/HEAD_WRITING_DURATION;
                float t2 = p->action_t;

                if (t1 < 0.5 && t2 >= 0.5) {
                    env.play_sound(p->write_sound, p->write_wave);
                }

                for (size_t i = 0; i < p->tape.count; ++i) {
                    p->tape.items[i].t = sinstep(p->action_t);
                }

                render_tape(w, h, (float)p->head.index);
                render_head(w, h, 0.0f);

                if (p->action_t >= 1.0f) {
                    for (size_t i = 0; i < p->tape.count; ++i) {
                        p->tape.items[i].symbol_a = p->tape.items[i].symbol_b;
                        p->tape.items[i].t = 0.0f;
                    }

                    next_action();
                }
            } break;

            case ACTION_WAIT: {
                if (!p->action_init) {
                    p->action_init = true;
                    // nothing to setup
                }

                p->action_t = (p->action_t*action.as.wait + dt)/action.as.wait;
                render_tape(w, h, (float)p->head.index);
                render_head(w, h, 0.0);

                if (p->action_t >= 1.0f) {
                    // nothing to teardown
                    next_action();
                }
            } break;

            case ACTION_INTRO: {
                if (!p->action_init) {
                    p->action_init = true;
                    // nothing to setup
                }

                p->action_t = (p->action_t*INTRO_DURATION + dt)/INTRO_DURATION;
                p->scene_t = sinstep(p->action_t);
                render_tape(w, h, (float)action.as.intro);
                render_head(w, h, 0.0f);

                if (p->action_t >= 1.0) {
                    p->head.index = action.as.intro;
                    p->scene_t = 1;

                    next_action();
                }
            } break;

            case ACTION_OUTRO: {
                if (!p->action_init) {
                    p->action_init = true;
                    // nothing to setup
                }

                p->action_t = (p->action_t*INTRO_DURATION + dt)/INTRO_DURATION;
                p->scene_t = sinstep(1.0f - p->action_t);
                render_tape(w, h, (float)p->head.index);
                render_head(w, h, 0.0f);

                if (p->action_t >= 1.0) {
                    p->scene_t = 0;

                    next_action();
                }
            } break;

            case ACTION_MOVE: {
                if (!p->action_init) {
                    p->action_init = true;
                    // nothing to setup
                }

                p->action_t = (p->action_t*HEAD_MOVING_DURATION + dt)/HEAD_MOVING_DURATION;

                float from = (float)p->head.index;
                float to = (float)(p->head.index + action.as.move);
                render_tape(w, h, Lerp(from, to, sinstep(p->action_t)));
                render_head(w, h, 0.0f);

                if (p->action_t >= 1.0) {
                    p->head.index += action.as.move;

                    next_action();
                }
            } break;

            case ACTION_SWITCH: {
                if (!p->action_init) {
                    p->action_init = true;
                    // nothing to setup
                }

                p->action_t = (p->action_t*HEAD_WRITING_DURATION + dt)/HEAD_WRITING_DURATION;

                render_tape(w, h, (float)p->head.index);
                render_head(w, h, sinstep(p->action_t));

                if (p->action_t >= 1.0) {
                    p->head.state = arena_strdup(&p->tape_strings, action.as.sweetch);

                    next_action();
                }
            } break;

            case ACTION_WRITE: {
                Cell *cell = NULL;
                if ((size_t)p->head.index < p->tape.count) {
                    cell = &p->tape.items[(size_t)p->head.index];
                }

                if (!p->action_init) {
                    p->action_init = true;
                    if (cell) {
                        cell->symbol_b = arena_strdup(&p->tape_strings, action.as.write);
                    }
                }

                float t1 = p->action_t;
                p->action_t = (p->action_t*HEAD_WRITING_DURATION + dt)/HEAD_WRITING_DURATION;
                float t2 = p->action_t;

                if (t1 < 0.5 && t2 >= 0.5) {
                    env.play_sound(p->write_sound, p->write_wave);
                }

                cell->t = sinstep(p->action_t);

                render_tape(w, h, (float)p->head.index);
                render_head(w, h, 0.0f);

                if (p->action_t >= 1.0) {
                    if (cell) {
                        cell->symbol_a = cell->symbol_b;
                        cell->t = 0.0;
                    }

                    next_action();
                }
            } break;
        }
    }
}

bool plug_finished(void)
{
    return p->ip >= p->script.count;
}
