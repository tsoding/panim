#ifndef ENV_H_
#define ENV_H_

#include <stdbool.h>
#include <raylib.h>

typedef struct {
    float delta_time;
    float screen_width;
    float screen_height;
    bool rendering;
    void (*play_sound)(Sound sound, Wave wave);
} Env;

#endif // ENV_H_
