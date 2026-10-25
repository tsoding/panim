#include "tasks.h"
#include "interpolators.h"

#include "raymath.h"

bool task_update(Task task, Env env)
{
    return task.vtable->update(task.data, env);
}

bool wait_done(Wait_Data *data)
{
    return data->cursor >= data->duration;
}

float wait_interp(Wait_Data *data)
{
    float t = 0.0f;
    if (data->duration > 0) {
        t = data->cursor/data->duration;
    }
    return t;
}

bool wait_update(Wait_Data *data, Env env)
{
    if (wait_done(data)) return true;
    if (!data->started) data->started = true;
    data->cursor += env.delta_time;
    return wait_done(data);
}

Wait_Data wait_data(float duration)
{
    return (Wait_Data) { .duration = duration };
}

Task_VTable wait_vtable = {
    .update = (task_update_data_t)wait_update,
};

Task task_wait(Arena *a, float duration)
{
    Wait_Data data = wait_data(duration);
    return (Task) {
        .vtable = &wait_vtable,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

bool move_scalar_update(Move_Scalar_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (!data->wait.started && data->value) {
        data->start = *data->value;
    }

    bool finished = wait_update(&data->wait, env);

    if (data->value) {
        *data->value = Lerp(
            data->start,
            data->target,
            interp_func(data->func, wait_interp(&data->wait)));
    }

    return finished;
}

Move_Scalar_Data move_scalar_data(float *value, float target, float duration, Interp_Func func)
{
    return (Move_Scalar_Data) {
        .wait = wait_data(duration),
        .value = value,
        .target = target,
        .func = func,
    };
}

Task_VTable move_scalar_vtable = {
    .update = (task_update_data_t)move_scalar_update,
};

Task task_move_scalar(Arena *a, float *value, float target, float duration, Interp_Func func)
{
    Move_Scalar_Data data = move_scalar_data(value, target, duration, func);
    return (Task) {
        .vtable = &move_scalar_vtable,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

bool move_vec2_update(Move_Vec2_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (!data->wait.started && data->value) {
        data->start = *data->value;
    }

    bool finished = wait_update(&data->wait, env);

    if (data->value) {
        *data->value = Vector2Lerp(
            data->start,
            data->target,
            interp_func(data->func, wait_interp(&data->wait)));
    }
    return finished;
}

Move_Vec2_Data move_vec2_data(Vector2 *value, Vector2 target, float duration, Interp_Func func)
{
    return (Move_Vec2_Data) {
        .wait = wait_data(duration),
        .value = value,
        .target = target,
        .func = func,
    };
}

Task_VTable move_vec2_vtable = {
    .update = (task_update_data_t)move_vec2_update,
};

Task task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration, Interp_Func func)
{
    Move_Vec2_Data data = move_vec2_data(value, target, duration, func);
    return (Task) {
        .vtable = &move_vec2_vtable,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

bool move_vec4_update(Move_Vec4_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (!data->wait.started && data->value) {
        data->start = *data->value;
    }

    bool finished = wait_update(&data->wait, env);

    if (data->value) {
        *data->value = QuaternionLerp(
            data->start,
            data->target,
            interp_func(data->func, wait_interp(&data->wait)));
    }

    return finished;
}

Move_Vec4_Data move_vec4_data(Vector4 *value, Vector4 target, float duration, Interp_Func func)
{
    return (Move_Vec4_Data) {
        .wait = wait_data(duration),
        .value = value,
        .target = target,
        .func = func,
    };
}

Task_VTable move_vec4_vtable = {
    .update = (task_update_data_t)move_vec4_update,
};

Task task_move_vec4(Arena *a, Vector4 *value, Vector4 target, float duration, Interp_Func func)
{
    Move_Vec4_Data data = move_vec4_data(value, target, duration, func);
    return (Task) {
        .vtable = &move_vec4_vtable,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}
bool group_update(Group_Data *data, Env env)
{
    bool finished = true;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task it = data->tasks.items[i];
        if (!task_update(it, env)) {
            finished = false;
        }
    }
    return finished;
}

Task_VTable group_vtable = {
    .update = (task_update_data_t)group_update,
};

Task task_group_(Arena *a, ...)
{
    Group_Data *data = (Group_Data*)arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.data == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .vtable = &group_vtable,
        .data = data,
    };
}

bool seq_update(Seq_Data *data, Env env)
{
    if (data->it >= data->tasks.count) return true;

    Task it = data->tasks.items[data->it];
    if (task_update(it, env)) {
        data->it += 1;
    }

    return data->it >= data->tasks.count;
}

Task_VTable seq_vtable = {
    .update = (task_update_data_t)seq_update,
};

Task task_seq_(Arena *a, ...)
{
    Seq_Data *data = (Seq_Data*)arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.data == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .vtable = &seq_vtable,
        .data = data,
    };
}
