#include <stdio.h>

#include <raylib.h>

#include <dlfcn.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#include "plug.h"
#include "ffmpeg.h"

// #define RENDER_WIDTH 1600
// #define RENDER_HEIGHT 900
// #define RENDER_FPS 30
#define RENDER_WIDTH 1920
#define RENDER_HEIGHT 1080
#define RENDER_FPS 60
#define RENDER_DELTA_TIME (1.0f/RENDER_FPS)

// The state of Panim Engine
static bool paused = false;
static FFMPEG *ffmpeg = NULL;
static RenderTexture2D screen = {0};
static void *libplug = NULL;

static bool reload_libplug(const char *libplug_path)
{
    if (libplug != NULL) {
        dlclose(libplug);
    }

    libplug = dlopen(libplug_path, RTLD_NOW);
    if (libplug == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    #define PLUG(name, ...) \
        name = dlsym(libplug, #name); \
        if (name == NULL) { \
            fprintf(stderr, "ERROR: %s\n", dlerror()); \
            return false; \
        }
    LIST_OF_PLUGS
    #undef PLUG

    return true;
}

static void finish_ffmpeg_rendering(void)
{
    ffmpeg_end_rendering(ffmpeg);
    plug_reset();
    ffmpeg = NULL;
    SetTraceLogLevel(LOG_INFO);
}

int main(int argc, char **argv)
{
    const char *program_name = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        fprintf(stderr, "Usage: %s <libplug.so>\n", program_name);
        fprintf(stderr, "ERROR: no animation dynamic library is provided\n");
        return 1;
    }

    const char *libplug_path = nob_shift_args(&argc, &argv);

    if (!reload_libplug(libplug_path)) return 1;

    float factor = 100.0f;
    InitWindow(16*factor, 9*factor, "Panim");
    InitAudioDevice();
    SetTargetFPS(60);
    plug_init();

    screen = LoadRenderTexture(RENDER_WIDTH, RENDER_HEIGHT);

    while (!WindowShouldClose()) {
        BeginDrawing();
            if (ffmpeg) {
                if (plug_finished()) {
                    finish_ffmpeg_rendering();
                } else {
                    BeginTextureMode(screen);
                    plug_update(RENDER_DELTA_TIME, RENDER_WIDTH, RENDER_HEIGHT, true);
                    EndTextureMode();

                    Image image = LoadImageFromTexture(screen.texture);
                    if (!ffmpeg_send_frame_flipped(ffmpeg, image.data, image.width, image.height)) {
                        finish_ffmpeg_rendering();
                    }
                    UnloadImage(image);
                }
            } else {
                if (IsKeyPressed(KEY_R)) {
                    SetTraceLogLevel(LOG_WARNING);
                    ffmpeg = ffmpeg_start_rendering(RENDER_WIDTH, RENDER_HEIGHT, RENDER_FPS);
                    plug_reset();
                } else {
                    if (IsKeyPressed(KEY_H)) {
                        void *state = plug_pre_reload();
                        reload_libplug(libplug_path);
                        plug_post_reload(state);
                    }

                    if (IsKeyPressed(KEY_SPACE)) {
                        paused = !paused;
                    }

                    if (IsKeyPressed(KEY_Q)) {
                        plug_reset();
                    }

                    plug_update(paused ? 0.0f : GetFrameTime(), GetScreenWidth(), GetScreenHeight(), false);
                }
            }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
