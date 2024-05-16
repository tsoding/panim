#ifndef INTERPOLATORS_H_
#define INTERPOLATORS_H_

#include <assert.h>
#include <math.h>
#include <raymath.h>

typedef enum {
    FUNC_SINSTEP,
    FUNC_SMOOTHSTEP,
    FUNC_SINPULSE,
} Interp_Func;

static inline float smoothstep(float x)
{
    if (x < 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return 3*x*x - 2*x*x*x;
}

static inline float sinstep(float t)
{
    if (t < 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return (sinf(PI*t - PI*0.5) + 1)*0.5;
}

static inline float sinpulse(float t)
{
    if (t < 0.0) return 0.0;
    if (t >= 1.0) return 0.0;
    return sinf(PI*t);
}

static inline Vector2 cubic_bezier(float t, Vector2 nodes[4])
{
    float it = 1 - t;
    Vector2 b = Vector2Scale(nodes[0], it*it*it);
    b = Vector2Add(b, Vector2Scale(nodes[1], 3*it*it*t));
    b = Vector2Add(b, Vector2Scale(nodes[2], 3*it*t*t));
    b = Vector2Add(b, Vector2Scale(nodes[3], t*t*t));
    return b;
}

static inline float interp_func(Interp_Func func, float t)
{
    switch (func) {
    case FUNC_SINSTEP:    return sinstep(t);
    case FUNC_SMOOTHSTEP: return smoothstep(t);
    case FUNC_SINPULSE:   return sinpulse(t);
    }
    assert(0 && "UNREACHABLE");
    return 0.0f;
}

#endif // INTERPOLATORS_H_
