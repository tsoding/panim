#ifndef PLUG_H_
#define PLUG_H_

#include "env.h"

// void plug_init(void)
// void *plug_pre_reload(void)
// void plug_post_reload(void *state)
// void plug_update(Env env)
// void plug_reset(void)
// bool plug_finished(void)

#define LIST_OF_PLUGS \
    PLUG(plug_init, void, void)         /* Initialize the plugin */ \
    PLUG(plug_pre_reload, void*, void)  /* Notify the plugin that it's about to get reloaded */ \
    PLUG(plug_post_reload, void, void*) /* Notify the plugin that it got reloaded */ \
    PLUG(plug_update, void, Env)        /* Render next frame of the animation */ \
    PLUG(plug_reset, void, void)        /* Reset the state of the animation */ \
    PLUG(plug_finished, bool, void)     /* Check if the animation is finished */ \

#endif // PLUG_H_
