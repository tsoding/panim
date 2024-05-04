#include "tasks.h"
#include "interpolators.h"

#include "raymath.h"

Task_VTable task_vtable = {0};
size_t TASK_MOVE_VEC2_TAG = 0;
size_t TASK_MOVE_VEC4_TAG = 0;
size_t TASK_SEQ_TAG = 0;
size_t TASK_GROUP_TAG = 0;
size_t TASK_WAIT_TAG = 0;
size_t TASK_REPEAT_TAG = 0;

void task_reset(Task *task, Env env)
{
    task_vtable.items[task->tag].reset(task, env);
}

bool task_update(Task *task, Env env)
{
    return task_vtable.items[task->tag].update(task, env);
}

size_t task_vtable_register(Arena *a, Task_Funcs funcs)
{
    size_t index = task_vtable.count;
    arena_da_append(a, &task_vtable, funcs);
    return index;
}

void task_vtable_rebuild(Arena *a)
{
    memset(&task_vtable, 0, sizeof(task_vtable));

    TASK_MOVE_VEC2_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_move_vec2_update,
        .reset = task_move_vec2_reset,
    });
    TASK_MOVE_VEC4_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_move_vec4_update,
        .reset = task_move_vec4_reset,
    });
    TASK_SEQ_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_seq_update,
        .reset = task_seq_reset,
    });
    TASK_GROUP_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_group_update,
        .reset = task_group_reset,
    });
    TASK_WAIT_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_wait_update,
        .reset = task_wait_reset,
    });
    TASK_REPEAT_TAG = task_vtable_register(a, (Task_Funcs) {
        .reset = task_repeat_reset,
        .update = task_repeat_update,
    });
}

void task_move_vec2_reset(Task *task, Env env)
{
    (void) env;
    Move_Vec2_Data *data = (void *)task;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_vec2_update(Task *task, Env env)
{
    Move_Vec2_Data *data = (void*)task;
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

Task *task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration)
{
    Move_Vec2_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_MOVE_VEC2_TAG;
    data->value = value;
    data->target = target;
    data->duration = duration;
    return (void*)data;
}

void task_move_vec4_reset(Task *task, Env env)
{
    (void) env;
    Move_Vec4_Data *data = (void*)task;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_vec4_update(Task *task, Env env)
{
    Move_Vec4_Data *data = (void*)task;
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

Task *task_move_vec4(Arena *a, Vector4 *value, Color target, float duration)
{
    Move_Vec4_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_MOVE_VEC4_TAG;
    data->value = value;
    data->target = ColorNormalize(target);
    data->duration = duration;
    return (void*)data;
}

void task_group_reset(Task *task, Env env)
{
    Group_Data *data = (void*)task;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        task_reset(data->tasks.items[i], env);
    }
}

bool task_group_update(Task *task, Env env)
{
    Group_Data *data = (void*)task;
    bool finished = true;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it =  data->tasks.items[i];
        if (!task_update(it, env)) {
            finished = false;
        }
    }
    return finished;
}

Task *task_group_(Arena *a, ...)
{
    Group_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_GROUP_TAG;

    va_list args;
    va_start(args, a);
    for (;;) {
        Task *task = va_arg(args, Task*);
        if (task == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (void*)data;
}

void task_seq_reset(Task *task, Env env)
{
    (void) env;
    Seq_Data *data = (void*)task;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task *it = data->tasks.items[i];
        task_reset(it, env);
    }
    data->it = 0;
}

bool task_seq_update(Task *task, Env env)
{
    Seq_Data *data = (void*)task;
    if (data->it >= data->tasks.count) return true;

    Task *it = data->tasks.items[data->it];
    if (task_update(it, env)) {
        data->it += 1;
    }

    return data->it >= data->tasks.count;
}

Task *task_seq_(Arena *a, ...)
{
    Seq_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_SEQ_TAG;

    va_list args;
    va_start(args, a);
    for (;;) {
        Task *task = va_arg(args, Task*);
        if (task == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (void*)data;
}

bool task_wait_update(Task *task, Env env)
{
    Wait_Data *data = (void*)task;
    if (data->t >= data->duration) return true;
    data->t += env.delta_time;
    return data->t >= data->duration;
}

void task_wait_reset(Task *task, Env env)
{
    (void) env;
    Wait_Data *data = (void*)task;
    data->t = 0.0f;
}

Task *task_wait(Arena *a, float duration)
{
    Wait_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_WAIT_TAG;
    data->duration = duration;
    return (void*)data;
}

void task_repeat_reset(Task *task, Env env)
{
    (void) env;
    Repeat_Data *data = (void*)task;
    data->i = 0;
}

bool task_repeat_update(Task *task, Env env)
{
    Repeat_Data *data = (void*)task;
    if (data->i >= data->times) return true;

    if (task_update(data->inner, env)) {
        data->i += 1;
        task_reset(data->inner, env);
    }

    return data->i >= data->times;
}

Task *task_repeat(Arena *a, size_t times, Task *inner)
{
    Repeat_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->tag = TASK_REPEAT_TAG;
    data->times = times;
    data->inner = inner;
    return (void*)data;
}
