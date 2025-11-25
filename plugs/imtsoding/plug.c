#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "plug.h"
#define NOB_STRIP_PREFIX
#include "nob.h"
#include "imanim.h"

#define PLUG(name, ret, ...) ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#define FONT_SIZE 68

typedef struct {
    size_t size;
    Font font;
    float radius;
    float roundness;
    float alpha;
    float rotation;
    AnimState anim;
    Env env;
    Sound kick_sound;
    Wave kick_wave;
    bool finished;
} Plug;

static Plug *p;

static void load_assets(void)
{
    p->font = LoadFontEx("./assets/fonts/Vollkorn-Regular.ttf", FONT_SIZE, NULL, 0);
    p->kick_wave = LoadWave("./assets/sounds/kick.wav");
    p->kick_sound = LoadSoundFromWave(p->kick_wave);
}

static void unload_assets(void)
{
    UnloadFont(p->font);
    UnloadSound(p->kick_sound);
    UnloadWave(p->kick_wave);
}


#define co_tween(anim, duration, ...) co_tween_impl((anim), (duration), __VA_ARGS__, NULL)


void co_tween_impl(AnimState *anim, float duration, ...)
{
    va_list args;
    va_start(args, duration);
    float interp;
    float *x = va_arg(args, float*);
    while (x != NULL) {
        float a = va_arg(args, double);
        float b = va_arg(args, double);
        anim_move_scalar(anim, x, b, duration);
        x = va_arg(args, float*);
    }
    va_end(args);
    wait_for_end(anim);
}

void co_sleep(AnimState *anim, float duration)
{
    anim_wait(anim, duration) ;
}

void animation(AnimState *anim, void *data)
{
    UNUSED(data);

    p->radius = 0;
    p->roundness = 0;
    p->alpha = 0;
    p->rotation = 0;

    float duration = 0.15f;
    float sleep = 0.25f;
    float curr = clipTime(anim, 0.1f);
    float prev = prevClipTime(anim, 0.1f);
    if(curr > 0 && prev == 0.0)
        p->env.play_sound(p->kick_sound, p->kick_wave);
    co_tween(anim, duration, &p->radius, 0.f, 1.f);
    co_sleep(anim, sleep);
    if(clipTime(anim, 0.1f) > 0 && prevClipTime(anim, 0.1f) == 0.0)
        p->env.play_sound(p->kick_sound, p->kick_wave);
    co_tween(anim, duration, &p->roundness, 0.f, 1.f);
    co_sleep(anim, sleep);
    if(clipTime(anim, 0.1f) > 0 && prevClipTime(anim, 0.1f) == 0.0)
        p->env.play_sound(p->kick_sound, p->kick_wave);
    co_tween(anim, duration,
         &p->alpha,     0.f, 1.f,
         &p->roundness, 1.f, 0.f,
         &p->rotation,  0.f, 1.f);
    co_sleep(anim, sleep);
    if(clipTime(anim, 0.1f) > 0 && prevClipTime(anim, 0.1f) == 0.0)
        p->env.play_sound(p->kick_sound, p->kick_wave);
    co_tween(anim, duration, &p->radius, 1.f, 0.f);
    co_sleep(anim, 2.0);
}

void plug_reset(void)
{
    p->anim.currentTime = 0.f;
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
    p->env = env;
    
    p->anim.clipStartTime = 0;
    p->anim.globEnd = 0;
    p->anim.currentTime += env.delta_time;
    p->anim.deltaTime = env.delta_time;

    animation(&p->anim, NULL);

    p->finished = p->anim.currentTime >= p->anim.clipStartTime;

    Color background_color = GetColor(0x181818FF);
    Color green_color      = GetColor(0x73C936FF);
    Color red_color        = GetColor(0xF43841FF);

    ClearBackground(background_color);

    Camera2D camera = {
        .zoom = 1.0,
        .rotation = 45.*p->rotation,
        .offset = {env.screen_width/2, env.screen_height/2},
    };

    BeginMode2D(camera); {
        float size = 300*p->radius;
        Rectangle rec = {
            .x          = -size/2,
            .y          = -size/2,
            .width      = size,
            .height     = size,
        };
        

        // // Square
        Color color = ColorAlphaBlend(green_color, ColorAlpha(red_color, p->alpha), WHITE);
        DrawRectangleRounded(rec, p->roundness, 30, color);

        // const char *text = "Tsoding Animation";
        // Vector2 text_size = MeasureTextEx(p->font, text, FONT_SIZE, 0);
        // Vector2 position = Vector2Scale(text_size, -0.5);
        // DrawTextEx(p->font, text, position, FONT_SIZE, 0, foreground_color);
    } EndMode2D();
}

bool plug_finished(void)
{
    return p->finished;
}

#include "imanim.c"
