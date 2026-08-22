#include "pipeline.h"
#include "command.h"
#include "redirection.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    Token *start;
    Token *end;
} CommandPart;

typedef struct {
    int read_fd;
    int write_fd;
    int *output_fds;
    size_t output_count;
} OutputCapture;

static size_t count_commands(Token *tokens)
{
    size_t count = 1;

    while (tokens != NULL) {

        if (tokens->type == TOKEN_PIPE) {
            count++;
        }

        tokens = tokens->next;
    }

    return count;
}

static void split_commands(
    Token *tokens,
    CommandPart *commands,
    size_t count
)
{
    size_t index = 0;

    Token *start = tokens;
    Token *current = tokens;

    while (current != NULL &&
           index < count - 1) {

        if (current->type == TOKEN_PIPE) {

            commands[index].start = start;
            commands[index].end = current;

            index++;

            start = current->next;
        }

        current = current->next;
    }

    commands[index].start = start;
    commands[index].end = NULL;
}

static void get_redirection(
    CommandPart *command,
    Input *input,
    Output *output
)
{
    Token *current = command->start;

    while (current != NULL &&
           current != command->end) {

        if (current->type == TOKEN_LT) {

            input->input_redirection = true;

            if (current->next != NULL &&
                current->next != command->end) {

                InputFile *file =
                    malloc(sizeof(InputFile));

                if (file == NULL) {
                    return;
                }

                file->name =
                    strdup(current->next->value);

                file->next = NULL;

                if (input->files == NULL) {
                    input->files = file;
                } else {

                    InputFile *last =
                        input->files;

                    while (last->next != NULL) {
                        last = last->next;
                    }

                    last->next = file;
                }
            }

        } else if (
            current->type == TOKEN_GT ||
            current->type == TOKEN_GTGT
        ) {

            output->output_redirection = true;

            if (current->next != NULL &&
                current->next != command->end) {

                OutputFile *file =
                    malloc(sizeof(OutputFile));

                if (file == NULL) {
                    return;
                }

                file->name =
                    strdup(current->next->value);

                file->append =
                    current->type == TOKEN_GTGT;

                file->next = NULL;

                if (output->files == NULL) {
                    output->files = file;
                } else {

                    OutputFile *last =
                        output->files;

                    while (last->next != NULL) {
                        last = last->next;
                    }

                    last->next = file;
                }
            }
        }

        current = current->next;
    }
}

static char **make_stage_argv(
    CommandPart *command
)
{
    size_t capacity = 8;
    size_t count = 0;

    char **argv =
        malloc(capacity * sizeof(char *));

    if (argv == NULL) {
        return NULL;
    }

    Token *current = command->start;

    while (current != NULL &&
           current != command->end) {

        if (current->type == TOKEN_LT ||
            current->type == TOKEN_GT ||
            current->type == TOKEN_GTGT) {

            current = current->next;

            if (current != NULL &&
                current != command->end) {
                current = current->next;
            }

            continue;
        }

        if (current->type == TOKEN_WORD) {

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
                strdup(current->value);

            if (argv[count] == NULL) {
                free_argv(argv);
                return NULL;
            }

            count++;
        }

        current = current->next;
    }

    argv[count] = NULL;

    return argv;
}

static void close_pipeline_pipes(
    int (*pipes)[2],
    size_t count
)
{
    for (size_t i = 0;
         i < count;
         i++) {

        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void forward_all_output(
    OutputCapture *captures,
    size_t count
)
{
    size_t active = 0;

    for (size_t i = 0;
         i < count;
         i++) {

        if (captures[i].read_fd >= 0) {
            active++;
        }
    }

    while (active > 0) {

        struct pollfd *fds =
            calloc(
                count,
                sizeof(struct pollfd)
            );

        size_t *indexes =
            calloc(
                count,
                sizeof(size_t)
            );

        if (fds == NULL ||
            indexes == NULL) {

            free(fds);
            free(indexes);
            return;
        }

        size_t number = 0;

        for (size_t i = 0;
             i < count;
             i++) {

            if (captures[i].read_fd >= 0) {

                fds[number].fd =
                    captures[i].read_fd;

                fds[number].events =
                    POLLIN;

                indexes[number] = i;

                number++;
            }
        }

        if (poll(
            fds,
            number,
            -1
        ) < 0) {

            free(fds);
            free(indexes);
            return;
        }

        for (size_t i = 0;
             i < number;
             i++) {

            if (!(fds[i].revents &
                  (POLLIN | POLLHUP))) {
                continue;
            }

            size_t index = indexes[i];

            char buffer[4096];

            ssize_t bytes =
                read(
                    captures[index].read_fd,
                    buffer,
                    sizeof(buffer)
                );

            if (bytes > 0) {

                for (size_t j = 0;
                     j < captures[index].output_count;
                     j++) {

                    ssize_t written = 0;

                    while (written < bytes) {

                        ssize_t result =
                            write(
                                captures[index].output_fds[j],
                                buffer + written,
                                bytes - written
                            );

                        if (result <= 0) {
                            break;
                        }

                        written += result;
                    }
                }

            } else {

                close(
                    captures[index].read_fd
                );

                captures[index].read_fd = -1;

                active--;
            }
        }

        free(fds);
        free(indexes);
    }
}

static void child_execute(
    CommandPart *command,
    int input_fd,
    int output_fd,
    OutputCapture *capture
)
{
    Input input = {
        .input_redirection = false,
        .files = NULL
    };

    Output output = {
        .output_redirection = false,
        .files = NULL
    };

    get_redirection(
        command,
        &input,
        &output
    );

    char **argv =
        make_stage_argv(command);

    if (argv == NULL ||
        argv[0] == NULL) {

        free_argv(argv);
        free_input_files(input.files);
        free_output_files(output.files);

        _exit(1);
    }

    char *path =
        find_command(argv[0]);

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
        free_input_files(input.files);
        free_output_files(output.files);

        _exit(127);
    }

    int actual_input = input_fd;

    if (input.input_redirection) {

        actual_input =
            prepare_input(&input);

        if (actual_input < 0) {

            free(path);
            free_argv(argv);
            free_input_files(input.files);
            free_output_files(output.files);

            _exit(1);
        }
    }

    if (actual_input >= 0) {

        if (dup2(
            actual_input,
            STDIN_FILENO
        ) < 0) {
            _exit(1);
        }

        close(actual_input);
    }

    if (output.output_redirection) {

        if (capture == NULL ||
            capture->write_fd < 0) {
            _exit(1);
        }

        if (dup2(
            capture->write_fd,
            STDOUT_FILENO
        ) < 0) {
            _exit(1);
        }

        close(capture->write_fd);

    } else if (output_fd >= 0) {

        if (dup2(
            output_fd,
            STDOUT_FILENO
        ) < 0) {
            _exit(1);
        }

        close(output_fd);
    }

    execv(path, argv);

    free(path);
    free_argv(argv);
    free_input_files(input.files);
    free_output_files(output.files);

    _exit(127);
}

void execute_pipeline(Token *tokens)
{
    size_t command_count =
        count_commands(tokens);

    if (command_count < 2) {
        return;
    }

    CommandPart *commands =
        calloc(
            command_count,
            sizeof(CommandPart)
        );

    if (commands == NULL) {
        return;
    }

    split_commands(
        tokens,
        commands,
        command_count
    );

    size_t pipe_count =
        command_count - 1;

    int (*pipes)[2] =
        malloc(
            pipe_count *
            sizeof(int[2])
        );

    pid_t *pids =
        calloc(
            command_count,
            sizeof(pid_t)
        );

    OutputCapture *captures =
        calloc(
            command_count,
            sizeof(OutputCapture)
        );

    if (pipes == NULL ||
        pids == NULL ||
        captures == NULL) {

        free(commands);
        free(pipes);
        free(pids);
        free(captures);

        return;
    }

    for (size_t i = 0;
         i < command_count;
         i++) {

        captures[i].read_fd = -1;
        captures[i].write_fd = -1;
        captures[i].output_fds = NULL;
        captures[i].output_count = 0;
    }

    for (size_t i = 0;
         i < pipe_count;
         i++) {

        if (pipe(pipes[i]) < 0) {

            for (size_t j = 0;
                 j < i;
                 j++) {

                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            free(commands);
            free(pipes);
            free(pids);
            free(captures);

            return;
        }
    }

    for (size_t i = 0;
         i < command_count;
         i++) {

        Input input = {
            .input_redirection = false,
            .files = NULL
        };

        Output output = {
            .output_redirection = false,
            .files = NULL
        };

        get_redirection(
            &commands[i],
            &input,
            &output
        );

        if (output.output_redirection) {

            captures[i].output_fds =
                prepare_output(
                    &output,
                    &captures[i].output_count
                );

            if (captures[i].output_fds == NULL) {

                free_input_files(input.files);
                free_output_files(output.files);

                close_pipeline_pipes(
                    pipes,
                    pipe_count
                );

                free(commands);
                free(pipes);
                free(pids);
                free(captures);

                return;
            }

            int capture_pipe[2];

            if (pipe(capture_pipe) < 0) {

                close_output_files(
                    captures[i].output_fds,
                    captures[i].output_count
                );

                free_input_files(input.files);
                free_output_files(output.files);

                close_pipeline_pipes(
                    pipes,
                    pipe_count
                );

                free(commands);
                free(pipes);
                free(pids);
                free(captures);

                return;
            }

            captures[i].read_fd =
                capture_pipe[0];

            captures[i].write_fd =
                capture_pipe[1];
        }

        free_input_files(input.files);
        free_output_files(output.files);
    }

    for (size_t i = 0;
         i < command_count;
         i++) {

        int input_fd = -1;
        int output_fd = -1;

        if (i > 0) {
            input_fd =
                pipes[i - 1][0];
        }

        if (i < pipe_count) {
            output_fd =
                pipes[i][1];
        }

        pid_t pid = fork();

        if (pid < 0) {
            continue;
        }

        if (pid == 0) {

            for (size_t j = 0;
                 j < pipe_count;
                 j++) {

                if (pipes[j][0] != input_fd) {
                    close(pipes[j][0]);
                }

                if (pipes[j][1] != output_fd) {
                    close(pipes[j][1]);
                }
            }

            for (size_t j = 0;
                 j < command_count;
                 j++) {

                if (captures[j].read_fd >= 0) {
                    close(captures[j].read_fd);
                }

                if (j != i &&
                    captures[j].write_fd >= 0) {

                    close(captures[j].write_fd);
                }
            }

            child_execute(
                &commands[i],
                input_fd,
                output_fd,
                &captures[i]
            );

            _exit(1);
        }

        pids[i] = pid;
    }

    close_pipeline_pipes(
        pipes,
        pipe_count
    );

    for (size_t i = 0;
         i < command_count;
         i++) {

        if (captures[i].write_fd >= 0) {
            close(captures[i].write_fd);
            captures[i].write_fd = -1;
        }
    }

    forward_all_output(
        captures,
        command_count
    );

    for (size_t i = 0;
         i < command_count;
         i++) {

        if (pids[i] > 0) {

            waitpid(
                pids[i],
                NULL,
                0
            );
        }
    }

    for (size_t i = 0;
         i < command_count;
         i++) {

        if (captures[i].read_fd >= 0) {
            close(captures[i].read_fd);
        }

        close_output_files(
            captures[i].output_fds,
            captures[i].output_count
        );
    }

    free(commands);
    free(pipes);
    free(pids);
    free(captures);
}