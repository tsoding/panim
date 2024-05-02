#ifndef TASKS_H_
#define TASKS_H_

#include "env.h"
#include "arena.h"

typedef struct {
    void (*reset)(Env, void*);
    bool (*update)(Env, void*);
    void *data;
} Task;

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} Tasks;

typedef struct {
    float t;
    bool init;
    float duration;

    Vector2 *value;
    Vector2 start, target;
} Move_V2_Data;

void task_move_v2_reset(Env env, void *raw_data);
bool task_move_v2_update(Env env, void *raw_data);
Task task_move_v2(Arena *a, Vector2 *value, Vector2 target, float duration);

typedef struct {
    float t;
    bool init;
    float duration;

    Vector4 *value;
    Vector4 start, target;
} Move_V4_Data;

void task_move_v4_reset(Env env, void *raw_data);
bool task_move_v4_update(Env env, void *raw_data);
Task task_move_v4(Arena *a, Vector4 *value, Color target, float duration);

typedef struct {
    Tasks tasks;
} Group_Data;

void task_group_reset(Env env, void *raw_data);
bool task_group_update(Env env, void *raw_data);
Task task_group_(Arena *a, ...);
#define task_group(...) task_group_(__VA_ARGS__, (Task){0})

typedef struct {
    Tasks tasks;
    size_t it;
} Seq_Data;

void task_seq_reset(Env env, void *raw_data);
bool task_seq_update(Env env, void *raw_data);
Task task_seq_(Arena *a, ...);
#define task_seq(...) task_seq_(__VA_ARGS__, (Task){0});

#endif // TASKS_H_
