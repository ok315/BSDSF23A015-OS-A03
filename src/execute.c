/* src/execute.c
 * Support:
 *  - Input redirection:  cmd < infile
 *  - Output redirection: cmd > outfile
 *  - Single pipe:        cmd1 | cmd2
 *  - Command chaining:   cmd1 ; cmd2 ; cmd3
 *  - Background execution via &
 *
 * Updated to use add_job() returning job index (1-based) and to print correct job numbers.
 */

#include "shell.h"
#include <fcntl.h>  // open flags
#include <sys/stat.h>
#include <ctype.h>   // for isspace()
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void parse_side(char *tokens[], char *argv[], char **in_file, char **out_file) {
    int ai = 0;
    *in_file = NULL;
    *out_file = NULL;

    for (int i = 0; tokens[i] != NULL; ++i) {
        if (strcmp(tokens[i], "<") == 0) {
            if (tokens[i+1] == NULL) {
                fprintf(stderr, "syntax error: expected filename after '<'\n");
                argv[0] = NULL;
                return;
            }
            *in_file = tokens[i+1];
            ++i;
        } else if (strcmp(tokens[i], ">") == 0) {
            if (tokens[i+1] == NULL) {
                fprintf(stderr, "syntax error: expected filename after '>'\n");
                argv[0] = NULL;
                return;
            }
            *out_file = tokens[i+1];
            ++i;
        } else {
            if (ai < MAXARGS + 1) {
                argv[ai++] = tokens[i];
            }
        }
    }
    argv[ai] = NULL;
    if (ai == 0) argv[0] = NULL;
}

/* Helper to join tokens into a single command string for job entries */
static void join_tokens(char *arglist[], char *out, size_t outlen) {
    out[0] = '\0';
    int first = 1;
    for (int i = 0; arglist[i] != NULL; ++i) {
        if (!first) strncat(out, " ", outlen - strlen(out) - 1);
        strncat(out, arglist[i], outlen - strlen(out) - 1);
        first = 0;
    }
}

/* detect and remove trailing &; returns 1 if background, 0 otherwise
   Modifies arglist in-place (may shorten it).
*/
static int detect_background(char *arglist[]) {
    if (arglist == NULL) return 0;
    int i = 0;
    while (arglist[i] != NULL) i++;
    if (i == 0) return 0;

    /* Case 1: last token is single & */
    if (strcmp(arglist[i-1], "&") == 0) {
        arglist[i-1] = NULL;
        return 1;
    }

    /* Case 2: last token ends with '&' (e.g., "sleep&") */
    size_t len = strlen(arglist[i-1]);
    if (len > 0 && arglist[i-1][len - 1] == '&') {
        /* remove trailing '&' */
        if (len == 1) { /* token was just "&" but handled above; safe check */
            arglist[i-1] = NULL;
        } else {
            arglist[i-1][len - 1] = '\0';
        }
        return 1;
    }

    return 0;
}

int execute_single(char* arglist[]) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

    /* detect background for this arglist (will modify arglist) */
    int background = detect_background(arglist);

    // Count pipes and locate pipe position (if any)
    int pipe_count = 0;
    int pipe_pos = -1;
    for (int i = 0; arglist[i] != NULL; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_count++;
            if (pipe_pos == -1) pipe_pos = i;
        }
    }

    if (pipe_count > 1) {
        fprintf(stderr, "error: multiple pipes not supported (this shell supports a single '|').\n");
        return -1;
    }

    if (pipe_count == 0) {
        char *argv[MAXARGS + 2];
        char *in_file = NULL;
        char *out_file = NULL;
        parse_side(arglist, argv, &in_file, &out_file);
        if (argv[0] == NULL) {
            fprintf(stderr, "syntax error: no command to execute\n");
            return -1;
        }

        pid_t cpid = fork();
        if (cpid < 0) {
            perror("fork failed");
            return -1;
        }

        if (cpid == 0) {
            /* Child: set up redirections, then exec */
            if (in_file != NULL) {
                int fdin = open(in_file, O_RDONLY);
                if (fdin < 0) {
                    perror("open input file");
                    _exit(1);
                }
                if (dup2(fdin, STDIN_FILENO) < 0) {
                    perror("dup2 input");
                    close(fdin);
                    _exit(1);
                }
                close(fdin);
            }
            if (out_file != NULL) {
                int fdout = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fdout < 0) {
                    perror("open output file");
                    _exit(1);
                }
                if (dup2(fdout, STDOUT_FILENO) < 0) {
                    perror("dup2 output");
                    close(fdout);
                    _exit(1);
                }
                close(fdout);
            }
            execvp(argv[0], argv);
            perror("execvp");
            _exit(1);
        } else {
            if (background) {
                /* Parent: don't wait; add to jobs */
                char cmd[JOB_CMD_LEN];
                join_tokens(arglist, cmd, sizeof(cmd));
                int jid = add_job(cpid, cmd);
                if (jid >= 0)
                    printf("[%d] %d\n", jid, (int)cpid); /* print correct background job notification */
                else
                    printf("[?] %d\n", (int)cpid);
                return 0;
            } else {
                int status;
                waitpid(cpid, &status, 0);
                return 0;
            }
        }
    } else {
        /*********************
         * Single-pipe case  *
         *********************/
        char *left_tokens[MAXARGS + 2];
        char *right_tokens[MAXARGS + 2];
        int li = 0, ri = 0;
        for (int i = 0; arglist[i] != NULL; ++i) {
            if (i < pipe_pos) {
                left_tokens[li++] = arglist[i];
            } else if (i > pipe_pos) {
                right_tokens[ri++] = arglist[i];
            }
        }
        left_tokens[li] = NULL;
        right_tokens[ri] = NULL;

        /* For pipes, background semantics apply to the whole pipeline.
           Decide background by checking right_tokens and left_tokens for & as well.
        */
        int pipeline_background = background;
        if (!pipeline_background) {
            /* If background not already detected, check right side too */
            if (detect_background(right_tokens)) pipeline_background = 1;
            else if (detect_background(left_tokens)) pipeline_background = 1;
        }

        char *left_argv[MAXARGS + 2];
        char *right_argv[MAXARGS + 2];
        char *left_in = NULL, *left_out = NULL;
        char *right_in = NULL, *right_out = NULL;

        parse_side(left_tokens, left_argv, &left_in, &left_out);
        parse_side(right_tokens, right_argv, &right_in, &right_out);

        if (left_argv[0] == NULL || right_argv[0] == NULL) {
            fprintf(stderr, "syntax error: invalid command on either side of '|'\n");
            return -1;
        }

        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            return -1;
        }

        pid_t left_pid = fork();
        if (left_pid < 0) {
            perror("fork");
            close(pipefd[0]); close(pipefd[1]);
            return -1;
        }

        if (left_pid == 0) {
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2 pipe write");
                _exit(1);
            }
            close(pipefd[0]);
            close(pipefd[1]);

            if (left_in != NULL) {
                int fdin = open(left_in, O_RDONLY);
                if (fdin < 0) {
                    perror("open input file (left)");
                    _exit(1);
                }
                dup2(fdin, STDIN_FILENO);
                close(fdin);
            }
            if (left_out != NULL) {
                int fdout = open(left_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fdout < 0) {
                    perror("open output file (left)");
                    _exit(1);
                }
                dup2(fdout, STDOUT_FILENO);
                close(fdout);
            }

            execvp(left_argv[0], left_argv);
            perror("execvp (left)");
            _exit(1);
        }

        pid_t right_pid = fork();
        if (right_pid < 0) {
            perror("fork");
            close(pipefd[0]); close(pipefd[1]);
            return -1;
        }

        if (right_pid == 0) {
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2 pipe read");
                _exit(1);
            }
            close(pipefd[0]);
            close(pipefd[1]);

            if (right_in != NULL) {
                int fdin = open(right_in, O_RDONLY);
                if (fdin < 0) {
                    perror("open input file (right)");
                    _exit(1);
                }
                dup2(fdin, STDIN_FILENO);
                close(fdin);
            }
            if (right_out != NULL) {
                int fdout = open(right_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fdout < 0) {
                    perror("open output file (right)");
                    _exit(1);
                }
                dup2(fdout, STDOUT_FILENO);
                close(fdout);
            }

            execvp(right_argv[0], right_argv);
            perror("execvp (right)");
            _exit(1);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        if (pipeline_background) {
            /* For a background pipeline, add the pipeline's rightmost pid as the job pid */
            char cmd[JOB_CMD_LEN];
            join_tokens(arglist, cmd, sizeof(cmd));
            int jid = add_job(right_pid, cmd);
            if (jid >= 0)
                printf("[%d] %d\n", jid, (int)right_pid);
            else
                printf("[?] %d\n", (int)right_pid);
            /* don't wait */
            return 0;
        } else {
            int status;
            waitpid(left_pid, &status, 0);
            waitpid(right_pid, &status, 0);
            return 0;
        }
    }
}

/* ===========================================================
 *  Function: execute_chained_input
 *  Purpose:  Split a full input line into commands separated
 *            by ';' and execute each sequentially.
 * =========================================================== */
int execute_chained_input(char *input_line) {
    if (input_line == NULL) return 0;

    char *saveptr = NULL;
    char *segment = strtok_r(input_line, ";", &saveptr);

    while (segment != NULL) {
        // trim leading/trailing spaces
        while (*segment == ' ' || *segment == '\t') segment++;
        char *end = segment + strlen(segment) - 1;
        while (end > segment && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        if (*segment != '\0') {
            // Tokenize and execute this subcommand
            char **arglist = tokenize(segment);
            if (arglist != NULL) {
                execute_single(arglist); // call single-command executor
                for (int i = 0; arglist[i] != NULL; i++)
                    free(arglist[i]);
                free(arglist);
            }
        }

        segment = strtok_r(NULL, ";", &saveptr);
    }

    return 0;
}
