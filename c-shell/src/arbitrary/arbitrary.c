#include "arbitrary.h"
#include "command.h"
#include "pipeline.h"
#include "redirection.h"

#include <stdbool.h>

static bool has_pipe(Token *tokens)
{
    while (tokens != NULL) {
        if (tokens->type == TOKEN_PIPE) {
            return true;
        }

        tokens = tokens->next;
    }

    return false;
}

static void execute_single(Token *tokens)
{
    Input input = {
        .input_redirection = false,
        .files = NULL
    };

    Output output = {
        .output_redirection = false,
        .files = NULL
    };

    check_redirection(
        tokens->next,
        &input,
        &output
    );

    execute_command(
        tokens,
        &input,
        &output
    );

    free_input_files(input.files);
    free_output_files(output.files);
}

void arbitrary_handler(Token *tokens)
{
    if (tokens == NULL) {
        return;
    }

    if (has_pipe(tokens)) {
        execute_pipeline(tokens);
    } else {
        execute_single(tokens);
    }
}