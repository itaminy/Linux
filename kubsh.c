#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>

extern void fuse_start(void);  // VFS (задание 11)

volatile sig_atomic_t sighup_received = 0;

// === Задание 9: Обработка сигнала SIGHUP ===
void handle_sighup(int sig) {
    write(STDOUT_FILENO, "\nConfiguration reloaded\n", 24);
    sighup_received = 1;
}

// === Задания 1,2,3,5,6,7,8,10 ===
void execute_command(char* cmd) {
    // === Задание 3: команда выхода \q ===
    if (strcmp(cmd, "\\q") == 0) exit(0);

    // === Задание 5: echo ===
    if (strncmp(cmd, "echo ", 5) == 0) {
        printf("%s\n", cmd + 5);
        return;
    }

    // === Задание 7: вывод переменной окружения ===
    if (strncmp(cmd, "\\e $", 4) == 0) {
        char* val = getenv(cmd + 4);
        if (!val) { fprintf(stderr, "Env variable not set\n"); return; }
        char* tok = strtok(val, ":");
        while (tok) { printf("%s\n", tok); tok = strtok(NULL, ":"); }
        return;
    }

    // === Задание 10: информация о диске (stub) ===
    if (strncmp(cmd, "\\l ", 3) == 0) {
        printf("Disk info for %s (stub, real parsing in vfs.c)\n", cmd+3);
        return;
    }

    // === Задание 8: выполнение бинарника ===
    pid_t pid = fork();
    if (pid == 0) {
        execlp(cmd, cmd, NULL);
        perror("command not found");  // === Задание 6: проверка команды ===
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else perror("fork failed");
}

int main() {
    // === Задание 9: сигнал SIGHUP ===
    signal(SIGHUP, handle_sighup);

    // === Задание 11: старт VFS ===
    fuse_start();

    // === Задание 4: история команд ===
    using_history();

    // === Задания 1,2: цикл чтения команд с Ctrl+D ===
    while (1) {
        char* line = readline("KubSH> ");
        if (!line) {
            if (sighup_received) { sighup_received = 0; continue; }
            break;
        }

        if (*line) add_history(line);  // === Задание 4: добавление в историю ===
        execute_command(line);          // Выполнение команды
        free(line);
    }
    return 0;
}
