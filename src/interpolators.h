#ifndef INTERPOLATORS_H_
#define INTERPOLATORS_H_

#include <math.h>

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
    if (t >= 1.0) return 1.0;
    return sinf(PI*t);
}

#endif // INTERPOLATORS_H_
