#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_AMP,
    TOKEN_SEMI,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_GTGT
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;

    struct Token *next;
} Token;

Token *lex_line(const char *line);
void free_tokens(Token *tokens);

void print_tokens(const Token *tokens);

#endif