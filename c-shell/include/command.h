#ifndef COMMAND_H
#define COMMAND_H

#include "arbitrary.h"
#include "lexer.h"

char *find_command(const char *command);

char **make_argv(Token *tokens);

void free_argv(char **argv);

int execute_command(
    Token *tokens,
    Input *input,
    Output *output
);

#endif