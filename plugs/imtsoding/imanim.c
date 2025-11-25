

float clipTime(AnimState *a, float duration) {
    float timeSinceStart = a->currentTime - a->clipStartTime;
    float animT = timeSinceStart / duration;
    if(animT < 0) //before the clip requested
        animT = 0.f;
    if(animT > 1) //after the clip requested
        animT = 1.f;

    if(a->globEnd < (a->clipStartTime + duration))
        a->globEnd = a->clipStartTime + duration;

    return animT;
}
float prevClipTime(AnimState *a, float duration) {
    float timeSinceStart = a->currentTime - a->deltaTime - a->clipStartTime;
    float animT = timeSinceStart / duration;
    if(animT < 0) //before the clip requested
        animT = 0.f;
    if(animT > 1) //after the clip requested
        animT = 1.f;

    if(a->globEnd < (a->clipStartTime + duration))
        a->globEnd = a->clipStartTime + duration;

    return animT;
}

void anim_wait(AnimState *a, float duration) {
    if(a->globEnd < (a->clipStartTime + duration))
        a->globEnd = a->clipStartTime + duration;
    a->clipStartTime = a->globEnd;
}

void wait_for_end(AnimState *a){
	a->clipStartTime = a->globEnd;
}

void anim_move_scalar(AnimState *anim, float *src, float dst, float duration)
{
    float t = clipTime(anim, duration);
    if(t<=0.0f) return;
    if(t>=1.0f) {
        *src = dst; return;
    }
    *src = Lerp(*src, dst, t);
}

void anim_move_vec2(AnimState *anim, Vector2 *src, Vector2 dst, float duration)
{
    float t = clipTime(anim, duration);
    if(t<=0.0f) return;
    if(t>=1.0f) {
        *src = dst; return;
    }
    *src = Vector2Lerp(*src, dst, t);
}

void anim_move_vec4(AnimState *anim, Vector4 *src, Vector4 dst, float duration)
{
    float t = clipTime(anim, duration);
    if(t<=0.0f) return;
    if(t>=1.0f) {
        *src = dst; return;
    }
    *src = Vector4Lerp(*src, dst, t);
}