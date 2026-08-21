#include "locate.h"
#include "lexer.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool is_executable(const char *path)
{
    return access(path, X_OK) == 0;
}

static char *absolute_path(const char *path)
{
    char resolved[PATH_MAX];

    if (realpath(path, resolved) == NULL) {
        return NULL;
    }

    return strdup(resolved);
}

static void check_current_directory(
    const char *name,
    bool *found
)
{
    char path[PATH_MAX];

    if (snprintf(
            path,
            sizeof(path),
            "./%s",
            name
        ) >= (int)sizeof(path)) {
        return;
    }

    if (!is_executable(path)) {
        return;
    }

    char *absolute = absolute_path(path);

    if (absolute != NULL) {
        printf("%s\n", absolute);
        free(absolute);
        *found = true;
    }
}

static void search_path(
    const char *name,
    bool *found
)
{
    char *path_env = getenv("PATH");

    if (path_env == NULL) {
        return;
    }

    char *path_copy = strdup(path_env);

    if (path_copy == NULL) {
        perror("strdup");
        return;
    }

    char *directory = strtok(path_copy, ":");

    while (directory != NULL) {
        if (directory[0] == '\0') {
            directory = ".";
        }

        char path[PATH_MAX];

        if (snprintf(
                path,
                sizeof(path),
                "%s/%s",
                directory,
                name
            ) < (int)sizeof(path)) {

            if (is_executable(path)) {
                char *absolute = absolute_path(path);

                if (absolute != NULL) {
                    printf("%s\n", absolute);
                    free(absolute);
                    *found = true;
                }
            }
        }

        directory = strtok(NULL, ":");
    }

    free(path_copy);
}

static void locate_file(const char *name)
{
    bool found = false;

    check_current_directory(
        name,
        &found
    );

    search_path(
        name,
        &found
    );

    if (!found) {
        printf(
            "locate: command not found (%s)\n",
            name
        );
    }
}

void locate_handler(Token *values)
{
    if (values == NULL) {
        printf("locate: invalid syntax\n");
        return;
    }

    while (values != NULL) {
        locate_file(values->value);
        values = values->next;
    }
}