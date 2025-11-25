#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

// Folder must with with forward slash /
#define BUILD_DIR "./build/"
#define PANIM_DIR "./panim/"
#define PLUGS_DIR "./plugs/"
#define THIRDPARTY_DIR "./thirdparty/"
#define RAYLIB_DIR THIRDPARTY_DIR"raylib-5.0_linux_amd64/"

Cmd cmd = {0};
Procs procs = {0};

void cflags(void)
{
    cmd_append(&cmd, "-Wall");
    cmd_append(&cmd, "-Wextra");
    cmd_append(&cmd, "-Wno-missing-field-initializers");
    cmd_append(&cmd, "-Wno-unused-function");
    cmd_append(&cmd, "-ggdb");
    cmd_append(&cmd, "-I"RAYLIB_DIR"include");
    cmd_append(&cmd, "-I"PANIM_DIR);
    cmd_append(&cmd, "-I.");
}

void cc(void)
{
    cmd_append(&cmd, "cc");
    cflags();
}

void cxx(void)
{
    cmd_append(&cmd, "g++");
    cmd_append(&cmd, "-Wno-missing-field-initializers"); // Very common warning when compiling raymath.h as C++
    cflags();
}

void libs(void)
{
    cmd_append(&cmd, "-Wl,-rpath="RAYLIB_DIR"lib/");
    cmd_append(&cmd, "-Wl,-rpath="PANIM_DIR);
    cmd_append(&cmd, "-L"RAYLIB_DIR"lib");
    cmd_append(&cmd, "-l:libraylib.so", "-lm", "-ldl", "-lpthread");
}

bool build_plug_c(bool force, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cc();
        cmd_append(&cmd, "-fPIC", "-shared", "-Wl,--no-undefined");
        cmd_append(&cmd, "-o", output_path);
        cmd_append(&cmd, source_path);
        libs();
        return cmd_run(&cmd, .async = &procs);
    }

    nob_log(INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_plug_cxx(bool force, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cxx();
        cmd_append(&cmd, "-fPIC", "-shared", "-Wl,--no-undefined");
        cmd_append(&cmd, "-o", output_path);
        cmd_append(&cmd, source_path);
        libs();
        return cmd_run(&cmd, .async = &procs);
    }

    nob_log(INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_exe(bool force, const char **input_paths, size_t input_paths_len, const char *output_path)
{
    int rebuild_is_needed = needs_rebuild(output_path, input_paths, input_paths_len);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cc();
        cmd_append(&cmd, "-o", output_path);
        da_append_many(&cmd, input_paths, input_paths_len);
        libs();
        return cmd_run(&cmd, .async = &procs);
    }

    nob_log(INFO, "%s is up-to-date", output_path);
    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program_name = shift(argv, argc);
    (void) program_name;

    bool force = false;
    while (argc > 0) {
        const char *flag = shift(argv, argc);
        if (strcmp(flag, "-f") == 0 || strcmp(flag, "-B") == 0) {
            force = true;
        } else {
            nob_log(ERROR, "Unknown flag %s", flag);
            return 1;
        }
    }

    if (!mkdir_if_not_exists(BUILD_DIR)) return 1;

    if (!build_plug_c(force, PLUGS_DIR"tm/plug.c", BUILD_DIR"libtm.so")) return 1;
    if (!build_plug_c(force, PLUGS_DIR"template/plug.c", BUILD_DIR"libtemplate.so")) return 1;
    if (!build_plug_c(force, PLUGS_DIR"squares/plug.c", BUILD_DIR"libsquare.so")) return 1;
    if (!build_plug_c(force, PLUGS_DIR"tsoding/plug.c", BUILD_DIR"libtsoding.so")) return 1;
    if (!build_plug_c(force, PLUGS_DIR"imtsoding/plug.c", BUILD_DIR"libimtsoding.so")) return 1;
    if (!build_plug_c(force, PLUGS_DIR"bezier/plug.c", BUILD_DIR"libbezier.so")) return 1;
    if (!build_plug_cxx(force, PLUGS_DIR"cpp/plug.cpp", BUILD_DIR"libcpp.so")) return 1;

    {
        const char *output_path = BUILD_DIR"panim";
        const char *input_paths[] = {
            PANIM_DIR"panim.c",
            PANIM_DIR"ffmpeg_linux.c"
        };
        size_t input_paths_len = ARRAY_LEN(input_paths);
        if (!build_exe(force, input_paths, input_paths_len, output_path)) return 1;
    }

    if (!procs_flush(&procs)) return 1;

    return 0;
}
