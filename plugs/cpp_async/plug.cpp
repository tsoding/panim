#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <coroutine>
#include <vector>
#include <memory>

#include <raylib.h>
#include <raymath.h>
#include "env.h"
#include "interpolators.h"
#include "plug.h"

#define SQUARE_SIZE 20

#define AWAIT(task) do { \
    auto __t = (task); \
    while (!__t.done()) { __t(); co_yield 0; } \
  } while (0)

struct promise;

struct Task : std::coroutine_handle<promise>
{
    using promise_type = ::promise;
};

struct promise
{
    Task get_return_object() { return {Task::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
    std::suspend_always yield_value(int /* dummy */)
    {
        return {};
    }
};

#define FONT_SIZE 68

typedef struct {
    size_t size;
    Task task;
    Vector2 position1;
    Vector2 position2;
    Vector2 position3;
} Plug;

static Plug *p;

static void load_assets(void)
{
}

static void unload_assets(void)
{
}

static Task move(Vector2 &position, Vector2 direction, size_t n)
{
  for (size_t i = 0; i < n; ++i)
  {
    position = Vector2Add(position, direction);
    co_yield 0;
  }
}

static Task move2(Vector2 &position, Vector2 direction1, size_t n1, Vector2 direction2, size_t n2)
{
  AWAIT(move(position, direction1, n1));
  printf("move 1 done\n");
  AWAIT(move(position, direction2, n2));
}

static Task combine(std::vector<Task> tasks)
{
  while (true)
  {
    bool ran = false;
    for (auto &task : tasks)
    {
      if (!task.done())
      {
        ran = true;
        task();
      }
    }
    if (!ran)
      break;
    co_yield 0;
  }
}

extern "C" {

#define PLUG(name, ret, ...) ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

void plug_reset(void)
{
    p->position1 = {0, 0};
    p->position2 = {300, 0};
    p->position3 = {100, 300};
    p->task = combine({ move(p->position1, {0.1, 0}, 300), move(p->position2, {0, 0.1}, 300), move2(p->position3, {0, 0.5}, 100, {0.5, 0}, 200) });
}

void plug_init(void)
{
    p = (Plug*)malloc(sizeof(*p));
    assert(p != NULL);
    memset(p, 0, sizeof(*p));
    p->size = sizeof(*p);

    load_assets();
    plug_reset();
}

void *plug_pre_reload(void)
{
    unload_assets();
    return p;
}

void plug_post_reload(void *state)
{
    p = (Plug*)state;
    if (p->size < sizeof(*p)) {
        TraceLog(LOG_INFO, "Migrating plug state schema %zu bytes -> %zu bytes", p->size, sizeof(*p));
        p = (Plug*)realloc(p, sizeof(*p));
        p->size = sizeof(*p);
    }

    load_assets();
}

void plug_update(Env env)
{
    if (!p->task.done())
      p->task();

    Color background_color = ColorFromHSV(0, 0, 0.05);
    Color foreground_color = ColorFromHSV(0, 0, 0.95);

    ClearBackground(background_color);

    Rectangle boundary = {
        .x = p->position1.x,
        .y = p->position1.y,
        .width = SQUARE_SIZE,
        .height = SQUARE_SIZE,
    };
    DrawRectangleRec(boundary, foreground_color);
    boundary = {
        .x = p->position2.x,
        .y = p->position2.y,
        .width = SQUARE_SIZE,
        .height = SQUARE_SIZE,
    };
    DrawRectangleRec(boundary, foreground_color);
    boundary = {
        .x = p->position3.x,
        .y = p->position3.y,
        .width = SQUARE_SIZE,
        .height = SQUARE_SIZE,
    };
    DrawRectangleRec(boundary, foreground_color);
}

bool plug_finished(void)
{
    return p->task.done();
}

}
