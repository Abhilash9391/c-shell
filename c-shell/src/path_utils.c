#include "path_utils.h"
#include "shell.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *resolve_path(const char *path)
{
    if (strcmp(path, "~") == 0) {
        return strdup(shell.home_directory);
    }

    if (strcmp(path, ".") == 0) {
        return getcwd(NULL, 0);
    }

    if (strcmp(path, "..") == 0) {
        char *current = getcwd(NULL, 0);

        if (current == NULL) {
            return NULL;
        }

        char *slash = strrchr(current, '/');

        if (slash == current) {
            slash[1] = '\0';
        } else if (slash != NULL) {
            *slash = '\0';
        }

        return current;
    }

    if (strcmp(path, "-") == 0) {
        if (shell.previous_directory == NULL) {
            return NULL;
        }

        return strdup(shell.previous_directory);
    }

    return strdup(path);
}