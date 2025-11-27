#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include "vfs.h"

#define HISTORY_PATH "/.kubsh_history"

static volatile sig_atomic_t reload_cfg = 0;

// === Задание 9: обработка SIGHUP ===
void sighup_handler(int sig) {
    (void)sig;
    write(1, "Configuration reloaded\n", 24);
    reload_cfg = 1;
}

// === выполнение бинарника (задание 8) ===
void run_binary(char *cmd, char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, args);
        perror("execvp");
        exit(1);
    } else wait(NULL);
}

// === echo (задание 5) ===
void cmd_echo(char *str) {
    printf("%s\n", str);
}

// === вывод переменной окружения (задание 7) ===
void cmd_env(char *var) {
    char *v = getenv(var);
    if (!v) { printf("Variable not found\n"); return; }

    char *tmp = strdup(v);
    char *tok = strtok(tmp, ":");
    while (tok) {
        printf("%s\n", tok);
        tok = strtok(NULL, ":");
    }
    free(tmp);
}

// === вывод информации о диске (задание 10) ===
void cmd_list_disk(char *path) {
    printf("== Disk partitions for %s ==\n", path);
    system("lsblk");
}

// === обработка команд (задания 1–8,10) ===
void process_command(char *line) {
    if (!strcmp(line, "\\q")) exit(0);             // 3 — команда выхода
    if (!strncmp(line, "echo ", 5)) {              // 5 — echo
        cmd_echo(line + 5);
        return;
    }
    if (!strncmp(line, "\\e $", 4)) {              // 7 — вывод переменной окружения
        cmd_env(line + 4);
        return;
    }
    if (!strncmp(line, "\\l ", 3)) {               // 10 — вывод списка разделов
        cmd_list_disk(line + 3);
        return;
    }

    // Выполнение бинарника (8)
    char *args[32];
    int i = 0;
    char *tok = strtok(line, " ");
    while (tok && i < 31) { args[i++] = tok; tok = strtok(NULL, " "); }
    args[i] = NULL;

    run_binary(args[0], args);
}

int main() {
    signal(SIGHUP, sighup_handler);

    // === запуск VFS (задание 11) ===
    start_vfs();

    // === история команд (задание 4) ===
    char histfile[256];
    sprintf(histfile, "%s%s", getenv("HOME"), HISTORY_PATH);
    read_history(histfile);

    // === цикл обработки команд (задания 1–2) ===
    while (1) {
        char *line = readline("kubsh> ");
        if (!line) break;  // Ctrl+D — 2

        if (*line) add_history(line);
        process_command(line);
        free(line);
    }

    write_history(histfile);
    return 0;
}
