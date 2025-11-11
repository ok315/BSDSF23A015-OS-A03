/* main.c - Readline-integrated shell main loop
 *
 * Adds background jobs, multi-line if blocks,
 * and Variable Assignment & Expansion (Feature 8).
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

/* Helper: detect a line exactly == fi */
static int is_fi_line(const char *line) {
    if (!line) return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, "fi", 2) != 0) return 0;
    line += 2;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '\0';
}

/* ---------------------- MAIN ---------------------- */
int main() {
    char *cmdline = NULL;
    char **arglist = NULL;

    while (1) {
        reap_zombies();  // clean finished background jobs

        cmdline = readline(PROMPT);
        if (!cmdline) break;

        char *trim = cmdline;
        while (*trim == ' ' || *trim == '\t') trim++;
        if (*trim == '\0') {
            free(cmdline);
            continue;
        }

        /* History: !n recall */
        if (trim[0] == '!') {
            char *endptr;
            long n = strtol(trim + 1, &endptr, 10);
            if (*endptr != '\0' || n <= 0 || n > history_count) {
                fprintf(stderr, "Invalid history ref: %s\n", trim);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(history[n - 1]);
            trim = cmdline;
        }

        add_history(trim);
        add_history_entry(cmdline);

        /* Multi-line if-then-else-fi block */
        if (strncmp(trim, "if", 2) == 0 && (trim[2] == ' ' || trim[2] == '\t' || trim[2] == '\0')) {
            size_t len = strlen(trim) + 1;
            char *block = malloc(len);
            strcpy(block, trim);

            while (1) {
                char *cont = readline("> ");
                if (!cont) { free(block); break; }
                if (is_fi_line(cont)) {
                    size_t newlen = len + strlen("\nfi");
                    block = realloc(block, newlen);
                    strcat(block, "\nfi");
                    free(cont);
                    break;
                }
                size_t newlen = len + strlen("\n") + strlen(cont);
                block = realloc(block, newlen);
                strcat(block, "\n");
                strcat(block, cont);
                len = newlen;
                free(cont);
            }

            free(cmdline);
            cmdline = block;
            trim = cmdline;
        }

        /* ---------- Command chaining ---------- */
        if (strchr(cmdline, ';')) {
            execute_chained_input(cmdline);
            free(cmdline);
            continue;
        }

        /* ---------- Tokenize ---------- */
        arglist = tokenize(cmdline);
        if (!arglist) { free(cmdline); continue; }

        /* ---------- FEATURE 8: ASSIGNMENT ---------- */
        if (arglist[0] && strchr(arglist[0], '=') && arglist[1] == NULL) {
            char *eq = strchr(arglist[0], '=');
            if (eq && eq != arglist[0]) {
                size_t name_len = eq - arglist[0];
                char name[name_len + 1];
                strncpy(name, arglist[0], name_len);
                name[name_len] = '\0';
                set_var(name, eq + 1);
            }
            /* cleanup & skip execution */
            for (int i = 0; arglist[i]; i++) free(arglist[i]);
            free(arglist);
            free(cmdline);
            continue;
        }

        /* ---------- FEATURE 8: VARIABLE EXPANSION ---------- */
        for (int i = 0; arglist[i]; i++) {
            if (arglist[i][0] == '$' && arglist[i][1] != '\0') {
                const char *val = get_var(arglist[i] + 1);
                free(arglist[i]);
                arglist[i] = strdup(val ? val : "");
            }
        }

        /* ---------- Builtins & Execution ---------- */
        if (strcmp(arglist[0], "history") == 0) {
            print_history();
        } else if (strcmp(arglist[0], "jobs") == 0) {
            print_jobs();
        } else if (!handle_builtin(arglist)) {
            if (!handle_if_then_else(cmdline))
                execute_single(arglist);
        }

        for (int i = 0; arglist[i]; i++) free(arglist[i]);
        free(arglist);
        free(cmdline);
    }

    free_history();
    free_all_variables();
    printf("\nShell exited.\n");
    return 0;
}
