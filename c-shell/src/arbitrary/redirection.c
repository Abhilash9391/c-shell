#include "redirection.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void add_input_file(
    Input *input,
    const char *name
)
{
    InputFile *file =
        malloc(sizeof(InputFile));

    if (file == NULL) {
        return;
    }

    file->name = strdup(name);
    file->next = NULL;

    if (input->files == NULL) {
        input->files = file;
        return;
    }

    InputFile *current = input->files;

    while (current->next != NULL) {
        current = current->next;
    }

    current->next = file;
}

static void add_output_file(
    Output *output,
    const char *name,
    bool append
)
{
    OutputFile *file =
        malloc(sizeof(OutputFile));

    if (file == NULL) {
        return;
    }

    file->name = strdup(name);
    file->append = append;
    file->next = NULL;

    if (output->files == NULL) {
        output->files = file;
        return;
    }

    OutputFile *current = output->files;

    while (current->next != NULL) {
        current = current->next;
    }

    current->next = file;
}

void check_redirection(
    Token *tokens,
    Input *input,
    Output *output
)
{
    while (tokens != NULL) {

        if (tokens->type == TOKEN_PIPE ||
            tokens->type == TOKEN_SEMI ||
            tokens->type == TOKEN_AMP) {
            break;
        }

        if (tokens->type == TOKEN_LT) {

            input->input_redirection = true;

            if (tokens->next != NULL &&
                tokens->next->type == TOKEN_WORD) {

                add_input_file(
                    input,
                    tokens->next->value
                );
            }

        } else if (
            tokens->type == TOKEN_GT ||
            tokens->type == TOKEN_GTGT
        ) {

            output->output_redirection = true;

            if (tokens->next != NULL &&
                tokens->next->type == TOKEN_WORD) {

                add_output_file(
                    output,
                    tokens->next->value,
                    tokens->type == TOKEN_GTGT
                );
            }
        }

        tokens = tokens->next;
    }
}

int prepare_input(Input *input)
{
    if (!input->input_redirection) {
        return -1;
    }

    int temp_fd = open(
        "/tmp/cshell_input_XXXXXX",
        O_RDWR | O_CREAT | O_TRUNC,
        0600
    );

    if (temp_fd < 0) {
        fprintf(
            stderr,
            "cshell: no such file or directory\n"
        );
        return -2;
    }

    InputFile *file = input->files;

    while (file != NULL) {

        int fd = open(
            file->name,
            O_RDONLY
        );

        if (fd < 0) {

            close(temp_fd);

            fprintf(
                stderr,
                "cshell: no such file or directory\n"
            );

            return -2;
        }

        char buffer[4096];
        ssize_t bytes;

        while ((bytes = read(
            fd,
            buffer,
            sizeof(buffer)
        )) > 0) {

            ssize_t written = 0;

            while (written < bytes) {

                ssize_t result = write(
                    temp_fd,
                    buffer + written,
                    bytes - written
                );

                if (result <= 0) {
                    close(fd);
                    close(temp_fd);
                    return -2;
                }

                written += result;
            }
        }

        if (bytes < 0) {
            close(fd);
            close(temp_fd);
            return -2;
        }

        close(fd);

        file = file->next;
    }

    if (lseek(
        temp_fd,
        0,
        SEEK_SET
    ) < 0) {

        close(temp_fd);
        return -2;
    }

    return temp_fd;
}

int *prepare_output(
    Output *output,
    size_t *count
)
{
    *count = 0;

    if (!output->output_redirection) {
        return NULL;
    }

    size_t number = 0;

    OutputFile *file = output->files;

    while (file != NULL) {
        number++;
        file = file->next;
    }

    int *fds =
        malloc(number * sizeof(int));

    if (fds == NULL) {
        return NULL;
    }

    file = output->files;

    size_t index = 0;

    while (file != NULL) {

        int flags =
            O_WRONLY | O_CREAT;

        if (file->append) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        int fd = open(
            file->name,
            flags,
            0644
        );

        if (fd < 0) {

            for (size_t i = 0;
                 i < index;
                 i++) {
                close(fds[i]);
            }

            free(fds);

            fprintf(
                stderr,
                "cshell: unable to create file for writing\n"
            );

            return NULL;
        }

        fds[index] = fd;

        index++;
        file = file->next;
    }

    *count = number;

    return fds;
}

void close_output_files(
    int *fds,
    size_t count
)
{
    if (fds == NULL) {
        return;
    }

    for (size_t i = 0;
         i < count;
         i++) {
        close(fds[i]);
    }

    free(fds);
}

void free_input_files(InputFile *files)
{
    while (files != NULL) {

        InputFile *next = files->next;

        free(files->name);
        free(files);

        files = next;
    }
}

void free_output_files(OutputFile *files)
{
    while (files != NULL) {

        OutputFile *next = files->next;

        free(files->name);
        free(files);

        files = next;
    }
}