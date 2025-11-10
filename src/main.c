/* main.c - Readline-integrated shell main loop
 *
 * Adds Feature 6 Task 3: background jobs (&), jobs list, and zombie reaping.
 */

#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Readline headers */
#include <readline/readline.h>
#include <readline/history.h>

#ifndef HISTORY_SIZE
#define HISTORY_SIZE 20
#endif

/* ---------------- Local history helpers (unchanged) ---------------- */
static char* history[HISTORY_SIZE] = { NULL };
static int history_count = 0;

static void add_history_entry(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') return;
    if (history_count >= HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; ++i) history[i - 1] = history[i];
        history_count = HISTORY_SIZE - 1;
    }
    history[history_count++] = strdup(cmd);
    if (history[history_count - 1] == NULL) {
        perror("strdup");
        history_count--;
    }
}

static void print_history(void) {
    for (int i = 0; i < history_count; ++i)
        printf("%d %s\n", i + 1, history[i]);
}

static void free_history(void) {
    for (int i = 0; i < history_count; ++i) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
}

/* ---------------------- MAIN ---------------------- */
int main() {
    char *cmdline = NULL;
    char **arglist = NULL;

    while (1) {
        /* Reap any finished background jobs before showing prompt */
        reap_zombies();

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        /* Trim leading spaces */
        char *trim = cmdline;
        while (*trim == ' ' || *trim == '\t') trim++;

        if (*trim == '\0') {
            free(cmdline);
            continue;
        }

        /* Handle !n re-execution */
        if (trim[0] == '!') {
            if (trim[1] == '\0') {
                fprintf(stderr, "Invalid history reference: '!'\n");
                free(cmdline);
                continue;
            }

            char *endptr = NULL;
            long n = strtol(trim + 1, &endptr, 10);
            if (endptr == trim + 1 || *endptr != '\0' || n <= 0) {
                fprintf(stderr, "Invalid history reference: %s\n", trim);
                free(cmdline);
                continue;
            }
            if (n < 1 || n > history_count) {
                fprintf(stderr, "History index out of range: %ld\n", n);
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = strdup(history[n - 1]);
            if (cmdline == NULL) {
                perror("strdup");
                continue;
            }
            add_history(cmdline);
        } else {
            add_history(trim);
        }

        add_history_entry(cmdline);

        /* ---------- Command chaining or single command ---------- */
        if (strchr(cmdline, ';') != NULL) {
            execute_chained_input(cmdline);
        } else {
            if ((arglist = tokenize(cmdline)) != NULL) {
                /* intercept history builtin */
                if (strcmp(arglist[0], "history") == 0) {
                    print_history();
                } else if (strcmp(arglist[0], "jobs") == 0) {
                    print_jobs();
                } else if (!handle_builtin(arglist)) {
                    execute_single(arglist);
                }
                for (int i = 0; arglist[i] != NULL; i++)
                    free(arglist[i]);
                free(arglist);
                arglist = NULL;
            }
        }

        free(cmdline);
        cmdline = NULL;
    }

    free_history();
    printf("\nShell exited.\n");
    return 0;
}

