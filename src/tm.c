#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"
#include "env.h"
#include "interpolators.h"
#include "tasks.h"

#if 1
    #define CELL_COLOR ColorFromHSV(0, 0.0, 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 0.88)
#else
    #define CELL_COLOR ColorFromHSV(0, 0.0, 1 - 0.15)
    #define HEAD_COLOR ColorFromHSV(200, 0.8, 0.8)
    #define BACKGROUND_COLOR ColorFromHSV(120, 0.0, 1 - 0.88)
#endif

#define CELL_WIDTH 200.0f
#define CELL_HEIGHT 200.0f
#define FONT_SIZE (CELL_WIDTH*0.52f)
#define CELL_PAD (CELL_WIDTH*0.15f)
#define START_AT_CELL_INDEX 5
#define HEAD_MOVING_DURATION 0.5f
#define HEAD_WRITING_DURATION 0.2f
#define INTRO_DURATION 1.0f
#define TAPE_SIZE 50

typedef enum {
    DIR_LEFT = -1,
    DIR_RIGHT = 1,
} Direction;

typedef enum {
    IMAGE_EGGPLANT,
    IMAGE_100,
    IMAGE_FIRE,
    IMAGE_JOY,
    IMAGE_OK,
    COUNT_IMAGES,
} Image_Index;

static_assert(COUNT_IMAGES == 5, "Amount of images is updated");
static const char *image_file_paths[COUNT_IMAGES] = {
    [IMAGE_EGGPLANT] = "./assets/images/eggplant.png",
    [IMAGE_100] = "./assets/images/100.png",
    [IMAGE_FIRE] = "./assets/images/fire.png",
    [IMAGE_JOY] = "./assets/images/joy.png",
    [IMAGE_OK] = "./assets/images/ok.png",
};

typedef enum {
    SYMBOL_TEXT,
    SYMBOL_IMAGE,
} Symbol_Kind;

typedef struct {
    Symbol_Kind kind;
    const char *text;
    Image_Index image_index;
} Symbol;

Symbol symbol_text(Arena *a, const char *text)
{
    return (Symbol) {
        .kind = SYMBOL_TEXT,
        .text = arena_strdup(a, text),
    };
}

Symbol symbol_image(Image_Index image_index)
{
    return (Symbol) {
        .kind = SYMBOL_IMAGE,
        .image_index = image_index,
    };
}

typedef enum {
    RULE_STATE = 0,
    RULE_READ,
    RULE_WRITE,
    RULE_STEP,
    RULE_NEXT,
    COUNT_RULE_SYMBOLS,
} Rule_Symbol;

typedef struct {
    Symbol symbols[COUNT_RULE_SYMBOLS];
} Rule;

typedef struct {
    Rule *items;
    size_t count;
    size_t capacity;
} Table;

typedef struct {
    Symbol symbol_a;
    Symbol symbol_b;
    float t;
} Cell;

typedef struct {
    Cell *items;
    size_t count;
    size_t capacity;
} Tape;

typedef struct {
    int index;
    float offset;

    Cell state;
    float state_t;
} Head;

typedef struct {
    size_t size;

    // State (survives the plugin reload, resets on plug_reset)
    Arena arena_state;
    Head head;
    Tape tape;
    float scene_t;
    float tape_y_offset;
    float table_lines_t;
    float table_symbols_t;
    float table_head_t;
    Task task;
    bool finished;

    // Assets (reloads along with the plugin, does not change throughout the animation)
    Arena arena_assets;
    Table table;
    Font font;
    Sound write_sound;
    Wave write_wave;
    Texture2D images[COUNT_IMAGES];
    Tag TASK_INTRO_TAG;
    Tag TASK_MOVE_HEAD_TAG;
    Tag TASK_WRITE_HEAD_TAG;
    Tag TASK_WRITE_ALL_TAG;
} Plug;

static Plug *p = NULL;


typedef struct {
    Wait_Data wait;
    size_t head;
} Intro_Data;

bool task_intro_update(Intro_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;
    if (!data->wait.started) p->head.index = data->head;
    bool finished = wait_update(&data->wait, env);
    p->scene_t = smoothstep(wait_interp(&data->wait));
    return finished;
}

Intro_Data intro_data(size_t head)
{
    return (Intro_Data) {
        .wait = wait_data(INTRO_DURATION),
        .head = head,
    };
}

Task task_intro(Arena *a, size_t head)
{
    Intro_Data data = intro_data(head);
    return (Task) {
        .tag = p->TASK_INTRO_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    Direction dir;
} Move_Head_Data;

bool move_head_update(Move_Head_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (wait_update(&data->wait, env)) {
        p->head.offset = 0.0f;
        p->head.index += data->dir;
        return true;
    }

    p->head.offset = Lerp(0, data->dir, smoothstep(wait_interp(&data->wait)));
    return false;
}

Move_Head_Data move_head(Direction dir)
{
    return (Move_Head_Data) {
        .wait = wait_data(HEAD_MOVING_DURATION),
        .dir = dir,
    };
}

Task task_move_head(Arena *a, Direction dir)
{
    Move_Head_Data data = move_head(dir);
    return (Task) {
        .tag = p->TASK_MOVE_HEAD_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    Symbol write;
} Write_Head_Data;

bool write_head_update(Write_Head_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    Cell *cell = NULL;
    if ((size_t)p->head.index < p->tape.count) {
        cell = &p->tape.items[(size_t)p->head.index];
    }

    if (!data->wait.started && cell) {
        cell->symbol_b = data->write;
        cell->t = 0.0;
    }

    float t1 = wait_interp(&data->wait);
    bool finished = wait_update(&data->wait, env);
    float t2 = wait_interp(&data->wait);

    if (t1 < 0.5 && t2 >= 0.5) {
        env.play_sound(p->write_sound, p->write_wave);
    }

    if (cell) cell->t = smoothstep(t2);

    if (finished && cell) {
        cell->symbol_a = cell->symbol_b;
        cell->t = 0.0;
    }

    return finished;
}

Write_Head_Data write_head_data(Symbol write)
{
    return (Write_Head_Data) {
        .wait = wait_data(HEAD_WRITING_DURATION),
        .write = write,
    };
}

Task task_write_head(Arena *a, Symbol write)
{
    Write_Head_Data data = write_head_data(write);
    return (Task) {
        .tag = p->TASK_WRITE_HEAD_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

typedef struct {
    Wait_Data wait;
    Symbol write;
} Write_All_Data;

bool write_all_update(Write_All_Data *data, Env env)
{
    if (wait_done(&data->wait)) return true;

    if (!data->wait.started) {
        for (size_t i = 0; i < p->tape.count; ++i) {
            p->tape.items[i].t = 0.0f;
            p->tape.items[i].symbol_b = data->write;
        }
    }

    float t1 = wait_interp(&data->wait);
    bool finished = wait_update(&data->wait, env);
    float t2 = wait_interp(&data->wait);

    if (t1 < 0.5 && t2 >= 0.5) {
        env.play_sound(p->write_sound, p->write_wave);
    }

    for (size_t i = 0; i < p->tape.count; ++i) {
        p->tape.items[i].t = smoothstep(t2);
    }

    if (finished) {
        for (size_t i = 0; i < p->tape.count; ++i) {
            p->tape.items[i].t = 0.0f;
            p->tape.items[i].symbol_a = p->tape.items[i].symbol_b;
        }
    }

    return finished;
}

Write_All_Data write_all_data(Symbol write)
{
    return (Write_All_Data) {
        .write = write,
        .wait = wait_data(HEAD_WRITING_DURATION),
    };
}

Task task_write_all(Arena *a, Symbol write)
{
    Write_All_Data data = write_all_data(write);
    return (Task) {
        .tag = p->TASK_WRITE_ALL_TAG,
        .data = arena_memdup(a, &data, sizeof(data)),
    };
}

static void table(Symbol state, Symbol read, Symbol write, Symbol step, Symbol next)
{
    Rule rule = {
        .symbols = {
            [RULE_STATE] = state,
            [RULE_READ] = read,
            [RULE_WRITE] = write,
            [RULE_STEP] = step,
            [RULE_NEXT] = next,
        },
    };
    nob_da_append(&p->table, rule);
}

static void load_assets(void)
{
    Arena *a = &p->arena_assets;
    arena_reset(a);

    int arrows_count = 0;
    int *arrows = LoadCodepoints("?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:)→←", &arrows_count);
    p->font = LoadFontEx("./assets/fonts/iosevka-regular.ttf", FONT_SIZE, arrows, arrows_count);
    UnloadCodepoints(arrows);
    GenTextureMipmaps(&p->font.texture);
    SetTextureFilter(p->font.texture, TEXTURE_FILTER_BILINEAR);

    for (size_t i = 0; i < COUNT_IMAGES; ++i) {
        p->images[i] = LoadTexture(image_file_paths[i]);
        GenTextureMipmaps(&p->images[i]);
        SetTextureFilter(p->images[i], TEXTURE_FILTER_BILINEAR);
    }

    p->write_wave = LoadWave("./assets/sounds/plant-bomb.wav");
    p->write_sound = LoadSoundFromWave(p->write_wave);

    // Table
    {
        table(
            symbol_text(a, "Inc"),
            symbol_text(a, "0"),
            symbol_text(a, "1"),
            symbol_text(a, "→"),
            symbol_text(a, "Halt"));
        table(
            symbol_text(a, "Inc"),
            symbol_text(a, "1"),
            symbol_text(a, "0"),
            symbol_text(a, "→"),
            symbol_text(a, "Inc"));
    }

    task_vtable_rebuild(a);
    p->TASK_INTRO_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)task_intro_update,
    });
    p->TASK_MOVE_HEAD_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)move_head_update,
    });
    p->TASK_WRITE_HEAD_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)write_head_update,
    });
    p->TASK_WRITE_ALL_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = (task_update_data_t)write_all_update,
    });
}

static void unload_assets(void)
{
    UnloadFont(p->font);
    UnloadSound(p->write_sound);
    UnloadWave(p->write_wave);
    for (size_t i = 0; i < COUNT_IMAGES; ++i) {
        UnloadTexture(p->images[i]);
    }
    p->table.count = 0;
}

static Task task_outro(Arena *a, float duration)
{
    return task_group(a,
        task_move_scalar(a, &p->scene_t, 0.0, duration),
        task_move_scalar(a, &p->tape_y_offset, 0.0, duration),
        task_move_scalar(a, &p->table_lines_t, 0.0, duration),
        task_move_scalar(a, &p->table_symbols_t, 0.0, duration),
        task_move_scalar(a, &p->table_head_t, 0.0, duration),
        task_move_scalar(a, &p->head.state_t, 0.0, duration));
}

void plug_reset(void)
{
    Arena *a = &p->arena_state;
    arena_reset(a);

    p->head.index = 0;
    p->head.offset = 0;
    p->head.state_t = 0;
    p->tape.count = 0;
    Symbol zero = symbol_text(a, "0");
    for (size_t i = 0; i < TAPE_SIZE; ++i) {
        Cell cell = {.symbol_a = zero,};
        nob_da_append(&p->tape, cell);
    }

    p->scene_t = 0;
    p->tape_y_offset = 0.0f;
    p->table_lines_t = 0;
    p->table_symbols_t = 0;
    p->table_head_t = 0;

    p->task = task_seq(a,
        task_intro(a, START_AT_CELL_INDEX),
        task_wait(a, 0.75),
        task_move_scalar(a, &p->tape_y_offset, -250.0, 0.5),
        task_wait(a, 0.75),

        task_seq(a,
            task_move_scalar(a, &p->table_lines_t, 1.0, 0.5),
            task_move_scalar(a, &p->table_symbols_t, 1.0, 0.5),
            task_move_scalar(a, &p->head.state_t, 1.0, 0.5),
            task_move_scalar(a, &p->table_head_t, 1.0, 0.5)),

        task_wait(a, 0.75),
        task_write_head(a, symbol_text(a, "1")),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_text(a, "2")),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_text(a, "69")),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_text(a, "420")),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_text(a, ":)")),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_image(IMAGE_JOY)),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_image(IMAGE_FIRE)),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_image(IMAGE_OK)),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_image(IMAGE_100)),
        task_move_head(a, DIR_RIGHT),
        task_write_head(a, symbol_image(IMAGE_EGGPLANT)),
        task_write_all(a, symbol_text(a, "0")),
        task_write_all(a, symbol_text(a, "69")),
        task_write_all(a, symbol_image(IMAGE_EGGPLANT)),
        task_write_all(a, symbol_text(a, "0")),
        task_wait(a, 0.5),
        task_outro(a, INTRO_DURATION),
        task_wait(a, 0.5)
        );
    p->finished = false;
}

void plug_init(void)
{
    p = malloc(sizeof(*p));
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
    p = state;
    if (p->size < sizeof(*p)) {
        TraceLog(LOG_INFO, "Migrating plug state schema %zu bytes -> %zu bytes", p->size, sizeof(*p));
        p = realloc(p, sizeof(*p));
        p->size = sizeof(*p);
    }

    load_assets();
}

static void text_in_rec(Rectangle rec, const char *text, float size, Color color)
{
    Vector2 rec_size = {rec.width, rec.height};
    float font_size = size;
    Vector2 text_size = MeasureTextEx(p->font, text, font_size, 0);
    Vector2 position = { .x = rec.x, .y = rec.y };

    position = Vector2Add(position, Vector2Scale(rec_size, 0.5));
    position = Vector2Subtract(position, Vector2Scale(text_size, 0.5));

    DrawTextEx(p->font, text, position, font_size, 0, color);
}

static void image_in_rec(Rectangle rec, Texture2D image, float size, Color color)
{
    Vector2 rec_size = {rec.width, rec.height};
    Vector2 image_size = {size, size};
    Vector2 position = {rec.x, rec.y};

    position = Vector2Add(position, Vector2Scale(rec_size, 0.5));
    position = Vector2Subtract(position, Vector2Scale(image_size, 0.5));

    Rectangle source = { 0, 0, image.width, image.height };
    Rectangle dest = { position.x, position.y, image_size.x, image_size.y };
    DrawTexturePro(image, source, dest, Vector2Zero(), 0.0, color);
}

static void symbol_in_rec(Rectangle rec, Symbol symbol, float size, Color color)
{
    switch (symbol.kind) {
        case SYMBOL_TEXT: {
            text_in_rec(rec, symbol.text, size, color);
        } break;
        case SYMBOL_IMAGE: {
            image_in_rec(rec, p->images[symbol.image_index], size, WHITE);
        } break;
    }
}

static void interp_symbol_in_rec(Rectangle rec, Symbol from_symbol, Symbol to_symbol, float size, float t, Color color)
{
    symbol_in_rec(rec, from_symbol, size*(1 - t), ColorAlpha(color, 1 - t));
    symbol_in_rec(rec, to_symbol, size*t, ColorAlpha(color, t));
}

static void cell_in_rec(Rectangle rec, Cell cell, Color color)
{
    interp_symbol_in_rec(rec, cell.symbol_a, cell.symbol_b, FONT_SIZE, cell.t, color);
}

static void render_table_lines(float x, float y, float field_width, float field_height, size_t table_columns, size_t table_rows, float t, float thick, Color color)
{
    thick *= t;
    for (size_t i = 0; i < table_rows + 1; ++i) {
        Vector2 start_pos = {
            .x = x - thick/2,
            .y = y + i*field_height,
        };
        Vector2 end_pos = {
            .x = x + field_width*table_columns + thick/2,
            .y = y + i*field_height,
        };
        if (i >= table_rows) {
            Vector2 t = start_pos;
            start_pos = end_pos;
            end_pos = t;
        }
        end_pos = Vector2Lerp(start_pos, end_pos, t);
        DrawLineEx(start_pos, end_pos, thick, color);
    }

    for (size_t i = 0; i < table_columns + 1; ++i) {
        Vector2 start_pos = {
            .x = x + i*field_width,
            .y = y,
        };
        Vector2 end_pos = {
            .x = x + i*field_width,
            .y = y + field_height*table_rows,
        };
        if (i > 0) {
            Vector2 t = start_pos;
            start_pos = end_pos;
            end_pos = t;
        }
        end_pos = Vector2Lerp(start_pos, end_pos, t);
        DrawLineEx(start_pos, end_pos, thick, color);
    }
}

void plug_update(Env env)
{
    ClearBackground(BACKGROUND_COLOR);

    p->finished = task_update(p->task, env);

    float head_thick = 20.0;
    float head_padding = head_thick*2.5;
    Rectangle head_rec = {
        .width = CELL_WIDTH + head_padding,
        .height = CELL_HEIGHT + head_padding,
    };
    float t = ((float)p->head.index + p->head.offset);
    head_rec.x = CELL_WIDTH/2 - head_rec.width/2 + Lerp(-20.0, t, p->scene_t)*(CELL_WIDTH + CELL_PAD);
    head_rec.y = CELL_HEIGHT/2 - head_rec.height/2;
    Camera2D camera = {
        .target = {
            .x = head_rec.x + head_rec.width/2,
            .y = head_rec.y + head_rec.height/2 - p->tape_y_offset,
        },
        .zoom = Lerp(0.5, 1.0, p->scene_t),
        .offset = {
            .x = env.screen_width/2,
            .y = env.screen_height/2,
        },
    };
    BeginMode2D(camera);

        // Tape
        {
            for (size_t i = 0; i < p->tape.count; ++i) {
                Rectangle rec = {
                    .x = i*(CELL_WIDTH + CELL_PAD),
                    .y = 0,
                    .width = CELL_WIDTH,
                    .height = CELL_HEIGHT,
                };
                DrawRectangleRec(rec, CELL_COLOR);
                cell_in_rec(rec, p->tape.items[i], BACKGROUND_COLOR);
            }
        }

        // Head
        {
            Rectangle state_rec = {
                .width = head_rec.width,
                .height = head_rec.height*0.5,
            };
            state_rec.x = head_rec.x,
            state_rec.y = head_rec.y + head_rec.height - state_rec.height*(1 - p->head.state_t),
            // DrawRectangleLinesEx(state_rec, 10, RED);
            symbol_in_rec(state_rec, (Symbol){.kind = SYMBOL_TEXT, .text = "Inc"}, FONT_SIZE*0.75, ColorAlpha(CELL_COLOR, p->head.state_t));
            float h = head_rec.height;
            if (state_rec.y + state_rec.height > head_rec.y + head_rec.height) {
                h += state_rec.y + state_rec.height - (head_rec.y + head_rec.height);
            }
            render_table_lines(head_rec.x, head_rec.y, head_rec.width, h, 1, 1, p->scene_t, head_thick, HEAD_COLOR);
        }

        // Table
        {
            float top_margin = 250.0;
            float right_margin = 70.0;
            float symbol_size = FONT_SIZE*0.75;
            float field_width = 20.0f*9 + CELL_PAD*0.5;
            float field_height = 15.0f*9 + CELL_PAD*0.5;
            float x = head_rec.x + head_rec.width/2 - (field_width*COUNT_RULE_SYMBOLS + right_margin)/2;
            float y = head_rec.y + head_rec.height + top_margin;

            for (size_t i = 0; i < p->table.count; ++i) {
                for (size_t j = 0; j < 2; ++j) {
                    Rectangle rec = {
                        .x = x + j*field_width,
                        .y = y + i*field_height,
                        .width = field_width,
                        .height = field_height,
                    };
                    symbol_in_rec(rec, p->table.items[i].symbols[j], symbol_size*p->table_symbols_t, ColorAlpha(CELL_COLOR, p->table_symbols_t));
                }

                for (size_t j = 2; j < COUNT_RULE_SYMBOLS; ++j) {
                    Rectangle rec = {
                        .x = x + j*field_width + right_margin,
                        .y = y + i*field_height,
                        .width = field_width,
                        .height = field_height,
                    };
                    symbol_in_rec(rec, p->table.items[i].symbols[j], symbol_size*p->table_symbols_t, ColorAlpha(CELL_COLOR, p->table_symbols_t));
                }
            }

            render_table_lines(x, y, field_width, field_height, 2, p->table.count, p->table_lines_t, 7.0f, CELL_COLOR);

            render_table_lines(x + 2*field_width + right_margin, y, field_width, field_height, 3, p->table.count, p->table_lines_t, 7.0f, CELL_COLOR);

            render_table_lines(x - head_padding/2, y - head_padding/2, 2*field_width + head_padding, field_height + head_padding, 1, 1, p->table_head_t, head_thick, HEAD_COLOR);
        }
    EndMode2D();
}

bool plug_finished(void)
{
    return p->finished;
}

#define ARENA_IMPLEMENTATION
#include "arena.h"
