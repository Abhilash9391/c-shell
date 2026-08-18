#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token *create_token(TokenType type, const char *value)
{
    Token *token = malloc(sizeof(*token));

    if (token == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    token->type = type;
    token->next = NULL;

    if (value != NULL) {
        token->value = strdup(value);

        if (token->value == NULL) {
            perror("strdup");
            free(token);
            exit(EXIT_FAILURE);
        }
    } else {
        token->value = NULL;
    }

    return token;
}

static void append_token(Token **head, Token **tail, Token *token)
{
    if (*head == NULL) {
        *head = token;
        *tail = token;
        return;
    }

    (*tail)->next = token;
    *tail = token;
}

static bool is_space(char c)
{
    return c == ' ' ||
           c == '\t' ||
           c == '\n' ||
           c == '\r';
}

static bool is_special(char c)
{
    return c == '|' ||
           c == '&' ||
           c == '>' ||
           c == '<' ||
           c == ';';
}

static void append_char(
    char **buffer,
    size_t *length,
    size_t *capacity,
    char c
)
{
    if (*length + 1 >= *capacity) {
        *capacity *= 2;

        char *new_buffer = realloc(*buffer, *capacity);

        if (new_buffer == NULL) {
            perror("realloc");
            free(*buffer);
            exit(EXIT_FAILURE);
        }

        *buffer = new_buffer;
    }

    (*buffer)[(*length)++] = c;
}

static char *parse_word(
    const char *line,
    size_t *position,
    bool *error
)
{
    size_t capacity = 16;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    while (line[*position] != '\0') {
        char c = line[*position];

        if (is_space(c) || is_special(c)) {
            break;
        }

        if (c == '\\') {
            (*position)++;

            if (line[*position] == '\0' ||
                line[*position] == '\n') {
                *error = true;
                free(buffer);
                return NULL;
            }

            append_char(
                &buffer,
                &length,
                &capacity,
                line[*position]
            );

            (*position)++;
            continue;
        }

        if (c == '"') {
            (*position)++;

            while (line[*position] != '\0' &&
                   line[*position] != '"') {
                c = line[*position];

                if (c == '\\') {
                    (*position)++;

                    if (line[*position] == '\0' ||
                        line[*position] == '\n') {
                        *error = true;
                        free(buffer);
                        return NULL;
                    }

                    if (line[*position] == '"' ||
                        line[*position] == '\\') {
                        append_char(
                            &buffer,
                            &length,
                            &capacity,
                            line[*position]
                        );
                    } else {
                        append_char(
                            &buffer,
                            &length,
                            &capacity,
                            '\\'
                        );

                        append_char(
                            &buffer,
                            &length,
                            &capacity,
                            line[*position]
                        );
                    }

                    (*position)++;
                    continue;
                }

                append_char(
                    &buffer,
                    &length,
                    &capacity,
                    c
                );

                (*position)++;
            }

            if (line[*position] != '"') {
                *error = true;
                free(buffer);
                return NULL;
            }

            (*position)++;
            continue;
        }

        if (c == '\'') {
            (*position)++;

            while (line[*position] != '\0' &&
                   line[*position] != '\'') {
                append_char(
                    &buffer,
                    &length,
                    &capacity,
                    line[*position]
                );

                (*position)++;
            }

            if (line[*position] != '\'') {
                *error = true;
                free(buffer);
                return NULL;
            }

            (*position)++;
            continue;
        }

        append_char(
            &buffer,
            &length,
            &capacity,
            c
        );

        (*position)++;
    }

    buffer[length] = '\0';

    return buffer;
}

Token *lex_line(const char *line)
{
    Token *head = NULL;
    Token *tail = NULL;
    size_t position = 0;

    while (line[position] != '\0') {
        if (is_space(line[position])) {
            position++;
            continue;
        }

        if (line[position] == '|') {
            append_token(
                &head,
                &tail,
                create_token(TOKEN_PIPE, NULL)
            );
            position++;
            continue;
        }

        if (line[position] == '&') {
            append_token(
                &head,
                &tail,
                create_token(TOKEN_AMP, NULL)
            );
            position++;
            continue;
        }

        if (line[position] == ';') {
            append_token(
                &head,
                &tail,
                create_token(TOKEN_SEMI, NULL)
            );
            position++;
            continue;
        }

        if (line[position] == '<') {
            append_token(
                &head,
                &tail,
                create_token(TOKEN_LT, NULL)
            );
            position++;
            continue;
        }

        if (line[position] == '>') {
            if (line[position + 1] == '>') {
                append_token(
                    &head,
                    &tail,
                    create_token(TOKEN_GTGT, NULL)
                );
                position += 2;
            } else {
                append_token(
                    &head,
                    &tail,
                    create_token(TOKEN_GT, NULL)
                );
                position++;
            }

            continue;
        }

        bool error = false;

        char *value = parse_word(
            line,
            &position,
            &error
        );

        if (error) {
            free_tokens(head);
            return NULL;
        }

        append_token(
            &head,
            &tail,
            create_token(TOKEN_WORD, value)
        );

        free(value);
    }

    return head;
}

void free_tokens(Token *tokens)
{
    while (tokens != NULL) {
        Token *next = tokens->next;

        free(tokens->value);
        free(tokens);

        tokens = next;
    }
}

static const char *token_type_name(TokenType type)
{
    switch (type) {
    case TOKEN_WORD:
        return "WORD";
    case TOKEN_PIPE:
        return "OP_PIPE";
    case TOKEN_AMP:
        return "OP_AMP";
    case TOKEN_SEMI:
        return "OP_SEMI";
    case TOKEN_LT:
        return "OP_LT";
    case TOKEN_GT:
        return "OP_GT";
    case TOKEN_GTGT:
        return "OP_GTGT";
    }

    return "UNKNOWN";
}

void print_tokens(const Token *tokens)
{
    while (tokens != NULL) {
        if (tokens->type == TOKEN_WORD) {
            printf(
                "%s(\"%s\")\n",
                token_type_name(tokens->type),
                tokens->value
            );
        } else {
            printf("%s\n", token_type_name(tokens->type));
        }

        tokens = tokens->next;
    }
}