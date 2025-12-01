#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "vfs.h"  // содержит декларации: void fuse_start(void); void print_disk_info(const char*);

// -------------------- SIGHUP --------------------
volatile sig_atomic_t sighup_received = 0;

static void handle_sighup(int sig) {
    (void)sig;  // подавляем предупреждение о неиспользуемом параметре

    const char msg[] = "\nConfiguration reloaded\n";

    // Используем write и явно игнорируем возвращаемое значение
    ssize_t ignored = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;  // подавляем предупреждение о неиспользованном возвращаемом значении

    // Отмечаем факт получения сигнала
    sighup_received = 1;
}
// -------------------- History --------------------
#define HISTORY_FILENAME ".kubsh_history"
static void load_history_file(void) {
    char *home = getenv("HOME");
    if (!home) return;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILENAME);
    read_history(path);
}
static void save_history_file(void) {
    char *home = getenv("HOME");
    if (!home) return;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILENAME);
    write_history(path);
}

// -------------------- Utilities --------------------
static void expand_tilde_in_command(char **command) {
    if (!command || !*command) return;
    char *p = strchr(*command, '~');
    if (!p) return;
    char *home = getenv("HOME");
    if (!home) return;
    size_t newlen = strlen(*command) + strlen(home) + 1;
    char *expanded = malloc(newlen);
    if (!expanded) return;
    size_t prefix = p - *command;
    memcpy(expanded, *command, prefix);
    expanded[prefix] = '\0';
    strcat(expanded, home);
    strcat(expanded, p + 1);
    free(*command);
    *command = expanded;
}

// -------------------- Command parsing/execution --------------------
static char **parse_command(char *command) {
    if (!command) return NULL;
    int capacity = 8;
    char **argv = malloc(sizeof(char*) * capacity);
    if (!argv) return NULL;
    int argc = 0;
    char *tok = strtok(command, " ");
    while (tok) {
        if (argc + 1 >= capacity) {
            capacity *= 2;
            argv = realloc(argv, sizeof(char*) * capacity);
            if (!argv) return NULL;
        }
        argv[argc++] = tok;
        tok = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    return argv;
}

static void execute_command(char **argv) {
    if (!argv || !argv[0]) return;
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        _exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
    } else {
        waitpid(pid, NULL, 0);
    }
}

// -------------------- Builtin / external command handler --------------------
typedef enum { CMD_OK, CMD_UNKNOWN, CMD_ERROR } CommandStatus;

static CommandStatus execute_external_command(const char *command) {
    if (!command) return CMD_UNKNOWN;

    // debug command: debug 'text' or debug text
    if (strncmp(command, "debug", 5) == 0 && (command[5] == ' ' || command[5] == '\0')) {
        const char *to_print = (command[5] == ' ') ? command + 6 : command + 5;
        if (*to_print == '\'') {
            size_t len = strlen(to_print);
            if (len > 1 && to_print[len-1] == '\'') {
                char *tmp = strndup(to_print+1, len-2);
                if (tmp) { printf("%s\n", tmp); free(tmp); }
                return CMD_OK;
            }
        }
        printf("%s\n", to_print);
        return CMD_OK;
    }

    // \e $VAR -> print env var splitted by :
    if (strncmp(command, "\\e $", 4) == 0) {
        const char *var = command + 4;
        if (!*var) { fprintf(stderr, "Environment variable is empty\n"); return CMD_ERROR; }
        char *value = getenv(var);
        if (!value) { fprintf(stderr, "Environment variable does not exist\n"); return CMD_ERROR; }
        char *copy = strdup(value);
        if (!copy) return CMD_ERROR;
        char *tok = strtok(copy, ":");
        while (tok) {
            puts(tok);
            tok = strtok(NULL, ":");
        }
        free(copy);
        return CMD_OK;
    }

    // \l disk_suffix  -> print disk partitions using print_disk_info
    if (strncmp(command, "\\l", 2) == 0) {
        const char *suf = command + 2;
        while (*suf == ' ') ++suf;
        if (*suf == '\0') { fprintf(stderr, "Disk is not selected\n"); return CMD_ERROR; }
        char disk[64];
        snprintf(disk, sizeof(disk), "/dev/%s", suf);
        print_disk_info(disk); // implemented in vfs.c
        return CMD_OK;
    }

    // cd command
    if (strncmp(command, "cd", 2) == 0) {
        if (command[2] == ' ' && command[3] != '\0') {
            if (chdir(command + 3) != 0) {
                fprintf(stderr, "Directory is not found\n");
                return CMD_ERROR;
            }
            return CMD_OK;
        } else {
            fprintf(stderr, "Directory is not selected\n");
            return CMD_ERROR;
        }
    }

    // stone (ASCII art)
    if (strcmp(command, "stone") == 0) {
        puts(":::...STONE ASCII...:::");
        return CMD_OK;
    }

    return CMD_UNKNOWN;
}
// -------------------- Main loop --------------------
int main(void) {
    signal(SIGHUP, handle_sighup);
    setbuf(stdout, NULL);

    // history
    using_history();
    load_history_file();

    // start VFS in background
    fuse_start();

    while (1) {
        if (sighup_received) { sighup_received = 0; }
        char *cmd = readline("\033[1;34mKubSH> \033[0m");
        if (!cmd) {
            // EOF (Ctrl+D)
            break;
        }

        expand_tilde_in_command(&cmd);

        if (*cmd) add_history(cmd);

        // exit command
        if (strcmp(cmd, "\\q") == 0) { free(cmd); break; }

        if (*cmd) {
            // save history each command
            save_history_file();

            // builtin / special handling
            CommandStatus status = execute_external_command(cmd);
            if (status == CMD_UNKNOWN) {
                // execute external binary
                char *cmdcopy = strdup(cmd);
                char **argv = parse_command(cmdcopy);
                if (argv) {
                    execute_command(argv);
                    free(argv);
                }
                free(cmdcopy);
            }
        }

        free(cmd);
    }

    // final history save
    save_history_file();
    return 0;
}
