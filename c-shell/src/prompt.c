#include "prompt.h"
#include "shell.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *shell_home = NULL;

void init_prompt(void)
{
    shell_home = getcwd(NULL, 0);
    shell.home_directory = shell_home;


    if (shell_home == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}

void print_prompt(void)
{
    char *cwd = NULL;
    char hostname[256];

    struct passwd *pw = getpwuid(getuid());

    if (pw == NULL) {
        perror("getpwuid");
        exit(EXIT_FAILURE);
    }

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }

    hostname[sizeof(hostname) - 1] = '\0';

    cwd = getcwd(NULL, 0);

    if (cwd == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    /*
     * Decide what path should be displayed.
     */
    if (strcmp(cwd, shell_home) == 0) {
        printf("<%s@%s:~> ", pw->pw_name, hostname);
    } else if (
        strncmp(cwd, shell_home, strlen(shell_home)) == 0 &&
        cwd[strlen(shell_home)] == '/'
    ) {
        printf(
            "<%s@%s:~%s> ",
            pw->pw_name,
            hostname,
            cwd + strlen(shell_home)
        );
    } else {
        printf(
            "<%s@%s:%s> ",
            pw->pw_name,
            hostname,
            cwd
        );
    }

    fflush(stdout);

    free(cwd);
}

void cleanup_prompt(void)
{
    free(shell_home);
    shell_home = NULL;
}