#include "input.h"
#include "lexer.h"
#include "prompt.h"
#include "parser.h"
#include "router.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    init_prompt();

    while (1) {
        print_prompt();

        char *line = read_input();

        if (line == NULL) {
            break;
        }

        Token *tokens = lex_line(line);

        if (tokens == NULL) {
            printf("cshell: invalid syntax\n");
        } else {
            if (!check_syntax(tokens, LINE)) {
                printf("cshell: invalid syntax\n");
            } else {
                router(tokens);
            }

            free_tokens(tokens);
        }

        free(line);
    }

    cleanup_prompt();

    return EXIT_SUCCESS;
}