#include "shell.h"

#include <stddef.h>
#include <strings.h> /* for bzero */

/* ------------------- existing functions (unchanged except minor edits) ------------------ */

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

/* ------------------- Variable store implementation (linked list) ------------------- */

typedef struct varnode {
    char *name;
    char *value;
    struct varnode *next;
} varnode_t;

static varnode_t *var_head = NULL;

/* set_var: add or update a variable */
int set_var(const char *name, const char *value) {
    if (name == NULL) return -1;
    if (name[0] == '\0') return -1;
    /* find existing */
    varnode_t *cur = var_head;
    while (cur != NULL) {
        if (strcmp(cur->name, name) == 0) {
            /* update */
            free(cur->value);
            cur->value = strdup(value ? value : "");
            if (cur->value == NULL) return -1;
            return 0;
        }
        cur = cur->next;
    }
    /* not found: create new node */
    varnode_t *node = (varnode_t*)malloc(sizeof(varnode_t));
    if (!node) return -1;
    node->name = strdup(name);
    node->value = strdup(value ? value : "");
    if (!node->name || !node->value) {
        free(node->name);
        free(node->value);
        free(node);
        return -1;
    }
    node->next = var_head;
    var_head = node;
    return 0;
}

/* get_var: return internal pointer to value or NULL */
const char* get_var(const char *name) {
    if (name == NULL) return NULL;
    varnode_t *cur = var_head;
    while (cur != NULL) {
        if (strcmp(cur->name, name) == 0) {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

/* print_all_variables: implement 'set' builtin behaviour (prints name=value) */
void print_all_variables(void) {
    varnode_t *cur = var_head;
    while (cur != NULL) {
        printf("%s=%s\n", cur->name, cur->value);
        cur = cur->next;
    }
}

/* free_all_variables: cleanup on shell exit */
void free_all_variables(void) {
    varnode_t *cur = var_head;
    while (cur != NULL) {
        varnode_t *next = cur->next;
        free(cur->name);
        free(cur->value);
        free(cur);
        cur = next;
    }
    var_head = NULL;
}

/* ------------------- builtins & if-then-else (slightly modified) ------------------- */

int handle_builtin(char** arglist) {
    if (arglist[0] == NULL) {
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        /* cleanup variables before exit */
        free_all_variables();
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
        printf("  set         - print defined shell variables (name=value)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    /* NEW: set builtin (print variables) */
    if (strcmp(arglist[0], "set") == 0) {
        print_all_variables();   // prints name=value
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

