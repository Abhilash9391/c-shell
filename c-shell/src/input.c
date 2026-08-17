#include "input.h"

#include <stdio.h>
#include <stdlib.h>

char *read_input(void)
{
    char *line = NULL;
    size_t capacity = 0;

    ssize_t length = getline(&line, &capacity, stdin);

    if (length == -1) {
        free(line);
        return NULL;
    }

    return line;
}