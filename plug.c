#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"
#include "ffmpeg.h"

#define FONT_SIZE 52
#define CELL_WIDTH 100.0f
#define CELL_HEIGHT 100.0f
#define CELL_PAD (CELL_WIDTH*0.15f)
// #define RENDER_WIDTH 1600
// #define RENDER_HEIGHT 900
// #define RENDER_FPS 30
#define RENDER_WIDTH 1920
#define RENDER_HEIGHT 1080
#define RENDER_FPS 60
#define RENDER_DELTA_TIME (1.0f/RENDER_FPS)

static inline float sinstep(float t)
{
    return (sinf(PI*t - PI*0.5) + 1)*0.5;
}

typedef struct {
    const char *from;
    const char *to;
    float t;
} Cell;

#define TAPE_COUNT 20

typedef enum {
    HP_MOVING,
    HP_WRITING,
    HP_HALT,
} Head_Phase;

typedef struct {
    Head_Phase phase;
    size_t from, to;
    float t;
} Head;

typedef struct {
    size_t move_to;
    const char *write_to;
} Instruction;

static Instruction instructions[] = {
    {.move_to = 0, .write_to = "ur"},
    {.move_to = 1, .write_to = "mom"},
    {.move_to = 2, .write_to = "is"},
    {.move_to = 3, .write_to = "a"},
    {.move_to = 4, .write_to = "nice"},
    {.move_to = 5, .write_to = "lady"},
    {.move_to = 6, .write_to = ":)"},
};

#define instructions_count NOB_ARRAY_LEN(instructions)

typedef struct {
    size_t size;

    bool pause;
    FFMPEG *ffmpeg;
    RenderTexture2D screen;

    size_t ip;
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

void reset_animation(void)
{
    p->ip = 0;
    for (size_t i = 0; i < TAPE_COUNT; ++i) {
        p->tape[i].from = "0";
        p->tape[i].to   = "1";
        p->tape[i].t    = 0.0f;
    }
    p->head.t = 0.0;
    p->head.phase = HP_MOVING;
    p->head.from = 0;
    p->head.to = NOB_ARRAY_GET(instructions, p->ip).move_to;
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->size = sizeof(*p);
    p->screen = LoadRenderTexture(RENDER_WIDTH, RENDER_HEIGHT);
    reset_animation();
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

void turing_machine(float dt, float _w, float _h)
{
    Vector2 cell_size = {CELL_WIDTH, CELL_HEIGHT};

    #if 1
        Color cell_color = ColorFromHSV(0, 0.0, 0.15);
        Color head_color = ColorFromHSV(200, 0.8, 0.8);
        Color background_color = ColorFromHSV(120, 0.0, 0.88);
    #else
        Color cell_color = ColorFromHSV(0, 0.0, 1 - 0.15);
        Color head_color = ColorFromHSV(200, 0.8, 0.8);
        Color background_color = ColorFromHSV(120, 0.0, 1 - 0.88);
    #endif

    ClearBackground(background_color);

    float t = 0.0f;
    #define HEAD_MOVING_DURATION 0.5f
    #define HEAD_WRITING_DURATION 0.2f
    switch (p->head.phase) {
        case HP_MOVING: {
            p->head.t = (p->head.t*HEAD_MOVING_DURATION + dt)/HEAD_MOVING_DURATION;
            if (p->head.t >= 1.0) {
                p->head.from = p->head.to;
                p->head.phase = HP_WRITING;

                p->tape[p->head.from].t = 0;
                p->tape[p->head.from].to = NOB_ARRAY_GET(instructions, p->ip).write_to;
            }
            t = (float)p->head.from + ((float)p->head.to - (float)p->head.from)*sinstep(p->head.t);
        } break;

        case HP_WRITING: {
            float t1 = p->tape[p->head.from].t;
            p->tape[p->head.from].t = (p->tape[p->head.from].t*HEAD_WRITING_DURATION + dt)/HEAD_WRITING_DURATION;
            float t2 = p->tape[p->head.from].t;

            if (t1 < 0.5 && t2 >= 0.5) {
                PlaySound(p->plant);
            }

            t = (float)p->head.from;

            if (p->tape[p->head.from].t >= 1.0) {
                if (p->ip + 1 >= instructions_count) {
                    p->head.phase = HP_HALT;
                } else if (NOB_ARRAY_GET(instructions, p->ip).move_to >= TAPE_COUNT) {
                    p->head.phase = HP_HALT;
                } else {
                    p->ip += 1;
                    p->head.to = NOB_ARRAY_GET(instructions, p->ip).move_to;
                    p->head.t = 0.0f;
                    p->head.phase = HP_MOVING;
                }
            }
        } break;

        case HP_HALT: {
            t = (float)p->head.from;
        } break;
    }

    for (size_t i = 0; i < TAPE_COUNT; ++i) {
        Rectangle rec = {
            .x = i*(CELL_WIDTH + CELL_PAD) + _w/2 - CELL_WIDTH/2 - t*(CELL_WIDTH + CELL_PAD),
            .y = _h/2 - CELL_HEIGHT/2,
            .width = CELL_WIDTH,
            .height = CELL_HEIGHT,
        };
        DrawRectangleRec(rec, cell_color);

        {
            const char *from_text = p->tape[i].from;
            float q = (1 - p->tape[i].t);
            float font_size = FONT_SIZE*q;
            Vector2 text_size = MeasureTextEx(p->font, from_text, font_size, 0);
            Vector2 position = { .x = rec.x, .y = rec.y };
            position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
            position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
            DrawTextEx(p->font, from_text, position, font_size, 0, ColorAlpha(background_color, q));
        }

        {
            const char *to_text = p->tape[i].to;
            float q = p->tape[i].t;
            float font_size = FONT_SIZE*q;
            Vector2 text_size = MeasureTextEx(p->font, to_text, font_size, 0);
            Vector2 position = { .x = rec.x, .y = rec.y };
            position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
            position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
            DrawTextEx(p->font, to_text, position, font_size, 0, ColorAlpha(background_color, q));
        }
    }

    float head_thick = 20.0;
    Rectangle rec = {
        .width = CELL_WIDTH + head_thick*3,
        .height = CELL_HEIGHT + head_thick*3,
    };
    rec.x = _w/2 - rec.width/2;
    rec.y = _h/2 - rec.height/2;
    DrawRectangleLinesEx(rec, head_thick, head_color);
}

void plug_update(void)
{
    BeginDrawing();
        if (p->ffmpeg) {
            if (p->head.phase == HP_HALT) {
                ffmpeg_end_rendering(p->ffmpeg);
                reset_animation();
                p->ffmpeg = NULL;
                SetTraceLogLevel(LOG_INFO);
            } else {
                BeginTextureMode(p->screen);
                turing_machine(RENDER_DELTA_TIME, RENDER_WIDTH, RENDER_HEIGHT);
                EndTextureMode();

                Image image = LoadImageFromTexture(p->screen.texture);
                if (!ffmpeg_send_frame_flipped(p->ffmpeg, image.data, image.width, image.height)) {
                    // NOTE: we don't check the result of ffmpeg_end_rendering here because we
                    // don't care at this point: writing a frame failed, so something went completely
                    // wrong. So let's just show to the user the "FFmpeg Failure" screen. ffmpeg_end_rendering
                    // should log any additional errors anyway.
                    ffmpeg_end_rendering(p->ffmpeg);
                    reset_animation();
                    p->ffmpeg = NULL;
                    SetTraceLogLevel(LOG_INFO);
                }
                UnloadImage(image);
            }
        } else {
            if (IsKeyPressed(KEY_R)) {
                SetTraceLogLevel(LOG_WARNING);
                p->ffmpeg = ffmpeg_start_rendering(RENDER_WIDTH, RENDER_HEIGHT, RENDER_FPS);
                reset_animation();
            }
            if (IsKeyPressed(KEY_SPACE)) {
                p->pause = !p->pause;
            }
            turing_machine(p->pause ? 0.0f : GetFrameTime(), GetScreenWidth(), GetScreenHeight());
        }
    EndDrawing();
}
