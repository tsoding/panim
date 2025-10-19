#define NOB_IMPLEMENTATION
#include "nob.h"

// Folder must with with forward slash /
#define BUILD_DIR "./build/"
#define PANIM_DIR "./panim/"
#define PLUGS_DIR "./plugs/"

void cflags(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(cmd, "-I./raylib/raylib-5.0_linux_amd64/include");
    nob_cmd_append(cmd, "-I"PANIM_DIR);
    nob_cmd_append(cmd, "-I.");
}

void cc(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "cc");
    cflags(cmd);
}

void cxx(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "g++");
    nob_cmd_append(cmd, "-Wno-missing-field-initializers"); // Very common warning when compiling raymath.h as C++
    cflags(cmd);
}

void libs(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "-Wl,-rpath=./raylib/raylib-5.0_linux_amd64/lib/");
    nob_cmd_append(cmd, "-Wl,-rpath="PANIM_DIR);
    nob_cmd_append(cmd, "-L./raylib/raylib-5.0_linux_amd64/lib");
    nob_cmd_append(cmd, "-l:libraylib.so", "-lm", "-ldl", "-lpthread");
}

bool build_plug_c3(bool force, Nob_Cmd *cmd, const char *output_path, const char **source_paths, size_t source_paths_count)
{
    int rebuild_is_needed = nob_needs_rebuild(nob_temp_sprintf("%s.so", output_path), source_paths, source_paths_count);
    if (rebuild_is_needed < 0) return false;
    if (force || rebuild_is_needed) {
        // TODO: check if c3c compile even exists
        // otherwise this is not buildable
        nob_cmd_append(cmd, "c3c", "dynamic-lib", "-o", output_path);
        nob_da_append_many(cmd, source_paths, source_paths_count);
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }

    return true;
}

bool build_plug_c(bool force, Nob_Cmd *cmd, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cc(cmd);
        nob_cmd_append(cmd, "-fPIC", "-shared", "-Wl,--no-undefined");
        nob_cmd_append(cmd, "-o", output_path);
        nob_cmd_append(cmd, source_path);
        libs(cmd);
        return nob_cmd_run_sync_and_reset(cmd);
    }

    nob_log(NOB_INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_plug_cxx(bool force, Nob_Cmd *cmd, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cxx(cmd);
        nob_cmd_append(cmd, "-fPIC", "-shared", "-Wl,--no-undefined", "-fcoroutines", "-std=c++20");
        nob_cmd_append(cmd, "-o", output_path);
        nob_cmd_append(cmd, source_path);
        libs(cmd);
        return nob_cmd_run_sync_and_reset(cmd);
    }

    nob_log(NOB_INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_exe(bool force, Nob_Cmd *cmd, const char **input_paths, size_t input_paths_len, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild(output_path, input_paths, input_paths_len);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cc(cmd);
        nob_cmd_append(cmd, "-o", output_path);
        nob_da_append_many(cmd, input_paths, input_paths_len);
        libs(cmd);
        return nob_cmd_run_sync_and_reset(cmd);
    }

    nob_log(NOB_INFO, "%s is up-to-date", output_path);
    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program_name = nob_shift_args(&argc, &argv);
    (void) program_name;

    bool force = false;
    while (argc > 0) {
        const char *flag = nob_shift_args(&argc, &argv);
        if (strcmp(flag, "-f") == 0) {
            force = true;
        } else {
            nob_log(NOB_ERROR, "Unknown flag %s", flag);
            return 1;
        }
    }

    if (!nob_mkdir_if_not_exists(BUILD_DIR)) return 1;

    Nob_Cmd cmd = {0};
    if (!build_plug_c(force, &cmd, PLUGS_DIR"tm/plug.c", BUILD_DIR"libtm.so")) return 1;
    if (!build_plug_c(force, &cmd, PLUGS_DIR"template/plug.c", BUILD_DIR"libtemplate.so")) return 1;
    if (!build_plug_c(force, &cmd, PLUGS_DIR"squares/plug.c", BUILD_DIR"libsquare.so")) return 1;
    if (!build_plug_c(force, &cmd, PLUGS_DIR"bezier/plug.c", BUILD_DIR"libbezier.so")) return 1;
    if (!build_plug_cxx(force, &cmd, PLUGS_DIR"cpp/plug.cpp", BUILD_DIR"libcpp.so")) return 1;
    if (!build_plug_cxx(force, &cmd, PLUGS_DIR"cpp_async/plug.cpp", BUILD_DIR"libcpp_async.so")) return 1;
    {
        const char *output_path = BUILD_DIR"libc3";
        const char *source_paths[] = {
            PLUGS_DIR"c3/plug.c3",
            PLUGS_DIR"c3/raylib.c3i",
            PLUGS_DIR"c3/future.c3"
        };
        size_t source_paths_count = NOB_ARRAY_LEN(source_paths);

        if (!build_plug_c3(force, &cmd, output_path, source_paths, source_paths_count)) return 1;
    }

    {
        const char *output_path = BUILD_DIR"panim";
        const char *input_paths[] = {
            PANIM_DIR"panim.c",
            PANIM_DIR"ffmpeg_linux.c"
        };
        size_t input_paths_len = NOB_ARRAY_LEN(input_paths);
        if (!build_exe(force, &cmd, input_paths, input_paths_len, output_path)) return 1;
    }

    return 0;
}
