#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char *cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0' || *cp == '\n') break;

        if (*cp == '<' || *cp == '>' || *cp == '|') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++;
            continue;
        }

        if (*cp == '"' || *cp == '\'') {
            char quote = *cp;
            cp++;
            int i = 0;
            while (*cp != '\0' && *cp != quote && i < ARGLEN - 1) {
                arglist[argnum][i++] = *cp;
                cp++;
            }
            arglist[argnum][i] = '\0';
            if (*cp == quote) cp++;
            argnum++;
            continue;
        }

        int i = 0;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
               *cp != '<' && *cp != '>' && *cp != '|' && *cp != '\n' &&
               i < ARGLEN - 1) {
            arglist[argnum][i++] = *cp;
            cp++;
        }
        arglist[argnum][i] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

int handle_builtin(char** arglist) {
    if (arglist[0] == NULL) {
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        char *targetDir = arglist[1];
        if (targetDir == NULL) {
            targetDir = getenv("HOME");
            if (targetDir == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }
        if (chdir(targetDir) != 0) { 
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>    - change directory\n"); 
        printf("  exit        - exit the shell\n");
        printf("  help        - show this message\n");
        printf("  jobs        - job control not implemented yet\n");
        printf("  if ... then ... else ... fi - simple conditional\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;
}

// NEW: Handle if-then-else logic
int handle_if_then_else(char* cmdline) {
    if (strncmp(cmdline, "if ", 3) != 0) return 0; // Not an if command

    char *then_part = strstr(cmdline, "then");
    char *else_part = strstr(cmdline, "else");
    char *fi_part   = strstr(cmdline, "fi");

    if (!then_part || !fi_part) {
        fprintf(stderr, "Syntax error: missing 'then' or 'fi'\n");
        return 1;
    }

    // Split parts safely
    *then_part = '\0';
    then_part += 4; // skip 'then'
    if (else_part) {
        *else_part = '\0';
        else_part += 4; // skip 'else'
    }
    *fi_part = '\0';

    char *if_cmd = cmdline + 3;
    while (*if_cmd == ' ') if_cmd++;

    // Execute IF command
    int ret = system(if_cmd);

    if (ret == 0) {
        system(then_part);
    } else if (else_part) {
        system(else_part);
    }

    return 1; // handled
}
