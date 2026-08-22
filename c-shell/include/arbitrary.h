#ifndef ARBITRARY_H
#define ARBITRARY_H

#include "lexer.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct InputFile {
    char *name;
    struct InputFile *next;
} InputFile;

typedef struct OutputFile {
    char *name;
    bool append;
    struct OutputFile *next;
} OutputFile;

typedef struct {
    bool input_redirection;
    InputFile *files;
} Input;

typedef struct {
    bool output_redirection;
    OutputFile *files;
} Output;

void arbitrary_handler(Token *tokens);

#endif