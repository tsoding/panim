#define NOB_IMPLEMENTATION
#include "./src/nob.h"

#define BUILD_DIR "./build"
#define SRC_DIR "./src"

void cflags(Nob_Cmd *cmd)
{
    nob_cmd_append(cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(cmd, "-I./raylib/raylib-5.0_linux_amd64/include");
    nob_cmd_append(cmd, "-I./src/");
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
    nob_cmd_append(cmd, "-Wl,-rpath="BUILD_DIR);
    nob_cmd_append(cmd, "-L./raylib/raylib-5.0_linux_amd64/lib");
    nob_cmd_append(cmd, "-l:libraylib.so", "-lm", "-ldl", "-lpthread");
}

bool build_plug_c(bool force, Nob_Cmd *cmd, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cmd->count = 0;
        cc(cmd);
        nob_cmd_append(cmd, "-fPIC", "-shared", "-Wl,--no-undefined");
        nob_cmd_append(cmd, "-o", output_path);
        nob_cmd_append(cmd, source_path, SRC_DIR"/tasks.c");
        libs(cmd);
        return nob_cmd_run_sync(*cmd);
    }

    nob_log(NOB_INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_plug_cxx(bool force, Nob_Cmd *cmd, const char *source_path, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild1(output_path, source_path);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cmd->count = 0;
        cxx(cmd);
        nob_cmd_append(cmd, "-fPIC", "-shared", "-Wl,--no-undefined");
        nob_cmd_append(cmd, "-o", output_path);
        nob_cmd_append(cmd, source_path, SRC_DIR"/tasks.c");
        libs(cmd);
        return nob_cmd_run_sync(*cmd);
    }

    nob_log(NOB_INFO, "%s is up-to-date", output_path);
    return true;
}

bool build_exe(bool force, Nob_Cmd *cmd, const char **input_paths, size_t input_paths_len, const char *output_path)
{
    int rebuild_is_needed = nob_needs_rebuild(output_path, input_paths, input_paths_len);
    if (rebuild_is_needed < 0) return false;

    if (force || rebuild_is_needed) {
        cmd->count = 0;
        cc(cmd);
        nob_cmd_append(cmd, "-o", output_path);
        nob_da_append_many(cmd, input_paths, input_paths_len);
        libs(cmd);
        return nob_cmd_run_sync(*cmd);
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
    if (!build_plug_c(force, &cmd, SRC_DIR"/tm.c", BUILD_DIR"/libtm.so")) return 1;
    if (!build_plug_c(force, &cmd, SRC_DIR"/template.c", BUILD_DIR"/libtemplate.so")) return 1;
    if (!build_plug_c(force, &cmd, SRC_DIR"/squares.c", BUILD_DIR"/libsquare.so")) return 1;
    if (!build_plug_c(force, &cmd, SRC_DIR"/bezier.c", BUILD_DIR"/libbezier.so")) return 1;
    if (!build_plug_cxx(force, &cmd, SRC_DIR"/probe.cpp", BUILD_DIR"/libprobe.so")) return 1;

    {
        const char *output_path = BUILD_DIR"/panim";
        const char *input_paths[] = {
            SRC_DIR"/panim.c",
            SRC_DIR"/ffmpeg_linux.c"
        };
        size_t input_paths_len = NOB_ARRAY_LEN(input_paths);
        if (!build_exe(force, &cmd, input_paths, input_paths_len, output_path)) return 1;
    }

    return 0;
}
