#ifndef PLUG_H_
#define PLUG_H_

#define LIST_OF_PLUGS \
    PLUG(plug_init, void*, void)                 /* Initialize the plugin */ \
    PLUG(plug_pre_reload, void*, void)           /* Notify the plugin that it's about to get reloaded */ \
    PLUG(plug_post_reload, void, void*)          /* Notify the plugin that it got reloaded */ \
    PLUG(plug_update, void, float, float, float) /* Render next frame of the animation */ \
    PLUG(plug_reset, void, void)                 /* Reset the state of the animation */ \
    PLUG(plug_finished, bool, void)              /* Check if the animation is finished */ \

#define PLUG(name, ret, ...) ret (*name)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#endif // PLUG_H_
