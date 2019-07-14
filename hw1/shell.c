#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function
 * parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_cd, "cd", "changes working directory to given path"},
    {cmd_pwd, "pwd", "prints path of working directory"},
    {cmd_wait, "wait", "waits for background processes"}};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) { exit(0); }

/* Changes working directory to given path */
int cmd_cd(struct tokens *tokens) {
    if (tokens_get_length(tokens) != 2) {
        printf("nuh nuh nuh, only 1 argument please \n");
    } else if (chdir(tokens_get_token(tokens, 1)) == -1) {
        printf("cd to this directory not possible \n");
    }
    return 0;
}

/* Prints path of working directory */
int cmd_pwd(unused struct tokens *tokens) {
    char pwd[BUFSIZ];
    getcwd(pwd, sizeof(pwd));
    printf("%s\n", pwd);
    return 0;
}

int cmd_wait(unused struct tokens *tokens) {
    while (waitpid(-1, 0, 0))
        ;
    return 0;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO;

    /* Check if we are running interactively */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the
         * shell until it becomes a foreground process. We use SIGTTIN to pause
         * the shell. When the shell gets moved to the
         * foreground, we'll receive a SIGCONT. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Saves the shell's process id */
        shell_pgid = getpid();

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current termios to a variable, so it can be restored later.
         */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

int main(unused int argc, unused char *argv[]) {
    init_shell();

    static char line[4096];
    int line_num = 0;

    signal(SIGTTOU, SIG_IGN);

    /* Please only print shell prompts when standard input is not a tty */
    if (shell_is_interactive) fprintf(stdout, "%d: ", line_num);

    while (fgets(line, 4096, stdin)) {
        waitpid(-1, 0, WNOHANG);
        /* Split our line into words. */
        struct tokens *tokens = tokenize(line);

        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0));

        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);

        } else {
            pid_t pid = fork();
            int exit;
            if (pid == 0) {
                setpgrp();
                int argsLen = tokens_get_length(tokens);
                char *inputCmd = tokens_get_token(tokens, 0);
                if (!argsLen) return 0;

                // Resolve Path
                char *PATH = getenv("PATH");
                char *currDir = strtok(PATH, ":");
                char currPath[2048];

                while (currDir != NULL) {
                    strcpy(currPath, currDir);
                    strcat(currPath, "/");
                    strcat(currPath, inputCmd);
                    if (access(currPath, F_OK) != -1) {
                        inputCmd = currPath;
                        break;
                    } else {
                        currPath[0] = '\n';
                        currDir = strtok(NULL, ":");
                    }
                }

                char *args[argsLen + 1];
                int i = 0;
                while (i < argsLen) {
                    if (strcmp(tokens_get_token(tokens, i), "<") == 0) {
                        i++;
                        FILE *input = fopen(tokens_get_token(tokens, i), "r");
                        dup2(fileno(input), 0);
                        fclose(input);
                    } else if (strcmp(tokens_get_token(tokens, i), ">") == 0) {
                        i++;
                        FILE *output = fopen(tokens_get_token(tokens, i), "w");
                        dup2(fileno(output), 1);
                        fclose(output);
                    } else if (i == argsLen - 1 &&
                               strcmp(tokens_get_token(tokens, i), "&") == 0) {
                        i++;
                        continue;
                    } else {
                        args[i] = tokens_get_token(tokens, i);
                    }
                    i++;
                }

                args[argsLen] = NULL;
                return execv(inputCmd, args);

            } else {
                setpgid(pid, pid);
                tcsetpgrp(STDIN_FILENO, getpgid(pid));
                if (tokens_get_length(tokens) > 0 &&
                    strcmp(
                        tokens_get_token(tokens, tokens_get_length(tokens) - 1),
                        "&") != 0) {
                    waitpid(pid, &exit, 0);
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }

        if (shell_is_interactive)
            /* Please only print shell prompts when standard input is not a tty
             */
            fprintf(stdout, "%d: ", ++line_num);

        /* Clean up memory */
        tokens_destroy(tokens);
    }

    return 0;
}
