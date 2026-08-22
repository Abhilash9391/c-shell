#include "command.h"
#include "redirection.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool check_executable(const char *path)
{
    return access(path, X_OK) == 0;
}

static char *check_current_directory(const char *name)
{
    char path[PATH_MAX];

    int length = snprintf(
        path,
        sizeof(path),
        "./%s",
        name
    );

    if (length < 0 || length >= (int)sizeof(path)) {
        return NULL;
    }

    if (!check_executable(path)) {
        return NULL;
    }

    return realpath(path, NULL);
}

static char *path_search(const char *name)
{
    char *path_env = getenv("PATH");

    if (path_env == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path_env);

    if (path_copy == NULL) {
        return NULL;
    }

    char *directory = strtok(path_copy, ":");

    while (directory != NULL) {

        if (directory[0] == '\0') {
            directory = ".";
        }

        char path[PATH_MAX];

        int length = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            directory,
            name
        );

        if (length >= 0 &&
            length < (int)sizeof(path) &&
            check_executable(path)) {

            char *absolute_path =
                realpath(path, NULL);

            if (absolute_path != NULL) {
                free(path_copy);
                return absolute_path;
            }
        }

        directory = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

char *find_command(const char *command)
{
    if (command == NULL || command[0] == '\0') {
        return NULL;
    }

    if (strchr(command, '/') != NULL) {

        if (!check_executable(command)) {
            return NULL;
        }

        return realpath(command, NULL);
    }

    if (command[0] == '%') {

        if (command[1] == '\0') {
            return NULL;
        }

        return path_search(command + 1);
    }

    char *path =
        check_current_directory(command);

    if (path != NULL) {
        return path;
    }

    return path_search(command);
}

void free_argv(char **argv)
{
    if (argv == NULL) {
        return;
    }

    for (size_t i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }

    free(argv);
}

char **make_argv(Token *tokens)
{
    size_t capacity = 8;
    size_t count = 0;

    char **argv =
        malloc(capacity * sizeof(char *));

    if (argv == NULL) {
        return NULL;
    }

    while (tokens != NULL) {

        if (tokens->type == TOKEN_PIPE ||
            tokens->type == TOKEN_SEMI ||
            tokens->type == TOKEN_AMP) {
            break;
        }

        if (tokens->type == TOKEN_LT ||
            tokens->type == TOKEN_GT ||
            tokens->type == TOKEN_GTGT) {

            tokens = tokens->next;

            if (tokens != NULL) {
                tokens = tokens->next;
            }

            continue;
        }

        if (tokens->type == TOKEN_WORD) {

            if (count + 1 >= capacity) {

                capacity *= 2;

                char **new_argv =
                    realloc(
                        argv,
                        capacity * sizeof(char *)
                    );

                if (new_argv == NULL) {
                    free_argv(argv);
                    return NULL;
                }

                argv = new_argv;
            }

            argv[count] =
                strdup(tokens->value);

            if (argv[count] == NULL) {
                free_argv(argv);
                return NULL;
            }

            count++;
        }

        tokens = tokens->next;
    }

    argv[count] = NULL;

    return argv;
}

int execute_command(
    Token *tokens,
    Input *input,
    Output *output
)
{
    char **argv = make_argv(tokens);

    if (argv == NULL || argv[0] == NULL) {
        free_argv(argv);
        return 1;
    }

    char *path = find_command(argv[0]);

    if (path == NULL) {

        const char *name = argv[0];

        if (name[0] == '%') {
            name++;
        }

        fprintf(
            stderr,
            "cshell: command not found (%s)\n",
            name
        );

        free_argv(argv);
        return 127;
    }

    int input_fd = prepare_input(input);

    if (input->input_redirection &&
        input_fd < 0) {

        free(path);
        free_argv(argv);
        return 1;
    }

    size_t output_count = 0;

    int *output_fds =
        prepare_output(
            output,
            &output_count
        );

    if (output->output_redirection &&
        output_fds == NULL) {

        if (input_fd >= 0) {
            close(input_fd);
        }

        free(path);
        free_argv(argv);
        return 1;
    }

    int output_pipe[2] = {-1, -1};

    if (output_count > 0) {

        if (pipe(output_pipe) < 0) {

            if (input_fd >= 0) {
                close(input_fd);
            }

            close_output_files(
                output_fds,
                output_count
            );

            free(path);
            free_argv(argv);
            return 1;
        }
    }

    pid_t pid = fork();

    if (pid < 0) {

        if (input_fd >= 0) {
            close(input_fd);
        }

        if (output_count > 0) {
            close(output_pipe[0]);
            close(output_pipe[1]);
        }

        close_output_files(
            output_fds,
            output_count
        );

        free(path);
        free_argv(argv);

        return 1;
    }

    if (pid == 0) {

        if (input_fd >= 0) {

            if (dup2(
                input_fd,
                STDIN_FILENO
            ) < 0) {
                _exit(1);
            }

            close(input_fd);
        }

        if (output_count > 0) {

            if (dup2(
                output_pipe[1],
                STDOUT_FILENO
            ) < 0) {
                _exit(1);
            }

            close(output_pipe[0]);
            close(output_pipe[1]);
        }

        execv(path, argv);

        _exit(127);
    }

    if (input_fd >= 0) {
        close(input_fd);
    }

    if (output_count > 0) {

        close(output_pipe[1]);

        char buffer[4096];
        ssize_t bytes;

        while ((bytes = read(
            output_pipe[0],
            buffer,
            sizeof(buffer)
        )) > 0) {

            for (size_t i = 0;
                 i < output_count;
                 i++) {

                ssize_t written = 0;

                while (written < bytes) {

                    ssize_t result = write(
                        output_fds[i],
                        buffer + written,
                        bytes - written
                    );

                    if (result <= 0) {
                        break;
                    }

                    written += result;
                }
            }
        }

        close(output_pipe[0]);
    }

    int status;

    while (waitpid(
        pid,
        &status,
        0
    ) < 0) {

        if (errno != EINTR) {
            break;
        }
    }

    close_output_files(
        output_fds,
        output_count
    );

    free(path);
    free_argv(argv);

    return status;
}