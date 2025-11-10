/* main.c - Readline-integrated shell main loop
 *
 * Adds Feature 6 Task 3: background jobs (&), jobs list, and zombie reaping.
 * Also extended to support multi-line if-then-else-fi blocks (Feature-7).
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

/* Helper: check whether a line (possibly with leading/trailing spaces) is exactly "fi" */
static int is_fi_line(const char *line) {
    if (line == NULL) return 0;
    /* skip leading whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "fi", 2) != 0) return 0;
    p += 2;
    /* skip trailing whitespace */
    while (*p == ' ' || *p == '\t') p++;
    return *p == '\0';
}

/* ---------------------- MAIN ---------------------- */
int main() {
    char *cmdline = NULL;
    char **arglist = NULL;

    while (1) {
        /* Reap any finished background jobs before showing prompt */
        reap_zombies();

        /* Read the first line of input */
        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        /* Trim leading spaces to examine the command start */
        char *trim = cmdline;
        while (*trim == ' ' || *trim == '\t') trim++;

        if (*trim == '\0') {
            free(cmdline);
            continue;
        }

        /* Handle !n re-execution (history reference) */
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
            /* set trim to the retrieved line */
            trim = cmdline;
            while (*trim == ' ' || *trim == '\t') trim++;
            /* add to readline history so readline knows about it */
            add_history(cmdline);
        } else {
            /* For normal single-line start, add to readline's history (this is lightweight) */
            add_history(trim);
        }

        /*
         * If this is an 'if' start (after leading spaces), collect a multi-line block
         * until a line that is exactly 'fi' (ignoring leading/trailing spaces) is entered.
         */
        if (strncmp(trim, "if", 2) == 0 && (trim[2] == ' ' || trim[2] == '\t' || trim[2] == '\0')) {
            /* Build a combined block string: start with the first trimmed line */
            size_t total_len = strlen(trim) + 1; /* include '\0' */
            char *block = (char*)malloc(total_len);
            if (!block) {
                perror("malloc");
                free(cmdline);
                continue;
            }
            strcpy(block, trim);

            /* Read continuation lines until "fi" */
            while (1) {
                char *cont = readline("> ");
                if (cont == NULL) {
                    /* EOF during block input -- abort this block */
                    fprintf(stderr, "\nUnexpected EOF while reading if-block; aborting block\n");
                    free(cont);
                    free(block);
                    block = NULL;
                    break;
                }

                /* Append a newline and the continuation line to block */
                size_t newlen = total_len + strlen("\n") + strlen(cont);
                char *tmp = realloc(block, newlen);
                if (tmp == NULL) {
                    perror("realloc");
                    free(cont);
                    free(block);
                    block = NULL;
                    break;
                }
                block = tmp;
                /* append newline and cont */
                strcat(block, "\n");
                strcat(block, cont);
                total_len = newlen;

                /* Check for fi line (only cont's contents) */
                if (is_fi_line(cont)) {
                    free(cont);
                    break; /* block complete */
                }
                free(cont);
            }

            /* If block reading succeeded, replace cmdline with block (and update trim) */
            if (block != NULL) {
                free(cmdline); /* free original single-line buffer */
                cmdline = block;
                trim = cmdline;
                while (*trim == ' ' || *trim == '\t') trim++;
                /* Also add the full block to our custom history structure */
                add_history_entry(cmdline);
            } else {
                /* Block aborted due to EOF or memory failure — skip execution */
                continue;
            }
        } else {
            /* Not an if-block: record the single-line command in our custom history */
            add_history_entry(cmdline);
        }

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
                    /*
                     * If this was an if-block, the tokenization will treat the whole block
                     * as a single token stream. We rely on a helper to detect and execute
                     * the if-then-else block (implemented in shell.c as handle_if_then_else).
                     */
                    if (handle_if_then_else(cmdline)) {
                        /* handled by if-block executor */
                    } else {
                        execute_single(arglist);
                    }
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
