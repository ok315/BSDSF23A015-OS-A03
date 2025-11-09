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
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    // Allocate token array similar to previous layout
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char *cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        // Skip whitespace
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0' || *cp == '\n') break;

        // If it's a special single-char token: <, >, |
        if (*cp == '<' || *cp == '>' || *cp == '|') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++; // consume the special char
            continue;
        }

        // Quoted string handling
        if (*cp == '"' || *cp == '\'') {
            char quote = *cp;
            cp++; // skip opening quote
            int i = 0;
            while (*cp != '\0' && *cp != quote && i < ARGLEN - 1) {
                arglist[argnum][i++] = *cp;
                cp++;
            }
            arglist[argnum][i] = '\0';
            if (*cp == quote) cp++; // skip closing quote if present
            argnum++;
            continue;
        }

        // Normal word (stop at whitespace or special char)
        int i = 0;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '<' && *cp != '>' && *cp != '|' && *cp != '\n' && i < ARGLEN - 1) {
            arglist[argnum][i++] = *cp;
            cp++;
        }
        arglist[argnum][i] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL; // null-terminate token list
    return arglist;
}

int handle_builtin(char** arglist) {
    // If no command, do nothing
    if (arglist[0] == NULL) {
        return 1; // Command handled (it's just empty input)
    }

    // Built-in: exit
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);   // exit the shell process itself
    }

    // Built-in: cd <directory>
    if (strcmp(arglist[0], "cd") == 0) {
        char *targetDir = arglist[1];

        // If no argument, go to home directory
        if (targetDir == NULL) {
            targetDir = getenv("HOME");
            if (targetDir == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }

        // Try to change directory
        if (chdir(targetDir) != 0) { 
            perror("cd");
        }
        return 1;
    }

    // Built-in: help
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>    - change directory\n"); 
        printf("  exit       - exit the shell\n");
        printf("  help       - show this message\n");
        printf("  jobs       - job control not implemented yet\n");
        return 1;
    }

    // Built-in: jobs (placeholder)
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    // Not a builtin command
    return 0;
}
