#include "hop.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *get_current_directory(void)
{
    char *cwd = getcwd(NULL, 0);

    if (cwd == NULL) {
        perror("getcwd");
        return NULL;
    }

    return cwd;
}

static bool change_directory(const char *path)
{
    char *current = get_current_directory();

    if (current == NULL) {
        return false;
    }

    if (chdir(path) == -1) {
        free(current);
        return false;
    }

    free(shell.previous_directory);
    shell.previous_directory = current;

    return true;
}

void hop_handler(Token *values)
{
    while (values != NULL) {
        char *value = values->value;

        if (strcmp(value, "~") == 0) {
            change_directory(shell.home_directory);

        } else if (strcmp(value, ".") == 0) {
            // Do nothing 

        } else if (strcmp(value, "..") == 0) {
            change_directory("..");

        } else if (strcmp(value, "-") == 0) {
            if (shell.previous_directory != NULL) {
                change_directory(shell.previous_directory);
            }

        } else {
            if (!change_directory(value)) {
                printf("hop: no such directory\n");
            }
        }

        values = values->next;
    }
}