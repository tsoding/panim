#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"

typedef struct {
    size_t i;
    float duration;
} Animation;

typedef struct {
    Color background;
    Animation a;
} Plug;

static Plug *p = NULL;

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->background = GREEN;
}

void *plug_pre_reload(void)
{
    return p;
}

void plug_post_reload(void *state)
{
    p = state;
}

typedef struct {
    float from;
    float to;
    float duration;
} Keyframe;

float animation_value(Animation a, Keyframe *kfs, size_t kfs_count)
{
    assert(kfs_count > 0);
    Keyframe *kf = &kfs[a.i%kfs_count];
    float t = a.duration/kf->duration;
    t = (sinf(PI*t - PI*0.5) + 1)*0.5;
    return Lerp(kf->from, kf->to, t);
}

void animation_update(Animation *a, Keyframe *kfs, size_t kfs_count)
{
    assert(kfs_count > 0);

    a->i = a->i % kfs_count;
    a->duration += GetFrameTime();

    while (a->duration >= kfs[a->i].duration) {
        a->duration -= kfs[a->i].duration;
        a->i = (a->i + 1) % kfs_count;
    }
}

void plug_update(void)
{
    float rw = 100.0;
    float rh = 100.0;
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

    Color cell_color = ColorFromHSV(0, 0.0, 0.15);
    Color head_color = ColorFromHSV(200, 0.8, 0.8);
    Color background_color = ColorFromHSV(120, 0.0, 1);

    BeginDrawing();
    animation_update(&p->a, kfs, NOB_ARRAY_LEN(kfs));
    float t = animation_value(p->a, kfs, NOB_ARRAY_LEN(kfs));
    ClearBackground(background_color);
    for (size_t i = 0; i < 20; ++i) {
        DrawRectangle(i*(rw + pad) + t, h/2 - rh/2, rw, rh, cell_color);
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
