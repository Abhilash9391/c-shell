#include "lexer.h"
#include "shell.h"
#include "arbitrary.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include<stdio.h>

void check_redirection(Token *values,Input *input,Output *output){

    while(values!=NULL){
        TokenType *token = values->type;
        if(*token == TOKEN_GTGT){
            //output_redirection

        }else if(*token == TOKEN_GT){
            //output_redirection

        }else if(*token == TOKEN_LT){
            //input_redirection

        }

        values = values->next;
    }





}
void redirect(){

}
static bool check_executable(const char *path)
{
    return access(path, X_OK) == 0;
}

static char  *check_currentdirectory(
    const char *name,
    bool *found
)
{
    char path[PATH_MAX];

    if (snprintf(
            path,
            sizeof(path),
            "./%s",
            name
        ) >= (int)sizeof(path)) {
        return;
    }

    if (!is_executable(path)) {
        return;
    }

    char *absolute = absolute_path(path);

    if (absolute != NULL) {
        *found = true;
        return absolute;
        
      
    }
}

static char *path_search(
    const char *name,
    bool *found
)
{
    char *path_env = getenv("PATH");

    if (path_env == NULL) {
        return;
    }

    char *path_copy = strdup(path_env);

    if (path_copy == NULL) {
        perror("strdup");
        return;
    }

    char *directory = strtok(path_copy, ":");

    while (directory != NULL) {
        if (directory[0] == '\0') {
            directory = ".";
        }

        char path[PATH_MAX];

        if (snprintf(
                path,
                sizeof(path),
                "%s/%s",
                directory,
                name
            ) < (int)sizeof(path)) {

            if (is_executable(path)) {
                char *absolute = absolute_path(path);

                if (absolute != NULL) {
                   
                    
                    *found = true;
                    return absolute;
                }
            }
        }

        directory = strtok(NULL, ":");
    }

    free(path_copy);
}



void check_command(char *value,bool *check_currdir,bool *check_path){

    if(1){

        // / present 
        

    }else if(1){
        // % present

    }else if(1){

    }

}

void execute(char* absolute_path){

}

void command_handler(char *value){
    bool *check_currdir = false;
    bool *check_path = false;
    bool *found = false;
    check_command(value,&check_currdir,&check_path);
    char *absolute_path;
    if(*check_currdir){
       absolute_path =  check_currentdirectory(value,found);

    }
    if(!found && check_path){
       absolute_path =  path_search(value,found);
    }

    //execute()




}

void builtin_handler(Token *values){



   Input *input ;
   input->input_redirection = false;
   input->input_files = NULL;

   Output *output ;
   output->output_redirection = false;
   output->output_files = NULL;

   

     
    char *command = values->value;
    check_redirection(values->next,input,output);
    if(input->input_redirection || output->output_redirection){
        redirect(&input,&output);
    }
    command_handler(command);



}
