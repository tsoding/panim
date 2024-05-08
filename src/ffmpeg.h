#ifndef FFMPEG_H_
#define FFMPEG_H_

#include <stddef.h>
#include <stdbool.h>

typedef struct FFMPEG FFMPEG;

FFMPEG *ffmpeg_start_rendering_video(const char *output_path, size_t width, size_t height, size_t fps);
FFMPEG *ffmpeg_start_rendering_audio(const char *output_path);
bool ffmpeg_send_frame_flipped(FFMPEG *ffmpeg, void *data, size_t width, size_t height);
bool ffmpeg_send_sound_samples(FFMPEG *ffmpeg, void *data, size_t size);
bool ffmpeg_end_rendering(FFMPEG *ffmpeg, bool cancel);

#endif // FFMPEG_H_
