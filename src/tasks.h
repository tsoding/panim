#ifndef TASKS_H_
#define TASKS_H_

#include "env.h"
#include "arena.h"

typedef struct {
    size_t tag;
    void *data;
} Task;

typedef struct {
    void (*reset)(Env, void*);
    bool (*update)(Env, void*);
} Task_Funcs;

void task_reset(Task task, Env env);
bool task_update(Task task, Env env);

typedef struct {
    Task_Funcs *items;
    size_t count;
    size_t capacity;
} Task_VTable;

extern Task_VTable task_vtable;
extern size_t TASK_MOVE_V2_TAG;
extern size_t TASK_MOVE_V4_TAG;
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
#define task_seq(...) task_seq_(__VA_ARGS__, (Task){0})

typedef struct {
    float t;
    float duration;
} Wait_Data;

bool task_wait_update(Env env, void *raw_data);
void task_wait_reset(Env env, void *raw_data);
Task task_wait(Arena *a, float duration);

typedef struct {
    size_t i;
    size_t times;
    Task inner;
} Repeat_Data;

void task_repeat_reset(Env env, void *raw_data);
bool task_repeat_update(Env env, void *raw_data);
Task task_repeat(Arena *a, size_t times, Task inner);

#endif // TASKS_H_
