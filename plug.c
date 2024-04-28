#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"

#define FONT_SIZE 52

typedef struct {
    size_t i;
    float duration;
} Animation;

typedef struct {
    float from;
    float to;
    float duration;
} Keyframe;

static float animation_value(Animation a, Keyframe *kfs, size_t kfs_count)
{
    assert(kfs_count > 0);
    Keyframe *kf = &kfs[a.i%kfs_count];
    float t = a.duration/kf->duration;
    t = (sinf(PI*t - PI*0.5) + 1)*0.5;
    return Lerp(kf->from, kf->to, t);
}

static void animation_update(Animation *a, Keyframe *kfs, size_t kfs_count)
{
    assert(kfs_count > 0);

    a->i = a->i % kfs_count;
    a->duration += GetFrameTime();

    while (a->duration >= kfs[a->i].duration) {
        a->duration -= kfs[a->i].duration;
        a->i = (a->i + 1) % kfs_count;
    }
}

// TODO: support simple migration to bigger structures
typedef struct {
    Animation a;
    Font font;
} Plug;

static Plug *p = NULL;

static void load_resources(void)
{
    p->font = LoadFontEx("./resources/fonts/iosevka-regular.ttf", FONT_SIZE, NULL, 0);
}

static void unload_resources(void)
{
    UnloadFont(p->font);
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
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
    load_resources();
}

void plug_update(void)
{
    float rw = 100.0;
    float rh = 100.0;
    Vector2 cell_size = {rw, rh};
    float pad = rw*0.15;
    float w = GetScreenWidth();
    float h = GetScreenHeight();

    size_t offset = 7;
    Keyframe kfs[] = {
        {.from = w/2 - rw/2 - (offset + 0)*(rw + pad), .to = w/2 - rw/2 - (offset + 0)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 0)*(rw + pad), .to = w/2 - rw/2 - (offset + 1)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 1)*(rw + pad), .to = w/2 - rw/2 - (offset + 1)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 1)*(rw + pad), .to = w/2 - rw/2 - (offset + 2)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 2)*(rw + pad), .to = w/2 - rw/2 - (offset + 2)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 2)*(rw + pad), .to = w/2 - rw/2 - (offset + 3)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 3)*(rw + pad), .to = w/2 - rw/2 - (offset + 3)*(rw + pad), .duration = 0.5,},
        {.from = w/2 - rw/2 - (offset + 3)*(rw + pad), .to = w/2 - rw/2 - (offset + 0)*(rw + pad), .duration = 0.5,},
    };

    #if 1
        Color cell_color = ColorFromHSV(0, 0.0, 0.15);
        Color head_color = ColorFromHSV(200, 0.8, 0.8);
        Color background_color = ColorFromHSV(120, 0.0, 0.95);
    #else
        Color cell_color = ColorFromHSV(0, 0.0, 1 - 0.15);
        Color head_color = ColorFromHSV(200, 0.8, 0.8);
        Color background_color = ColorFromHSV(120, 0.0, 1 - 0.95);
    #endif

    BeginDrawing();
    animation_update(&p->a, kfs, NOB_ARRAY_LEN(kfs));
    float t = animation_value(p->a, kfs, NOB_ARRAY_LEN(kfs));
    ClearBackground(background_color);
    for (size_t i = 0; i < 20; ++i) {
        Rectangle rec = {
            .x = i*(rw + pad) + t,
            .y = h/2 - rh/2,
            .width = rw,
            .height = rh,
        };
        DrawRectangleRec(rec, cell_color);

        const char *text = "0";
        Vector2 text_size = MeasureTextEx(p->font, text, FONT_SIZE, 0);
        Vector2 position = { .x = rec.x, .y = rec.y };
        position = Vector2Add(position, Vector2Scale(cell_size, 0.5));
        position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
        DrawTextEx(p->font, text, position, FONT_SIZE, 0, background_color);
    }

    float head_thick = 20.0;
    Rectangle rec = {
        .width = rw + head_thick*3,
        .height = rh + head_thick*3,
    };
    rec.x = w/2 - rec.width/2;
    rec.y = h/2 - rec.height/2;
    DrawRectangleLinesEx(rec, head_thick, head_color);

    EndDrawing();
}
