#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    LINE,
    BG,
    TGT,
    CMD,
    ARG
} VariableType;

bool check_syntax(Token *tokens, VariableType current);

#endif