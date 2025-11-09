/* src/execute.c
 * Support:
 *  - Input redirection:  cmd < infile
 *  - Output redirection: cmd > outfile
 *  - Single pipe:         cmd1 | cmd2
 *
 * Notes:
 *  - This implementation handles exactly one '|' token (single pipe).
 *    If multiple '|' tokens appear, it will report an error.
 *  - Redirections for each side are honored. In each child we
 *    perform dup2 for pipe endpoints first, then file redirections --
 *    this allows a file redirection to override a pipe-targeted fd.
 */

#include "shell.h"
#include <fcntl.h>  // open flags
#include <sys/stat.h>

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
    if (ai == 0) argv[0] = NULL; // no command present
}

int execute(char* arglist[]) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

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
        // No pipe: parse whole arglist for redirections and execute single command
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
            int status;
            waitpid(cpid, &status, 0);
            return 0;
        }
    } else {
        /*********************
         * Single-pipe case  *
         *********************/
        // Build left_tokens and right_tokens arrays (both NULL-terminated)
        char *left_tokens[MAXARGS + 2];
        char *right_tokens[MAXARGS + 2];
        int li = 0, ri = 0;
        for (int i = 0; arglist[i] != NULL; ++i) {
            if (i < pipe_pos) {
                left_tokens[li++] = arglist[i];
            } else if (i > pipe_pos) {
                right_tokens[ri++] = arglist[i];
            } else {
                // skip the '|' token
            }
        }
        left_tokens[li] = NULL;
        right_tokens[ri] = NULL;

        // Prepare argv and redirection files for both sides
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

        // Fork left child (writer)
        pid_t left_pid = fork();
        if (left_pid < 0) {
            perror("fork");
            // close pipe fds
            close(pipefd[0]); close(pipefd[1]);
            return -1;
        }

        if (left_pid == 0) {
            /* Left child: write end -> STDOUT */
            // dup write end to STDOUT
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2 pipe write");
                _exit(1);
            }
            // Close both pipe fds (we duplicated the needed one)
            close(pipefd[0]);
            close(pipefd[1]);

            /* Handle file redirections for left side (they override pipe if present) */
            if (left_in != NULL) {
                int fdin = open(left_in, O_RDONLY);
                if (fdin < 0) {
                    perror("open input file (left)");
                    _exit(1);
                }
                if (dup2(fdin, STDIN_FILENO) < 0) {
                    perror("dup2 input (left)");
                    close(fdin);
                    _exit(1);
                }
                close(fdin);
            }
            if (left_out != NULL) {
                int fdout = open(left_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fdout < 0) {
                    perror("open output file (left)");
                    _exit(1);
                }
                if (dup2(fdout, STDOUT_FILENO) < 0) {
                    perror("dup2 output (left)");
                    close(fdout);
                    _exit(1);
                }
                close(fdout);
            }

            execvp(left_argv[0], left_argv);
            perror("execvp (left)");
            _exit(1);
        }

        // Fork right child (reader)
        pid_t right_pid = fork();
        if (right_pid < 0) {
            perror("fork");
            // attempt to clean up: kill left child? close fds
            close(pipefd[0]); close(pipefd[1]);
            return -1;
        }

        if (right_pid == 0) {
            /* Right child: read end -> STDIN */
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2 pipe read");
                _exit(1);
            }
            // Close both pipe fds
            close(pipefd[0]);
            close(pipefd[1]);

            /* Handle file redirections for right side */
            if (right_in != NULL) {
                int fdin = open(right_in, O_RDONLY);
                if (fdin < 0) {
                    perror("open input file (right)");
                    _exit(1);
                }
                if (dup2(fdin, STDIN_FILENO) < 0) {
                    perror("dup2 input (right)");
                    close(fdin);
                    _exit(1);
                }
                close(fdin);
            }
            if (right_out != NULL) {
                int fdout = open(right_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fdout < 0) {
                    perror("open output file (right)");
                    _exit(1);
                }
                if (dup2(fdout, STDOUT_FILENO) < 0) {
                    perror("dup2 output (right)");
                    close(fdout);
                    _exit(1);
                }
                close(fdout);
            }

            execvp(right_argv[0], right_argv);
            perror("execvp (right)");
            _exit(1);
        }

        // Parent: close unused pipe fds and wait for children
        close(pipefd[0]);
        close(pipefd[1]);

        int status;
        waitpid(left_pid, &status, 0);
        waitpid(right_pid, &status, 0);

        return 0;
    }
}
