#include "tasks.h"
#include "interpolators.h"

#include "raymath.h"

Task_VTable task_vtable = {0};
Tag TASK_MOVE_SCALAR_TAG = 0;
Tag TASK_MOVE_VEC2_TAG = 0;
Tag TASK_MOVE_VEC4_TAG = 0;
Tag TASK_SEQ_TAG = 0;
Tag TASK_GROUP_TAG = 0;
Tag TASK_WAIT_TAG = 0;
Tag TASK_REPEAT_TAG = 0;

void task_reset(Task task, Env env)
{
    task_vtable.items[task.tag].reset(task.data, env);
}

bool task_update(Task task, Env env)
{
    return task_vtable.items[task.tag].update(task.data, env);
}

Tag task_vtable_register(Arena *a, Task_Funcs funcs)
{
    Tag tag = task_vtable.count;
    arena_da_append(a, &task_vtable, funcs);
    return tag;
}

void task_vtable_rebuild(Arena *a)
{
    memset(&task_vtable, 0, sizeof(task_vtable));

    TASK_MOVE_SCALAR_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_move_scalar_update,
        .reset = (task_reset_data_t)task_move_scalar_reset,
    });
    TASK_MOVE_VEC2_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_move_vec2_update,
        .reset = (task_reset_data_t)task_move_vec2_reset,
    });
    TASK_MOVE_VEC4_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_move_vec4_update,
        .reset = (task_reset_data_t)task_move_vec4_reset,
    });
    TASK_SEQ_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_seq_update,
        .reset = (task_reset_data_t)task_seq_reset,
    });
    TASK_GROUP_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_group_update,
        .reset = (task_reset_data_t)task_group_reset,
    });
    TASK_WAIT_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_wait_update,
        .reset = (task_reset_data_t)task_wait_reset,
    });
    TASK_REPEAT_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_repeat_update,
        .reset = (task_reset_data_t)task_repeat_reset,
    });
}

void task_move_scalar_reset(Move_Scalar_Data *data, Env env)
{
    (void) env;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_scalar_update(Move_Scalar_Data *data, Env env)
{
    if (data->t >= 1.0f) return true; // task is done

    if (!data->init) {
        // First update of the task
        if (data->value) data->start = *data->value;
        data->init = true;
    }

    data->t = (data->t*data->duration + env.delta_time)/data->duration;
    if (data->value) *data->value = Lerp(data->start, data->target, smoothstep(data->t));
    return data->t >= 1.0f;
}

Task task_move_scalar(Arena *a, float *value, float target, float duration)
{
    Move_Scalar_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = target;
    data->duration = duration;
    return (Task) {
        .tag = TASK_MOVE_SCALAR_TAG,
        .data = data,
    };
}

void task_move_vec2_reset(Move_Vec2_Data *data, Env env)
{
    (void) env;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_vec2_update(Move_Vec2_Data *data, Env env)
{
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

Task task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration)
{
    Move_Vec2_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = target;
    data->duration = duration;
    return (Task) {
        .tag = TASK_MOVE_VEC2_TAG,
        .data = data,
    };
}

void task_move_vec4_reset(Move_Vec4_Data *data, Env env)
{
    (void) env;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_vec4_update(Move_Vec4_Data *data, Env env)
{
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

Task task_move_vec4(Arena *a, Vector4 *value, Color target, float duration)
{
    Move_Vec4_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = ColorNormalize(target);
    data->duration = duration;
    return (Task) {
        .tag = TASK_MOVE_VEC4_TAG,
        .data = data,
    };
}

void task_group_reset(Group_Data *data, Env env)
{
    for (size_t i = 0; i < data->tasks.count; ++i) {
        task_reset(data->tasks.items[i], env);
    }
}

bool task_group_update(Group_Data *data, Env env)
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

Task task_group_(Arena *a, ...)
{
    Group_Data *data = arena_alloc(a, sizeof(*data));
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
        .tag = TASK_GROUP_TAG,
        .data = data,
    };
}

void task_seq_reset(Seq_Data *data, Env env)
{
    (void) env;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task it = data->tasks.items[i];
        task_reset(it, env);
    }
    data->it = 0;
}

bool task_seq_update(Seq_Data *data, Env env)
{
    if (data->it >= data->tasks.count) return true;

    Task it = data->tasks.items[data->it];
    if (task_update(it, env)) {
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
        if (task.data == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .tag = TASK_SEQ_TAG,
        .data = data,
    };
}

bool task_wait_update(Wait_Data *data, Env env)
{
    if (data->t >= data->duration) return true;
    data->t += env.delta_time;
    return data->t >= data->duration;
}

void task_wait_reset(Wait_Data *data, Env env)
{
    (void) env;
    data->t = 0.0f;
}

Task task_wait(Arena *a, float duration)
{
    Wait_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->duration = duration;
    return (Task) {
        .tag = TASK_WAIT_TAG,
        .data = data,
    };
}

void task_repeat_reset(Repeat_Data *data, Env env)
{
    (void) env;
    data->i = 0;
}

bool task_repeat_update(Repeat_Data *data, Env env)
{
    if (data->i >= data->times) return true;

    if (task_update(data->inner, env)) {
        data->i += 1;
        task_reset(data->inner, env);
    }

    return data->i >= data->times;
}

Task task_repeat(Arena *a, size_t times, Task inner)
{
    Repeat_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->times = times;
    data->inner = inner;
    return (Task) {
        .tag = TASK_REPEAT_TAG,
        .data = data,
    };
}
