#ifndef SHELL_H
#define SHELL_H

typedef struct {
    char *home_directory;
    char *previous_directory;
} ShellState;

extern ShellState shell;

#endif