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

/* Job support */
#define MAX_JOBS 64
#define JOB_CMD_LEN 256

typedef struct {
    pid_t pid;
    char cmd[JOB_CMD_LEN];
} job_t;

/* Function prototypes */
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);

/* executor prototypes:
   - execute_single: executes a single tokenized command (fork/exec/wait or pipe handling)
   - execute_chained_input: takes a raw input line and handles semicolon-separated chaining
*/
int execute_single(char** arglist);
int execute_chained_input(char* input_line);

/* Job manager prototypes
   add_job now returns the job index (1-based) on success, -1 on failure.
*/
int add_job(pid_t pid, const char *cmd);
void remove_job(pid_t pid);
void print_jobs(void);
void reap_zombies(void); /* reap finished background children (WNOHANG) */

/* NEW: handle built-in commands.
   Returns 1 if builtin handled, 0 otherwise */
int handle_builtin(char** arglist);

/* NEW: handle_if_then_else parses & executes a collected if-then-else-fi block.
   It takes the full multiline block as a single string (with newlines) and
   returns 1 if it handled the block, 0 otherwise.
*/
int handle_if_then_else(char *cmdline);

#endif // SHELL_H
