#include "shell.h"

#include <stddef.h>

ShellState shell = {
    .home_directory = NULL,
    .previous_directory = NULL
};