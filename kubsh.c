#define FUSE_USE_VERSION 31
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "vfs.h"

#define HISTORY_FILE ".kubsh_history"

volatile sig_atomic_t sighup_received = 0;

void handle_sighup(int sig) {
    (void)sig;
    const char msg[] = "\nConfiguration reloaded\n";
    write(STDOUT_FILENO, msg, sizeof(msg)-1);
    sighup_received = 1;
}

void load_history() {
    char *home = getenv("HOME");
    if (!home) home = "";
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILE);
    read_history(path);
}

void save_history_to_file() {
    char *home = getenv("HOME");
    if (!home) home = "";
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILE);
    write_history(path);
}

void expand_tilde(char **cmd) {
    char *tilde = strchr(*cmd, '~');
    if (!tilde) return;
    char *home = getenv("HOME");
    if (!home) home = "";
    char buf[1024];
    size_t pre = tilde - *cmd;
    strncpy(buf, *cmd, pre);
    buf[pre] = '\0';
    strcat(buf, home);
    strcat(buf, tilde + 1);
    free(*cmd);
    *cmd = strdup(buf);
}

int execute_external_command(const char *cmd) {
    if (strncmp(cmd, "echo ", 5) == 0) {
        printf("%s\n", cmd + 5);
        return 1;
    }
    if (strncmp(cmd, "\\e $", 4) == 0) {
        char *var = getenv(cmd + 4);
        if (var) {
            char *token = strtok(strdup(var), ":");
            while (token) {
                printf("%s\n", token);
                token = strtok(NULL, ":");
            }
        }
        return 1;
    }
    if (strncmp(cmd, "\\l ", 3) == 0) {
        char disk[256];
        snprintf(disk, sizeof(disk), "/dev/%s", cmd+3);
        print_disk_info(disk);
        return 1;
    }
    if (strncmp(cmd, "cd ", 3) == 0) {
        if (chdir(cmd + 3) != 0) perror("cd");
        return 1;
    }
    if (strcmp(cmd, "debug") == 0 || strncmp(cmd, "debug ", 6) == 0) {
        printf("%s\n", cmd + 6);
        return 1;
    }
    return 0;
}

int main() {
    signal(SIGHUP, handle_sighup);
    load_history();
    fuse_start(); // запускаем VFS в отдельном потоке

    while (1) {
        if (sighup_received) sighup_received = 0;
        char *input = readline("KubSH> ");
        if (!input) break;

        expand_tilde(&input);

        if (strlen(input) == 0) { free(input); continue; }
        add_history(input);

        if (strcmp(input, "\\q") == 0) {
            free(input);
            break;
        }

        if (!execute_external_command(input)) {
            pid_t pid = fork();
            if (pid == 0) {
                execlp("/bin/sh", "sh", "-c", input, NULL);
                perror("exec");
                exit(1);
            } else if (pid > 0) {
                wait(NULL);
            } else {
                perror("fork");
            }
        }
        free(input);
        save_history_to_file();
    }
    return 0;
}
