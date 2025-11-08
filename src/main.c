/* main.c - Readline-integrated shell main loop
 *
 * Assumes shell.h already provides:
 *   - PROMPT macro/string
 *   - prototypes for tokenize(), handle_builtin(), execute(), etc.
 *
 * Link with -lreadline
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

/* History storage (kept for !n and printing via `history`) */
static char* history[HISTORY_SIZE] = { NULL };
static int history_count = 0;

/* add a command string to our local history array (duplicates the string) */
static void add_history_entry(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') return;

    /* If history is full, free the oldest and shift left */
    if (history_count >= HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; ++i) {
            history[i-1] = history[i];
        }
        history_count = HISTORY_SIZE - 1;
    }

    history[history_count++] = strdup(cmd);
    if (history[history_count-1] == NULL) {
        perror("strdup");
        /* If strdup failed, repair count */
        history_count--;
    }
}

/* print the history with 1-based numbering */
static void print_history(void) {
    for (int i = 0; i < history_count; ++i) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

/* free all stored history entries */
static void free_history(void) {
    for (int i = 0; i < history_count; ++i) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
}

int main() {
    char* cmdline = NULL;
    char** arglist = NULL;

    /* Optional: initialize readline completion or settings here if desired */

    while ((cmdline = readline(PROMPT)) != NULL) {

        /* readline returns a malloc'd string — must free eventually */

        /* Trim leading spaces quickly (so "! 3" is not treated as "!") */
        char *trim = cmdline;
        while (*trim == ' ' || *trim == '\t') trim++;

        /* Handle empty line (just Enter) */
        if (*trim == '\0') {
            free(cmdline);
            continue;
        }

        /* Special: re-execution with !n (must be handled BEFORE tokenization
           and BEFORE adding the literal "!n" string to history) */
        if (trim[0] == '!') {
            /* parse the number that follows '!' */
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

            /* Replace cmdline with a duplicate of the requested history entry.
               Free the readline-provided cmdline first. */
            free(cmdline);
            cmdline = strdup(history[n - 1]);
            if (cmdline == NULL) {
                perror("strdup");
                continue;
            }

            /* Also add the resolved command to the Readline history so Up/Down
               will include commands re-run via !n. This mirrors the behavior
               of typing the command yourself. */
            add_history(cmdline);
        } else {
            /* Not a !n reference — add the exact user-typed line to Readline history */
            /* We add it here (after verifying not-empty) so literal "!n" does
               not get added when user intended to reference history. */
            add_history(trim);
            /* Note: readline's add_history expects a char*; passing `trim` which
               points into `cmdline` is fine because readline copies the text
               internally into its history data structures. */
        }

        /* Now also add to our local history array (for printing & !n indexing) */
        /* We store the exact resolved command text (cmdline may be strdup'd or original) */
        add_history_entry(cmdline);

        /* Tokenize and handle builtins/external commands */
        if ((arglist = tokenize(cmdline)) != NULL) {

            /* Provide a built-in `history` command that prints entries */
            if (strcmp(arglist[0], "history") == 0) {
                print_history();
            } else {
                /* Check and handle other builtins */
                if (!handle_builtin(arglist)) {
                    /* Not a built-in → run external command */
                    execute(arglist);
                }
            }

            /* Free the memory allocated by tokenize() */
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
            arglist = NULL;
        }

        free(cmdline);
        cmdline = NULL;
    }

    /* cleanup before exiting */
    free_history();

    /* Optionally, write readline history to a file with write_history() if you want persistent history */

    printf("\nShell exited.\n");
    return 0;
}
