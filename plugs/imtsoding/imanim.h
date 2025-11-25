#include <raymath.h>

typedef struct {
    float currentTime; // time since start of the animation, increment with deltaTime every frame
    float clipStartTime; // where the code is in the animation, only updated by a "wait" operation, reset every frame.
    float globEnd; // when the animation ends, updated by clipTime, reset every frame.
    float deltaTime;

} AnimState;

/*
* returns a value between [0.0, 1.0] representing at which point 
* the current time the animation is in.
* a 0.0 return value represents that the current clip hasn't started yet 
* a 1.0 return value represents that the current clip is fully finished
* 
* updates the globEnd value used in wait_for_end
*/
float clipTime(AnimState *a, float duration);

/*
* returns a value between [0.0, 1.0] representing at which point 
* the time the animation was in last frame.
*
* updates the globEnd value used in wait_for_end
*
* this is useful for sound triggers
*
*     if(clipTime(anim, 0.1f) > 0 && prevClipTime(anim, 0.1f) == 0.0)
*         p->env.play_sound(p->kick_sound, p->kick_wave);
*/
float prevClipTime(AnimState *a, float duration);

/*
* advances the clipStartTime 
*/
void anim_wait(AnimState *a, float duration) ;
void wait_for_end(AnimState *a);

/*
* utilities for interpolating floats using clipTime(anim, duration)
* 
* if clipTime would return 0.0f it is a noop
* if clipTime(anim, duration) would return 1.0f does `*src = dst;`
*/
void anim_move_scalar(AnimState *anim, float *src, float dst, float duration);
void anim_move_vec2(AnimState *anim, Vector2 *src, Vector2 dst, float duration);
void anim_move_vec4(AnimState *anim, Vector4 *src, Vector4 dst, float duration);