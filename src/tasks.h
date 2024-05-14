#ifndef TASKS_H_
#define TASKS_H_

#include "env.h"
#include "arena.h"
#include "interpolators.h"

typedef size_t Tag;

typedef struct {
    Tag tag;
    void *data;
} Task;

typedef bool (*task_update_data_t)(void*, Env);

typedef struct {
    task_update_data_t update;
} Task_Funcs;

bool task_update(Task task, Env env);

typedef struct {
    Task_Funcs *items;
    size_t count;
    size_t capacity;
} Task_VTable;

extern Task_VTable task_vtable;
extern Tag TASK_WAIT_TAG;
extern Tag TASK_MOVE_SCALAR_TAG;
extern Tag TASK_MOVE_VEC2_TAG;
extern Tag TASK_MOVE_VEC4_TAG;
extern Tag TASK_SEQ_TAG;
extern Tag TASK_GROUP_TAG;

Tag task_vtable_register(Arena *a, Task_Funcs funcs);
void task_vtable_rebuild(Arena *a);

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} Tasks;

typedef struct {
    bool started;
    float cursor;
    float duration;
} Wait_Data;

float wait_interp(Wait_Data *data);
bool wait_done(Wait_Data *data);
bool wait_update(Wait_Data *data, Env env);
Wait_Data wait_data(float duration);
Task task_wait(Arena *a, float duration);

typedef struct {
    Wait_Data wait;
    float *value;
    float start, target;
    Interp_Func func;
} Move_Scalar_Data;

bool move_scalar_update(Move_Scalar_Data *data, Env env);
Move_Scalar_Data move_scalar_data(float *value, float target, float duration, Interp_Func func);
Task task_move_scalar(Arena *a, float *value, float target, float duration, Interp_Func);

typedef struct {
    Wait_Data wait;
    Vector2 *value;
    Vector2 start, target;
    Interp_Func func;
} Move_Vec2_Data;

bool move_vec2_update(Move_Vec2_Data *data, Env env);
Move_Vec2_Data move_vec2_data(Vector2 *value, Vector2 target, float duration, Interp_Func func);
Task task_move_vec2(Arena *a, Vector2 *value, Vector2 target, float duration, Interp_Func func);

typedef struct {
    Wait_Data wait;
    Vector4 *value;
    Vector4 start, target;
    Interp_Func func;
} Move_Vec4_Data;

bool move_vec4_update(Move_Vec4_Data *data, Env env);
Move_Vec4_Data move_vec4_data(Vector4 *value, Vector4 target, float duration, Interp_Func func);
Task task_move_vec4(Arena *a, Vector4 *value, Vector4 target, float duration, Interp_Func func);

typedef struct {
    Tasks tasks;
} Group_Data;

bool group_update(Group_Data *data, Env env);
Task task_group_(Arena *a, ...);
#define task_group(...) task_group_(__VA_ARGS__, (Task){0})

typedef struct {
    Tasks tasks;
    size_t it;
} Seq_Data;

bool seq_update(Seq_Data *data, Env env);
Task task_seq_(Arena *a, ...);
#define task_seq(...) task_seq_(__VA_ARGS__, (Task){0})

#endif // TASKS_H_
