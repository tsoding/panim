#include <stdio.h>
#include <stdint.h>

#include <raylib.h>

#include <dlfcn.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#include "plug.h"
#include "ffmpeg.h"

// #define FFMPEG_VIDEO_WIDTH 1600
// #define FFMPEG_VIDEO_HEIGHT 900
// #define FFMPEG_VIDEO_FPS 30
#define FFMPEG_VIDEO_WIDTH 1920
#define FFMPEG_VIDEO_HEIGHT 1080
#define FFMPEG_VIDEO_FPS 60
#define FFMPEG_VIDEO_DELTA_TIME (1.0f/FFMPEG_VIDEO_FPS)
#define FFMPEG_SOUND_SAMPLE_RATE 44100
#define FFMPEG_SOUND_CHANNELS 2
#define FFMPEG_SOUND_SAMPLE_SIZE_BITS 16
#define FFMPEG_SOUND_SAMPLE_SIZE_BYTES (FFMPEG_SOUND_SAMPLE_SIZE_BITS/8)
// SPF - Samples Per Frame
#define FFMPEG_SOUND_SPF (FFMPEG_SOUND_SAMPLE_RATE/FFMPEG_VIDEO_FPS)

// The state of Panim Engine
static bool paused = false;
static FFMPEG *ffmpeg_video = NULL;
static FFMPEG *ffmpeg_audio = NULL;
static RenderTexture2D screen = {0};
static void *libplug = NULL;
static Wave ffmpeg_wave = {0};
static size_t ffmpeg_wave_cursor = 0;
static uint8_t silence[FFMPEG_SOUND_SPF*FFMPEG_SOUND_SAMPLE_SIZE_BYTES*FFMPEG_SOUND_CHANNELS] = {0};

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

static void finish_ffmpeg_video_rendering(void)
{
    ffmpeg_end_rendering(ffmpeg_video);
    plug_reset();
    ffmpeg_video = NULL;
    SetTraceLogLevel(LOG_INFO);
}

static void finish_ffmpeg_audio_rendering(void)
{
    ffmpeg_end_rendering(ffmpeg_audio);
    plug_reset();
    ffmpeg_audio = NULL;
    SetTraceLogLevel(LOG_INFO);
}

void dummy_play_sound(Sound _sound, Wave _wave)
{
    (void)_sound;
    (void)_wave;
}

void ffmpeg_play_sound(Sound _sound, Wave wave)
{
    (void)_sound;

    if (
        wave.sampleRate != FFMPEG_SOUND_SAMPLE_RATE      ||
        wave.sampleSize != FFMPEG_SOUND_SAMPLE_SIZE_BITS ||
        wave.channels   != FFMPEG_SOUND_CHANNELS
    ) {
        TraceLog(LOG_ERROR,
                 "Animation tried to play sound with rate: %dhz, sample size: %d bits, channels: %d. "
                 "But we only support rate: %dhz, sample size: %d bits, channels: %d for now",
                 wave.sampleRate, wave.sampleSize, wave.channels);
        return;
    }

    ffmpeg_wave = wave;
    ffmpeg_wave_cursor = 0;
}

void preview_play_sound(Sound sound, Wave _wave)
{
    (void)_wave;
    PlaySound(sound);
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
    SetExitKey(KEY_NULL);
    plug_init();

    screen = LoadRenderTexture(FFMPEG_VIDEO_WIDTH, FFMPEG_VIDEO_HEIGHT);

    while (!WindowShouldClose()) {
        BeginDrawing();
            if (ffmpeg_video) {
                if (plug_finished() || IsKeyPressed(KEY_ESCAPE)) {
                    finish_ffmpeg_video_rendering();
                } else {
                    BeginTextureMode(screen);
                    plug_update(CLITERAL(Env) {
                        .screen_width = FFMPEG_VIDEO_WIDTH,
                        .screen_height = FFMPEG_VIDEO_HEIGHT,
                        .delta_time = FFMPEG_VIDEO_DELTA_TIME,
                        .rendering = true,
                        .play_sound = dummy_play_sound,
                    });
                    EndTextureMode();

                    Image image = LoadImageFromTexture(screen.texture);
                    if (!ffmpeg_send_frame_flipped(ffmpeg_video, image.data, image.width, image.height)) {
                        finish_ffmpeg_video_rendering();
                    }
                    UnloadImage(image);
                }
            } else if (ffmpeg_audio) {
                if (plug_finished() || IsKeyPressed(KEY_ESCAPE)) {
                    finish_ffmpeg_audio_rendering();
                } else {
                    BeginTextureMode(screen);
                    plug_update(CLITERAL(Env) {
                        .screen_width = FFMPEG_VIDEO_WIDTH,
                        .screen_height = FFMPEG_VIDEO_HEIGHT,
                        .delta_time = FFMPEG_VIDEO_DELTA_TIME,
                        .rendering = true,
                        .play_sound = ffmpeg_play_sound,
                    });
                    EndTextureMode();

                    size_t frame_count = ffmpeg_wave.frameCount;
                    size_t frame_size = FFMPEG_SOUND_SAMPLE_SIZE_BYTES*FFMPEG_SOUND_CHANNELS;
                    size_t frames_begin = ffmpeg_wave_cursor;
                    size_t frames_end = ffmpeg_wave_cursor + FFMPEG_SOUND_SPF;
                    if (frames_end > frame_count) {
                        frames_end = frame_count;
                    }
                    void *sound_data = (uint8_t*)ffmpeg_wave.data + frames_begin*frame_size;
                    size_t sound_size = (frames_end - frames_begin)*frame_size;
                    if (!ffmpeg_send_sound_samples(ffmpeg_audio, sound_data, sound_size)) {
                        finish_ffmpeg_audio_rendering();
                    }
                    ffmpeg_wave_cursor += frames_end - frames_begin;
                    size_t silence_size = (FFMPEG_SOUND_SPF - (frames_end - frames_begin))*frame_size;
                    if (!ffmpeg_send_sound_samples(ffmpeg_audio, silence, silence_size)) {
                        finish_ffmpeg_audio_rendering();
                    }
                }
            } else {
                if (IsKeyPressed(KEY_R)) {
                    SetTraceLogLevel(LOG_WARNING);
                    ffmpeg_video = ffmpeg_start_rendering_video("output.mp4", FFMPEG_VIDEO_WIDTH, FFMPEG_VIDEO_HEIGHT, FFMPEG_VIDEO_FPS);
                    plug_reset();
                } else if (IsKeyPressed(KEY_T)) {
                    SetTraceLogLevel(LOG_WARNING);
                    ffmpeg_audio = ffmpeg_start_rendering_audio("output.wav");
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

                    plug_update(CLITERAL(Env) {
                        .screen_width = GetScreenWidth(),
                        .screen_height = GetScreenHeight(),
                        .delta_time = paused ? 0.0f : GetFrameTime(),
                        .rendering = false,
                        .play_sound = preview_play_sound,
                    });
                }
            }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
