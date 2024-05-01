#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>
#include "env.h"

#define FONT_SIZE 68

typedef struct {
    size_t size;
    Font font;
} Plug;

static Plug *p;

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
    Color background_color = ColorFromHSV(0, 0, 0.05);
    Color foreground_color = ColorFromHSV(0, 0, 0.95);

    ClearBackground(background_color);

    const char *text = "Panim Template";
    Vector2 text_size = MeasureTextEx(p->font, text, FONT_SIZE, 0);

    Vector2 position = {env.screen_width/2, env.screen_height/2};
    position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));
    DrawTextEx(p->font, text, position, FONT_SIZE, 0, foreground_color);
}

bool plug_finished(void)
{
    return true;
}
