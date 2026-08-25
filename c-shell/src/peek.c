#include "peek.h"
#include "lexer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHUNK_SIZE 4096

static bool is_flag(const char *value)
{
    return value[0] == '-' && value[1] != '\0';
}

static bool parse_flag(
    const char *value,
    bool *reverse,
    bool *linecount
)
{
    for (size_t i = 1; value[i] != '\0'; i++) {
        if (value[i] == 'r') {
            *reverse = true;
        } else if (value[i] == 'n') {
            *linecount = true;
        } else {
            return false;
        }
    }

    return true;
}

static void print_line(
    const char *line,
    size_t length,
    bool newline,
    bool linecount,
    int *line_number
)
{
    bool non_empty = false;

    for (size_t i = 0; i < length; i++) {
        if (line[i] != '\n') {
            non_empty = true;
            break;
        }
    }

    if (linecount && non_empty) {
        printf("%d ", *line_number);
        (*line_number)++;
    }

    if (length > 0) {
        fwrite(line, 1, length, stdout);
    }

    if (newline) {
        putchar('\n');
    }
}

static void read_forward(
    int fd,
    bool linecount,
    int *line_number
)
{
    char buffer[CHUNK_SIZE];
    char *line = NULL;

    size_t line_length = 0;
    size_t capacity = 0;

    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                print_line(
                    line,
                    line_length,
                    true,
                    linecount,
                    line_number
                );

                line_length = 0;
            } else {
                if (line_length == capacity) {
                    size_t new_capacity =
                        capacity == 0 ? 128 : capacity * 2;

                    char *new_line = realloc(
                        line,
                        new_capacity
                    );

                    if (new_line == NULL) {
                        perror("realloc");
                        free(line);
                        return;
                    }

                    line = new_line;
                    capacity = new_capacity;
                }

                line[line_length++] = buffer[i];
            }
        }
    }

    if (bytes_read == -1) {
        perror("peek: read");
        free(line);
        return;
    }

    if (line_length > 0) {
        print_line(
            line,
            line_length,
            false,
            linecount,
            line_number
        );
    }

    free(line);
}

static void print_reversed_line(
    const char *line,
    size_t length,
    bool newline,
    bool linecount,
    int *line_number
)
{
    bool non_empty = false;

    for (size_t i = 0; i < length; i++) {
        if (line[i] != '\n') {
            non_empty = true;
            break;
        }
    }

    if (linecount && non_empty) {
        printf("%d ", *line_number);
        (*line_number)++;
    }

    for (size_t i = length; i > 0; i--) {
        putchar(line[i - 1]);
    }

    if (newline) {
        putchar('\n');
    }
}

static void read_reverse_seekable(
    int fd,
    bool linecount,
    int *line_number
)
{
    off_t file_size = lseek(fd, 0, SEEK_END);

    if (file_size == (off_t)-1) {
        perror("peek: lseek");
        return;
    }

    if (file_size == 0) {
        return;
    }

    char buffer[CHUNK_SIZE];
    char *line = NULL;

    size_t line_length = 0;
    size_t capacity = 0;

    off_t position = file_size;
    bool first_newline = true;

    while (position > 0) {
        size_t amount =
            position >= CHUNK_SIZE
                ? CHUNK_SIZE
                : (size_t)position;

        position -= amount;

        if (lseek(fd, position, SEEK_SET) == (off_t)-1) {
            perror("peek: lseek");
            free(line);
            return;
        }

        ssize_t bytes_read = read(fd, buffer, amount);

        if (bytes_read != (ssize_t)amount) {
            if (bytes_read == -1) {
                perror("peek: read");
            } else {
                fprintf(stderr, "peek: unexpected end of file\n");
            }

            free(line);
            return;
        }

        for (ssize_t i = bytes_read - 1; i >= 0; i--) {
            char c = buffer[i];

            if (c == '\n') {
                if (first_newline && line_length == 0) {
                    first_newline = false;
                    continue;
                }

                first_newline = false;

                print_reversed_line(
                    line,
                    line_length,
                    true,
                    linecount,
                    line_number
                );

                line_length = 0;
            } else {
                first_newline = false;

                if (line_length == capacity) {
                    size_t new_capacity =
                        capacity == 0 ? 128 : capacity * 2;

                    char *new_line = realloc(
                        line,
                        new_capacity
                    );

                    if (new_line == NULL) {
                        perror("realloc");
                        free(line);
                        return;
                    }

                    line = new_line;
                    capacity = new_capacity;
                }

                line[line_length++] = c;
            }
        }
    }

    if (line_length > 0) {
        print_reversed_line(
            line,
            line_length,
            false,
            linecount,
            line_number
        );
    }

    if (file_size == 1) {
        if (lseek(fd, 0, SEEK_SET) != (off_t)-1) {
            char c;

            if (read(fd, &c, 1) == 1 && c == '\n') {
                putchar('\n');
            }
        }
    }

    free(line);
}

static void read_reverse_buffered(
    int fd,
    bool linecount,
    int *line_number
)
{
    char **lines = NULL;
    bool *newlines = NULL;

    size_t count = 0;
    size_t capacity = 0;

    char buffer[CHUNK_SIZE];

    char *current_line = NULL;
    size_t current_length = 0;
    size_t current_capacity = 0;

    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                if (count == capacity) {
                    size_t new_capacity =
                        capacity == 0 ? 16 : capacity * 2;

                    char **new_lines = realloc(
                        lines,
                        new_capacity * sizeof(*lines)
                    );

                    if (new_lines == NULL) {
                        perror("realloc");
                        free(current_line);

                        for (size_t j = 0; j < count; j++) {
                            free(lines[j]);
                        }

                        free(lines);
                        free(newlines);
                        return;
                    }

                    lines = new_lines;

                    bool *new_newlines = realloc(
                        newlines,
                        new_capacity * sizeof(*newlines)
                    );

                    if (new_newlines == NULL) {
                        perror("realloc");
                        free(current_line);

                        for (size_t j = 0; j < count; j++) {
                            free(lines[j]);
                        }

                        free(lines);
                        free(newlines);
                        return;
                    }

                    newlines = new_newlines;
                    capacity = new_capacity;
                }

                char *stored_line = malloc(
                    current_length + 1
                );

                if (stored_line == NULL) {
                    perror("malloc");
                    free(current_line);

                    for (size_t j = 0; j < count; j++) {
                        free(lines[j]);
                    }

                    free(lines);
                    free(newlines);
                    return;
                }

                if (current_length > 0) {
                    memcpy(
                        stored_line,
                        current_line,
                        current_length
                    );
                }

                stored_line[current_length] = '\0';

                lines[count] = stored_line;
                newlines[count] = true;
                count++;

                current_length = 0;
            } else {
                if (current_length == current_capacity) {
                    size_t new_capacity =
                        current_capacity == 0
                            ? 128
                            : current_capacity * 2;

                    char *new_line = realloc(
                        current_line,
                        new_capacity
                    );

                    if (new_line == NULL) {
                        perror("realloc");
                        free(current_line);

                        for (size_t j = 0; j < count; j++) {
                            free(lines[j]);
                        }

                        free(lines);
                        free(newlines);
                        return;
                    }

                    current_line = new_line;
                    current_capacity = new_capacity;
                }

                current_line[current_length++] = buffer[i];
            }
        }
    }

    if (bytes_read == -1) {
        perror("peek: read");
    }

    if (current_length > 0) {
        if (count == capacity) {
            size_t new_capacity =
                capacity == 0 ? 16 : capacity * 2;

            char **new_lines = realloc(
                lines,
                new_capacity * sizeof(*lines)
            );

            if (new_lines == NULL) {
                perror("realloc");
                free(current_line);

                for (size_t j = 0; j < count; j++) {
                    free(lines[j]);
                }

                free(lines);
                free(newlines);
                return;
            }

            lines = new_lines;

            bool *new_newlines = realloc(
                newlines,
                new_capacity * sizeof(*newlines)
            );

            if (new_newlines == NULL) {
                perror("realloc");
                free(current_line);

                for (size_t j = 0; j < count; j++) {
                    free(lines[j]);
                }

                free(lines);
                free(newlines);
                return;
            }

            newlines = new_newlines;
            capacity = new_capacity;
        }

        char *stored_line = malloc(current_length + 1);

        if (stored_line == NULL) {
            perror("malloc");
            free(current_line);

            for (size_t j = 0; j < count; j++) {
                free(lines[j]);
            }

            free(lines);
            free(newlines);
            return;
        }

        memcpy(
            stored_line,
            current_line,
            current_length
        );

        stored_line[current_length] = '\0';

        lines[count] = stored_line;
        newlines[count] = false;
        count++;
    }

    free(current_line);

    for (size_t i = count; i > 0; i--) {
        size_t index = i - 1;

        print_line(
            lines[index],
            strlen(lines[index]),
            newlines[index],
            linecount,
            line_number
        );

        free(lines[index]);
    }

    free(lines);
    free(newlines);
}

static void process_input(
    int fd,
    bool reverse,
    bool linecount,
    int *line_number
)
{
    if (!reverse) {
        read_forward(
            fd,
            linecount,
            line_number
        );

        return;
    }

    errno = 0;

    off_t position = lseek(fd, 0, SEEK_CUR);

    if (position != (off_t)-1) {
        read_reverse_seekable(
            fd,
            linecount,
            line_number
        );
    } else if (errno == ESPIPE) {
        read_reverse_buffered(
            fd,
            linecount,
            line_number
        );
    } else {
        perror("peek: lseek");
    }
}

static void process_file(
    const char *filename,
    bool reverse,
    bool linecount,
    int *line_number
)
{
    if (strcmp(filename, "-") == 0) {
        process_input(
            STDIN_FILENO,
            reverse,
            linecount,
            line_number
        );

        return;
    }

    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        printf("peek: no such file or directory\n");
        return;
    }

    struct stat info;

    if (fstat(fd, &info) == -1) {
        printf("peek: no such file or directory\n");
        close(fd);
        return;
    }

    if (S_ISDIR(info.st_mode)) {
        printf("peek: is a directory\n");
        close(fd);
        return;
    }

    process_input(
        fd,
        reverse,
        linecount,
        line_number
    );

    close(fd);
}

void peek_handler(Token *values)
{
    bool reverse = false;
    bool linecount = false;

    char **names = NULL;

    size_t count = 0;
    size_t capacity = 0;

    while (values != NULL) {
        char *value = values->value;

        if (is_flag(value)) {
            if (!parse_flag(
                    value,
                    &reverse,
                    &linecount
                )) {

                printf("peek: invalid syntax\n");
                free(names);
                return;
            }
        } else {
            if (count == capacity) {
                size_t new_capacity =
                    capacity == 0 ? 16 : capacity * 2;

                char **new_names = realloc(
                    names,
                    new_capacity * sizeof(*names)
                );

                if (new_names == NULL) {
                    perror("realloc");
                    free(names);
                    return;
                }

                names = new_names;
                capacity = new_capacity;
            }

            names[count] = value;
            count++;
        }

        values = values->next;
    }

    int line_number = 1;

    if (count == 0) {
        process_input(
            STDIN_FILENO,
            reverse,
            linecount,
            &line_number
        );

        free(names);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        process_file(
            names[i],
            reverse,
            linecount,
            &line_number
        );
    }

    free(names);
}