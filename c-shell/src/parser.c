#include "parser.h"

#include <stdbool.h>

bool check_syntax(Token *tokens, VariableType current)
{
    while (tokens != NULL) {
        TokenType token = tokens->type;

        if (current == LINE) {
            if (token == TOKEN_WORD) {
                current = ARG;
            } else {
                return false;
            }
        } else if (current == ARG) {
            if (token == TOKEN_WORD) {
                current = ARG;
            } else if (token == TOKEN_LT ||
                       token == TOKEN_GT ||
                       token == TOKEN_GTGT) {
                current = TGT;
            } else if (token == TOKEN_PIPE ||
                       token == TOKEN_SEMI) {
                current = CMD;
            } else if (token == TOKEN_AMP) {
                current = BG;
            } else {
                return false;
            }
        } else if (current == CMD) {
            if (token == TOKEN_WORD) {
                current = ARG;
            } else {
                return false;
            }
        } else if (current == TGT) {
            if (token == TOKEN_WORD) {
                current = ARG;
            } else {
                return false;
            }
        } else if (current == BG) {
            if (token == TOKEN_WORD) {
                current = ARG;
            } else {
                return false;
            }
        } else {
            return false;
        }

        tokens = tokens->next;
    }

    if (current == LINE ||
        current == ARG ||
        current == BG) {
        return true;
    }

    return false;
}