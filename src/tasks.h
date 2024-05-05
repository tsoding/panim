#ifndef TASKS_H_
#define TASKS_H_

#include "env.h"
#include "arena.h"

typedef size_t Tag;

typedef struct {
    Tag tag;
    const char *file;
    int line;
    void *data;
} Task;

typedef void (*task_reset_data_t)(void*, Env);
typedef bool (*task_update_data_t)(void*, Env);

typedef struct {
    task_reset_data_t reset;
    task_update_data_t update;
} Task_Funcs;

void task_reset(Task task, Env env);
bool task_update(Task task, Env env);

typedef struct {
    Task_Funcs *items;
    size_t count;
    size_t capacity;
} Task_VTable;

extern Task_VTable task_vtable;
extern Tag TASK_MOVE_SCALAR_TAG;
extern Tag TASK_MOVE_VEC2_TAG;
extern Tag TASK_MOVE_VEC4_TAG;
extern Tag TASK_SEQ_TAG;
extern Tag TASK_GROUP_TAG;
extern Tag TASK_WAIT_TAG;
extern Tag TASK_REPEAT_TAG;

Tag task_vtable_register(Arena *a, Task_Funcs funcs);
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

    float *value;
    float start, target;
} Move_Scalar_Data;

void task_move_scalar_reset(Move_Scalar_Data *data, Env env);
bool task_move_scalar_update(Move_Scalar_Data *data, Env env);
Task task_move_scalar(Arena *a, float *value, float target, float duration);

typedef struct {
    float t;
    bool init;
    float duration;

    Vector2 *value;
    Vector2 start, target;
} Move_Vec2_Data;

void task_move_vec2_reset(Move_Vec2_Data *data, Env env);
bool task_move_vec2_update(Move_Vec2_Data *data, Env env);
Task task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration);

typedef struct {
    float t;
    bool init;
    float duration;

    Vector4 *value;
    Vector4 start, target;
} Move_Vec4_Data;

void task_move_vec4_reset(Move_Vec4_Data *data, Env env);
bool task_move_vec4_update(Move_Vec4_Data *data, Env env);
Task task_move_vec4(Arena *a, Vector4 *value, Color target, float duration);

typedef struct {
    Tasks tasks;
} Group_Data;

void task_group_reset(Group_Data *data, Env env);
bool task_group_update(Group_Data *data, Env env);
Task task_group_(Arena *a, ...);
#define task_group(...) task_group_(__VA_ARGS__, (Task){0})

typedef struct {
    Tasks tasks;
    size_t it;
} Seq_Data;

void task_seq_reset(Seq_Data *data, Env env);
bool task_seq_update(Seq_Data *data, Env env);
Task task_seq_(Arena *a, ...);
#define task_seq(...) task_seq_(__VA_ARGS__, (Task){0})

typedef struct {
    float t;
    float duration;
} Wait_Data;

void task_wait_reset(Wait_Data *data, Env env);
bool task_wait_update(Wait_Data *data, Env env);
Task task_wait(Arena *a, float duration);

typedef struct {
    size_t i;
    size_t times;
    Task inner;
} Repeat_Data;

void task_repeat_reset(Repeat_Data *data, Env env);
bool task_repeat_update(Repeat_Data *data, Env env);
Task task_repeat(Arena *a, size_t times, Task inner);

#endif // TASKS_H_
