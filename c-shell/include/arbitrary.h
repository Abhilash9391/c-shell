#include "lexer.h"
#ifndef ARBITRARY_H
#define ARBITRARY_H


void builtin_handler(Token* values);
typedef struct input{
        bool *input_redirection ;
        char *input_files  ;
    } Input;

    typedef struct output{
        bool *output_redirection ;
        char *output_files ;
    } Output;

#endif