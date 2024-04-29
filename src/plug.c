#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"

#define FONT_SIZE 52
#define CELL_WIDTH 100.0f
#define CELL_HEIGHT 100.0f
#define CELL_PAD (CELL_WIDTH*0.15f)

#if 1
    #define CELL_COLOR ColorFromHSV(0, 0.0, 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 0.88)
#else
    #define CELL_COLOR ColorFromHSV(0, 0.0, 1 - 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 1 - 0.88)
#endif


#define HEAD_MOVING_DURATION 0.5f
#define HEAD_WRITING_DURATION 0.2f

static inline float sinstep(float t)
{
    return (sinf(PI*t - PI*0.5) + 1)*0.5;
}

static inline float smoothstep(float t)
{
    return 3*t*t - 2*t*t*t;
}

typedef struct {
    const char *symbol;
} Cell;

#define TAPE_COUNT 20

typedef enum {
    DIR_LEFT = -1,
    DIR_RIGHT = 1,
} Direction;

typedef struct {
    int index;
} Head;

typedef enum {
    ACTION_MOVE,
    ACTION_WRITE,
} Action_Kind;

typedef union {
    Direction move;
    const char *write;
} Action_As;

typedef struct {
    Action_Kind kind;
    Action_As as;
} Action;

static Action script[] = {
    {.kind = ACTION_WRITE, .as = { .write = "Foo" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_RIGHT }},
    {.kind = ACTION_WRITE, .as = { .write = "Bar" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_LEFT }},
    {.kind = ACTION_WRITE, .as = { .write = "0" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_RIGHT }},
    {.kind = ACTION_WRITE, .as = { .write = "0" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_RIGHT }},
    {.kind = ACTION_WRITE, .as = { .write = "1" }},
    {.kind = ACTION_WRITE, .as = { .write = "2" }},
    {.kind = ACTION_WRITE, .as = { .write = "3" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_RIGHT }},
    {.kind = ACTION_WRITE, .as = { .write = "1" }},
    {.kind = ACTION_WRITE, .as = { .write = "2" }},
    {.kind = ACTION_WRITE, .as = { .write = "3" }},
    {.kind = ACTION_MOVE,  .as = { .move  = DIR_RIGHT }},
    {.kind = ACTION_WRITE, .as = { .write = "1" }},
    {.kind = ACTION_WRITE, .as = { .write = "2" }},
    {.kind = ACTION_WRITE, .as = { .write = "3" }},
};

#define script_size NOB_ARRAY_LEN(script)

typedef struct {
    size_t size;

    size_t ip;
    float t;
    Cell tape[TAPE_COUNT];
    Head head;

    Font font;
    Sound plant;
} Plug;

static Plug *p = NULL;

static void load_resources(void)
{
    p->font = LoadFontEx("./resources/fonts/iosevka-regular.ttf", FONT_SIZE, NULL, 0);
    p->plant = LoadSound("./resources/sounds/plant-bomb.wav");
}

static void unload_resources(void)
{
    UnloadFont(p->font);
    UnloadSound(p->plant);
}

void plug_reset(void)
{
    p->head.index = 0;
    p->ip = 0;
    p->t = 0.0;
    for (size_t i = 0; i < TAPE_COUNT; ++i) {
        p->tape[i].symbol = "0";
    }
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->size = sizeof(*p);
    plug_reset();
    load_resources();
}

void *plug_pre_reload(void)
{
    unload_resources();
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

    load_resources();
}

void text_in_cell(Rectangle rec, const char *from_text, const char *to_text, float t)
{
    Vector2 cell_size = {CELL_WIDTH, CELL_HEIGHT};

    {
        float font_size = FONT_SIZE*(1 - t);
        Vector2 text_size = MeasureTextEx(p->font, from_text, font_size, 0);
        Vector2 position = { .x = rec.x, .y = rec.y };
        position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
        position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
        DrawTextEx(p->font, from_text, position, font_size, 0, ColorAlpha(BACKGROUND_COLOR, 1 - t));
    }

    {
        float font_size = FONT_SIZE*t;
        Vector2 text_size = MeasureTextEx(p->font, to_text, font_size, 0);
        Vector2 position = { .x = rec.x, .y = rec.y };
        position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
        position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
        DrawTextEx(p->font, to_text, position, font_size, 0, ColorAlpha(BACKGROUND_COLOR, t));
    }
}

void plug_update(float dt, float w, float h)
{
    ClearBackground(BACKGROUND_COLOR);

    float t = 0.0f;
    if (p->ip < script_size) {
        Action action = NOB_ARRAY_GET(script, p->ip);
        switch (action.kind) {
            case ACTION_MOVE: {
                p->t = (p->t*HEAD_MOVING_DURATION + dt)/HEAD_MOVING_DURATION;
                if (p->t >= 1.0) {
                    p->head.index += action.as.move;

                    p->ip += 1;
                    p->t = 0;
                }

                float from = (float)p->head.index;
                float to = (float)(p->head.index + action.as.move);
                t = Lerp(from, to, sinstep(p->t));
            } break;

            case ACTION_WRITE: {
                float t1 = p->t;
                p->t = (p->t*HEAD_WRITING_DURATION + dt)/HEAD_WRITING_DURATION;
                float t2 = p->t;

                if (t1 < 0.5 && t2 >= 0.5) {
                    PlaySound(p->plant);
                }

                if (p->t >= 1.0) {
                    assert(0 <= p->head.index);
                    assert(p->head.index < TAPE_COUNT);
                    p->tape[p->head.index].symbol = action.as.write;

                    p->ip += 1;
                    p->t = 0;
                }

                t = (float)p->head.index;
            } break;
        }
    } else {
        t = (float)p->head.index;
    }

    for (size_t i = 0; i < TAPE_COUNT; ++i) {
        Rectangle rec = {
            .x = i*(CELL_WIDTH + CELL_PAD) + w/2 - CELL_WIDTH/2 - t*(CELL_WIDTH + CELL_PAD),
            .y = h/2 - CELL_HEIGHT/2,
            .width = CELL_WIDTH,
            .height = CELL_HEIGHT,
        };
        DrawRectangleRec(rec, CELL_COLOR);

        if (
            (size_t)p->head.index == i &&      // we are rendering the head
            p->ip < script_size &&             // there is a currently executing instruction
            script[p->ip].kind == ACTION_WRITE // that instruction is ACTION_WRITE
        ) {
            text_in_cell(rec, p->tape[i].symbol, script[p->ip].as.write, p->t);
        } else {
            text_in_cell(rec, p->tape[i].symbol, "", 0);
        }
    }

    float head_thick = 20.0;
    Rectangle rec = {
        .width = CELL_WIDTH + head_thick*3,
        .height = CELL_HEIGHT + head_thick*3,
    };
    rec.x = w/2 - rec.width/2;
    rec.y = h/2 - rec.height/2;
    DrawRectangleLinesEx(rec, head_thick, HEAD_COLOR);
}

bool plug_finished(void)
{
    return p->ip >= script_size;
}
