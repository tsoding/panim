#ifndef PLUG_H_
#define PLUG_H_

void (*plug_init)(void) = NULL;
void *(*plug_pre_reload)(void) = NULL;
void (*plug_post_reload)(void*) = NULL;
void (*plug_update)(void) = NULL;

#endif // PLUG_H_
