#include "tasks.h"
#include "interpolators.h"

#include "raymath.h"

void task_move_v2_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V2_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_v2_update(Env env, void *raw_data)
{
    Move_V2_Data *data = raw_data;
    if (data->t >= 1.0f) return true; // task is done

    if (!data->init) {
        // First update of the task
        if (data->value) data->start = *data->value;
        data->init = true;
    }

    data->t = (data->t*data->duration + env.delta_time)/data->duration;
    if (data->value) *data->value = Vector2Lerp(data->start, data->target, smoothstep(data->t));
    return data->t >= 1.0f;
}

Task task_move_v2(Arena *a, Vector2 *value, Vector2 target, float duration)
{
    Move_V2_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = target;
    data->duration = duration;
    return (Task) {
        .reset = task_move_v2_reset,
        .update = task_move_v2_update,
        .data = data,
    };
}

void task_move_v4_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V4_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_v4_update(Env env, void *raw_data)
{
    Move_V4_Data *data = raw_data;
    if (data->t >= 1.0f) return true;

    if (!data->init) {
        // First update of the task
        if (data->value) data->start = *data->value;
        data->init = true;
    }

    data->t = (data->t*data->duration + env.delta_time)/data->duration;
    if (data->value) *data->value = QuaternionLerp(data->start, data->target, smoothstep(data->t));
    return data->t >= 1.0f;
}

Task task_move_v4(Arena *a, Vector4 *value, Color target, float duration)
{
    Move_V4_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = ColorNormalize(target);
    data->duration = duration;
    return (Task) {
        .reset = task_move_v4_reset,
        .update = task_move_v4_update,
        .data = data,
    };
}

void task_group_reset(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        it->reset(env, it->data);
    }
}

bool task_group_update(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    bool finished = true;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        if (!it->update(env, it->data)) {
            finished = false;
        }
    }
    return finished;
}

Task task_group_(Arena *a, ...)
{
    Group_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.update == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .reset = task_group_reset,
        .update = task_group_update,
        .data = data,
    };
}

void task_seq_reset(Env env, void *raw_data)
{
    (void) env;
    Seq_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = &data->tasks.items[i];
        it->reset(env, it->data);
    }
    data->it = 0;
}

bool task_seq_update(Env env, void *raw_data)
{
    Seq_Data *data = raw_data;
    if (data->it >= data->tasks.count) return true;

    Task *task = &data->tasks.items[data->it];
    if (task->update(env, task->data)) {
        data->it += 1;
    }

    return data->it >= data->tasks.count;
}

Task task_seq_(Arena *a, ...)
{
    Seq_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.update == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .update = task_seq_update,
        .reset = task_seq_reset,
        .data = data,
    };
}
