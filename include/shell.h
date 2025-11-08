#ifndef SHELL_H
#define SHELL_H
#define HISTORY_SIZE 20

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist);

/* NEW: handle built-in commands.
   Returns 1 if the command was a builtin and handled (so caller should NOT fork/exec),
   Returns 0 otherwise (so caller should proceed to execute() for external commands). */
int handle_builtin(char** arglist);

#endif // SHELL_H
