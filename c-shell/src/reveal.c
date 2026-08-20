#include "reveal.h"
#include "path_utils.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool is_flag(const char *value)
{
    return value[0] == '-' && value[1] != '\0';
}

static bool parse_flag(
    const char *value,
    bool *show_hidden,
    bool *recursive
)
{
    for (size_t i = 1; value[i] != '\0'; i++) {
        if (value[i] == 'a') {
            *show_hidden = true;
        } else if (value[i] == 't') {
            *recursive = true;
        } else {
            return false;
        }
    }

    return true;
}

static int compare_names(const void *a, const void *b)
{
    const char *first = *(const char **)a;
    const char *second = *(const char **)b;

    return strcmp(first, second);
}

static void free_names(char **names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }

    free(names);
}

static void list_directory(
    const char *path,
    bool show_hidden,
    bool recursive
)
{
    DIR *directory = opendir(path);

    if (directory == NULL) {
        printf("reveal: no such directory\n");
        return;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }

        if (count == capacity) {
            if (capacity == 0) {
                capacity = 16;
            } else {
                capacity *= 2;
            }

            char **new_names = realloc(
                names,
                capacity * sizeof(*names)
            );

            if (new_names == NULL) {
                perror("realloc");
                closedir(directory);
                free_names(names, count);
                exit(EXIT_FAILURE);
            }

            names = new_names;
        }

        names[count] = strdup(entry->d_name);

        if (names[count] == NULL) {
            perror("strdup");
            closedir(directory);
            free_names(names, count);
            exit(EXIT_FAILURE);
        }

        count++;
    }

    closedir(directory);

    qsort(
        names,
        count,
        sizeof(*names),
        compare_names
    );

    for (size_t i = 0; i < count; i++) {
        struct stat info;

        size_t path_length = strlen(path);
        size_t name_length = strlen(names[i]);

        char *child_path = malloc(
            path_length + name_length + 2
        );

        if (child_path == NULL) {
            perror("malloc");
            free_names(names, count);
            exit(EXIT_FAILURE);
        }

        snprintf(
            child_path,
            path_length + name_length + 2,
            "%s/%s",
            path,
            names[i]
        );

        bool is_directory = false;

        if (stat(child_path, &info) == 0 &&
            S_ISDIR(info.st_mode)) {
            is_directory = true;
        }

        if (is_directory) {
            printf("%s/\n", names[i]);
        } else {
            printf("%s\n", names[i]);
        }

        if (recursive && is_directory) {
            list_directory(
                child_path,
                show_hidden,
                recursive
            );
        }

        free(child_path);
    }

    free_names(names, count);
}

void reveal_handler(Token *values)
{
    bool show_hidden = false;
    bool recursive = false;

    char *directory = NULL;

    while (values != NULL) {
        char *value = values->value;

        if (is_flag(value) && strcmp(value, "-") != 0) {
            if (!parse_flag(value, &show_hidden, &recursive)) {
                printf("reveal: invalid syntax\n");
                free(directory);
                return;
            }
        } else {
            if (directory != NULL) {
                printf("reveal: invalid syntax\n");
                free(directory);
                return;
            }

            directory = strdup(value);

            if (directory == NULL) {
                perror("strdup");
                exit(EXIT_FAILURE);
            }
        }

        values = values->next;
    }

    if (directory == NULL) {
        directory = strdup(".");

        if (directory == NULL) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    }

    char *path = resolve_path(directory);

    free(directory);

    if (path == NULL) {
        printf("reveal: no such directory\n");
        return;
    }

    struct stat info;

    if (stat(path, &info) == -1 || !S_ISDIR(info.st_mode)) {
        printf("reveal: no such directory\n");
        free(path);
        return;
    }

    list_directory(
        path,
        show_hidden,
        recursive
    );

    free(path);
}