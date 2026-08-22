#ifndef REDIRECTION_H
#define REDIRECTION_H

#include "arbitrary.h"
#include "lexer.h"

void check_redirection(
    Token *tokens,
    Input *input,
    Output *output
);

int prepare_input(Input *input);

int *prepare_output(
    Output *output,
    size_t *count
);

void close_output_files(
    int *fds,
    size_t count
);

void free_input_files(InputFile *files);

void free_output_files(OutputFile *files);

#endif