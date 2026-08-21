#include "hop.h"
#include "shell.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char *path;
    unsigned long visits;
    time_t last_visit;
} FrecencyEntry;

static FrecencyEntry *entries = NULL;
static size_t entry_count = 0;
static size_t entry_capacity = 0;
static bool frecency_loaded = false;

static char *get_current_directory(void)
{
    char *cwd = getcwd(NULL, 0);

    if (cwd == NULL) {
        perror("getcwd");
        return NULL;
    }

    return cwd;
}

static char *get_frecency_file(void)
{
    const char *home = getenv("HOME");

    if (home == NULL) {
        return NULL;
    }

    size_t length = strlen(home) + 21;

    char *path = malloc(length);

    if (path == NULL) {
        return NULL;
    }

    snprintf(
        path,
        length,
        "%s/.c_shell_frecency",
        home
    );

    return path;
}

static void load_frecency(void)
{
    if (frecency_loaded) {
        return;
    }

    frecency_loaded = true;

    char *file = get_frecency_file();

    if (file == NULL) {
        return;
    }

    FILE *fp = fopen(file, "r");

    free(file);

    if (fp == NULL) {
        return;
    }

    char path[PATH_MAX];
    unsigned long visits;
    long last_visit;

    while (fscanf(
        fp,
        "%1023[^|]|%lu|%ld\n",
        path,
        &visits,
        &last_visit
    ) == 3) {

        if (entry_count == entry_capacity) {
            size_t new_capacity =
                entry_capacity == 0 ? 16 : entry_capacity * 2;

            FrecencyEntry *new_entries = realloc(
                entries,
                new_capacity * sizeof(*entries)
            );

            if (new_entries == NULL) {
                break;
            }

            entries = new_entries;
            entry_capacity = new_capacity;
        }

        entries[entry_count].path = strdup(path);

        if (entries[entry_count].path == NULL) {
            break;
        }

        entries[entry_count].visits = visits;
        entries[entry_count].last_visit = (time_t)last_visit;

        entry_count++;
    }

    fclose(fp);
}

static void save_frecency(void)
{
    char *file = get_frecency_file();

    if (file == NULL) {
        return;
    }

    FILE *fp = fopen(file, "w");

    free(file);

    if (fp == NULL) {
        return;
    }

    for (size_t i = 0; i < entry_count; i++) {
        fprintf(
            fp,
            "%s|%lu|%ld\n",
            entries[i].path,
            entries[i].visits,
            (long)entries[i].last_visit
        );
    }

    fclose(fp);
}

static void update_frecency(const char *path)
{
    time_t now = time(NULL);

    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            entries[i].visits++;
            entries[i].last_visit = now;

            save_frecency();
            return;
        }
    }

    if (entry_count == entry_capacity) {
        size_t new_capacity =
            entry_capacity == 0 ? 16 : entry_capacity * 2;

        FrecencyEntry *new_entries = realloc(
            entries,
            new_capacity * sizeof(*entries)
        );

        if (new_entries == NULL) {
            return;
        }

        entries = new_entries;
        entry_capacity = new_capacity;
    }

    entries[entry_count].path = strdup(path);

    if (entries[entry_count].path == NULL) {
        return;
    }

    entries[entry_count].visits = 1;
    entries[entry_count].last_visit = now;

    entry_count++;

    save_frecency();
}

static double frecency_score(
    const FrecencyEntry *entry
)
{
    time_t now = time(NULL);

    double age = difftime(
        now,
        entry->last_visit
    );

    if (age < 0) {
        age = 0;
    }

    return (double)entry->visits / (age + 1.0);
}

static char *find_frecency_match(const char *name)
{
    char *best_path = NULL;
    double best_score = -1.0;

    for (size_t i = 0; i < entry_count; i++) {
        if (strstr(entries[i].path, name) == NULL) {
            continue;
        }

        struct stat info;

        if (stat(entries[i].path, &info) == -1) {
            continue;
        }

        if (!S_ISDIR(info.st_mode)) {
            continue;
        }

        double score = frecency_score(
            &entries[i]
        );

        if (score > best_score) {
            char *new_path = strdup(
                entries[i].path
            );

            if (new_path == NULL) {
                free(best_path);
                return NULL;
            }

            free(best_path);
            best_path = new_path;
            best_score = score;
        }
    }

    return best_path;
}

static bool change_directory(const char *path)
{
    char *current = get_current_directory();

    if (current == NULL) {
        return false;
    }

    if (chdir(path) == -1) {
        free(current);
        return false;
    }

    char *new_directory = get_current_directory();

    if (new_directory == NULL) {
        free(current);
        return false;
    }

    free(shell.previous_directory);
    shell.previous_directory = current;

    update_frecency(new_directory);

    free(new_directory);

    return true;
}

void hop_handler(Token *values)
{
    load_frecency();

    while (values != NULL) {
        char *value = values->value;

        if (strcmp(value, "~") == 0) {
            change_directory(shell.home_directory);

        } else if (strcmp(value, ".") == 0) {
            char *current = get_current_directory();

            if (current != NULL) {
                update_frecency(current);
                free(current);
            }

        } else if (strcmp(value, "..") == 0) {
            change_directory("..");

        } else if (strcmp(value, "-") == 0) {
            if (shell.previous_directory != NULL) {
                change_directory(
                    shell.previous_directory
                );
            }

        } else {
            if (!change_directory(value)) {
                char *match = find_frecency_match(value);

                if (match == NULL ||
                    !change_directory(match)) {
                    printf(
                        "hop: no such directory\n"
                    );
                }

                free(match);
            }
        }

        values = values->next;
    }
}