#include "input.h"
#include "prompt.h"

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

        /*
         * A2:
         * We only consume the input for now.
         * Execution will be implemented later.
         */
        free(line);
    }

    cleanup_prompt();

    return EXIT_SUCCESS;
}