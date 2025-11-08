#include "shell.h"

#ifndef HISTORY_SIZE
#define HISTORY_SIZE 20
#endif

/* History storage */
static char* history[HISTORY_SIZE] = { NULL };
static int history_count = 0;

/* add a command string to history (duplicates the string) */
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
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        /* Trim leading spaces quickly (so "! 3" is not treated as "!") */
        char *trim = cmdline;
        while (*trim == ' ' || *trim == '\t') trim++;

        /* Handle empty line */
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

            /* Replace cmdline with a duplicate of the requested history entry */
            free(cmdline);
            cmdline = strdup(history[n - 1]);
            if (cmdline == NULL) {
                perror("strdup");
                continue;
            }
            /* proceed below: the resolved command (from history) will be tokenized
               and then added to history as a normal command */
        }

        /* At this point cmdline contains the actual command to execute.
           Add to history now (we store the exact text the user typed or resolved). */
        add_history_entry(cmdline);

        /* Tokenize and handle builtins/external commands */
        if ((arglist = tokenize(cmdline)) != NULL) {

            /* Provide a built-in `history` command that prints entries */
            /* Note: handle_builtin already handles builtins like cd/exit/help/jobs.
               We'll check for "history" explicitly before calling handle_builtin so
               it appears as a built-in command too. */
            if (strcmp(arglist[0], "history") == 0) {
                print_history();
            } else {
                /* NEW: Check if command is built-in */
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
        }

        free(cmdline);
    }

    /* cleanup before exiting */
    free_history();

    printf("\nShell exited.\n");
    return 0;
}
