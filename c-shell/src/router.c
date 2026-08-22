#include "router.h"
#include "lexer.h"
#include "hop.h"
#include "peek.h"
#include "locate.h"
#include "reveal.h"

#include <string.h>

void router(Token *tokens)
{
    char *command = tokens->value;

    if (strcmp(command, "hop") == 0) {
        hop_handler(tokens->next);

    } else if (strcmp(command, "peek") == 0) {
        peek_handler(tokens->next);

    } else if (strcmp(command, "reveal") == 0) {
        reveal_handler(tokens->next);

    } else if (strcmp(command, "locate") == 0) {
        locate_handler(tokens->next);

    }else{

        builtin_handler(tokens->next);

    }
}