#include <stdio.h>

#include <raylib.h>
#include "plug.h"

#include <dlfcn.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

void *libplug = NULL;

bool reload_libplug(const char *libplug_path)
{
    if (libplug != NULL) {
        dlclose(libplug);
    }

    libplug = dlopen(libplug_path, RTLD_NOW);
    if (libplug == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    plug_init = dlsym(libplug, "plug_init");
    if (plug_init == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    plug_pre_reload = dlsym(libplug, "plug_pre_reload");
    if (plug_pre_reload == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    plug_post_reload = dlsym(libplug, "plug_post_reload");
    if (plug_post_reload == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    plug_update = dlsym(libplug, "plug_update");
    if (plug_update == NULL) {
        fprintf(stderr, "ERROR: %s\n", dlerror());
        return false;
    }

    return true;
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

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_H)) {
            void *state = plug_pre_reload();
            reload_libplug(libplug_path);
            plug_post_reload(state);
        }

        plug_update();
    }

    CloseWindow();

    return 0;
}
