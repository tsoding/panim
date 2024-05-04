#ifndef TASKS_H_
#define TASKS_H_

#include "env.h"
#include "arena.h"

typedef struct {
    size_t tag;
    void *data;
} Task;

typedef struct {
    void (*reset)(void*, Env);
    bool (*update)(void*, Env);
} Task_Funcs;

void task_reset(Task task, Env env);
bool task_update(Task task, Env env);

typedef struct {
    Task_Funcs *items;
    size_t count;
    size_t capacity;
} Task_VTable;

extern Task_VTable task_vtable;
extern size_t TASK_MOVE_VEC2_TAG;
extern size_t TASK_MOVE_VEC4_TAG;
extern size_t TASK_SEQ_TAG;
extern size_t TASK_GROUP_TAG;
extern size_t TASK_WAIT_TAG;
extern size_t TASK_REPEAT_TAG;

size_t task_vtable_register(Arena *a, Task_Funcs funcs);
void task_vtable_rebuild(Arena *a);

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
} Move_Vec2_Data;

void task_move_vec2_reset(void *raw_data, Env env);
bool task_move_vec2_update(void *raw_data, Env env);
Task task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration);

typedef struct {
    float t;
    bool init;
    float duration;

    Vector4 *value;
    Vector4 start, target;
} Move_Vec4_Data;

void task_move_vec4_reset(void *raw_data, Env env);
bool task_move_vec4_update(void *raw_data, Env env);
Task task_move_vec4(Arena *a, Vector4 *value, Color target, float duration);

typedef struct {
    Tasks tasks;
} Group_Data;

void task_group_reset(void *raw_data, Env env);
bool task_group_update(void *raw_data, Env env);
Task task_group_(Arena *a, ...);
#define task_group(...) task_group_(__VA_ARGS__, (Task){0})

typedef struct {
    Tasks tasks;
    size_t it;
} Seq_Data;

void task_seq_reset(void *raw_data, Env env);
bool task_seq_update(void *raw_data, Env env);
Task task_seq_(Arena *a, ...);
#define task_seq(...) task_seq_(__VA_ARGS__, (Task){0})

typedef struct {
    float t;
    float duration;
} Wait_Data;

void task_wait_reset(void *raw_data, Env env);
bool task_wait_update(void *raw_data, Env env);
Task task_wait(Arena *a, float duration);

typedef struct {
    size_t i;
    size_t times;
    Task inner;
} Repeat_Data;

void task_repeat_reset(void *raw_data, Env env);
bool task_repeat_update(void *raw_data, Env env);
Task task_repeat(Arena *a, size_t times, Task inner);

#endif // TASKS_H_
