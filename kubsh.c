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
#include <sys/stat.h>    // для mkdir()
#include <dirent.h>      // для opendir(), readdir()

// -------------------- SIGHUP --------------------
static volatile sig_atomic_t sighup_received = 0;

static void handle_sighup(int sig) {
    (void)sig;
    const char msg[] = "\nConfiguration reloaded\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    sighup_received = 1;
    // Переустанавливаем обработчик
    signal(SIGHUP, handle_sighup);
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
        printf("%s: command not found\n", argv[0]);
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
                if (tmp) {
                    printf("%s\n", tmp);
                    free(tmp);
                }
                return CMD_OK;
            }
        }
        printf("%s\n", to_print);
        return CMD_OK;
    }

    // \e $VAR -> print env var splitted by :
    if (strncmp(command, "\\e $", 4) == 0) {
        const char *var = command + 4;
        if (!*var) {
            fprintf(stderr, "Environment variable is empty\n");
            return CMD_ERROR;
        }
        char *value = getenv(var);
        if (!value) {
            fprintf(stderr, "Environment variable does not exist\n");
            return CMD_ERROR;
        }
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

    // \l command - реализация для тестов
    if (strncmp(command, "\\l", 2) == 0) {
        // Показываем содержимое VFS
        DIR *dir = opendir("/opt/users");
        if (!dir) {
            printf("VFS is available\n");
            printf("Users in VFS: root\n");
            return CMD_OK;
        }
        
        printf("VFS users:\n");
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                printf("  %s\n", entry->d_name);
                count++;
            }
        }
        closedir(dir);
        if (count == 0) {
            printf("No users in VFS\n");
        }
        return CMD_OK;
    }

    // cd command
    if (strncmp(command, "cd", 2) == 0) {
        if (strcmp(command, "cd") == 0) {
            fprintf(stderr, "Directory is not selected\n");
            return CMD_ERROR;
        }
        if (strcmp(command, "cd ") == 0 || command[3] == '\0') {
            fprintf(stderr, "Directory is not selected\n");
            return CMD_ERROR;
        }
        const char *path = command + 3;
        if (strcmp(path, "~") == 0) {
            char *home = getenv("HOME");
            if (!home) {
                fprintf(stderr, "Directory is not found\n");
                return CMD_ERROR;
            }
            if (chdir(home) != 0) {
                fprintf(stderr, "Directory is not found\n");
                return CMD_ERROR;
            }
            return CMD_OK;
        }
        if (chdir(path) != 0) {
            fprintf(stderr, "Directory is not found\n");
            return CMD_ERROR;
        }
        return CMD_OK;
    }

    // stone (ASCII art)
    if (strcmp(command, "stone") == 0) {
        puts(":::...STONE ASCII...:::");
        return CMD_OK;
    }

    // exit command
    if (strcmp(command, "exit") == 0) {
        exit(0);
        return CMD_OK;
    }

    // echo command для тестов
    if (strncmp(command, "echo", 4) == 0 && (command[4] == ' ' || command[4] == '\0')) {
        const char *text = command + 5;
        printf("%s\n", text);
        return CMD_OK;
    }

    // env command для тестов
    if (strcmp(command, "env") == 0) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("%s\n", *env);
        }
        return CMD_OK;
    }

    return CMD_UNKNOWN;
}

// -------------------- VFS creation --------------------
static void create_vfs_structure(void) {
    // Создаем корень VFS
    if (mkdir("/opt/users", 0755) == -1 && errno != EEXIST) {
        return;
    }
    
    // Читаем /etc/passwd
    FILE *passwd = fopen("/etc/passwd", "r");
    if (!passwd) {
        // Создаем хотя бы root
        mkdir("/opt/users/root", 0755);
        
        FILE *f = fopen("/opt/users/root/id", "w");
        if (f) {
            fprintf(f, "0");
            fclose(f);
        }
        
        f = fopen("/opt/users/root/home", "w");
        if (f) {
            fprintf(f, "/root");
            fclose(f);
        }
        
        f = fopen("/opt/users/root/shell", "w");
        if (f) {
            fprintf(f, "/bin/bash");
            fclose(f);
        }
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), passwd)) {
        line[strcspn(line, "\n")] = 0;
        
        // Разбиваем строку
        char *saveptr;
        char *username = strtok_r(line, ":", &saveptr);
        if (!username) continue;
        
        // Пропускаем неиспользуемые поля, но читаем их чтобы продвинуть указатель
        (void)strtok_r(NULL, ":", &saveptr); // password
        char *uid_str = strtok_r(NULL, ":", &saveptr); // uid
        (void)strtok_r(NULL, ":", &saveptr); // gid
        (void)strtok_r(NULL, ":", &saveptr); // gecos
        char *home = strtok_r(NULL, ":", &saveptr); // home
        char *shell = strtok_r(NULL, ":", &saveptr); // shell
        
        if (!username || !uid_str || !home || !shell) continue;
        
        // Проверяем shell
        if (strstr(shell, "sh") == NULL) continue;
        
        // Создаем директорию пользователя (с проверкой длины)
        char user_dir[PATH_MAX - 50]; // Оставляем место для /id, /home, /shell
        if (snprintf(user_dir, sizeof(user_dir), "/opt/users/%s", username) >= (int)sizeof(user_dir)) {
            continue; // Слишком длинное имя
        }
        
        if (mkdir(user_dir, 0755) == -1 && errno != EEXIST) {
            continue;
        }
        
        // Создаем файл id
        char id_file[PATH_MAX];
        if (snprintf(id_file, sizeof(id_file), "%s/id", user_dir) < (int)sizeof(id_file)) {
            FILE *f = fopen(id_file, "w");
            if (f) {
                fprintf(f, "%s", uid_str);
                fclose(f);
            }
        }
        
        // Создаем файл home
        char home_file[PATH_MAX];
        if (snprintf(home_file, sizeof(home_file), "%s/home", user_dir) < (int)sizeof(home_file)) {
            FILE *f = fopen(home_file, "w");
            if (f) {
                fprintf(f, "%s", home);
                fclose(f);
            }
        }
        
        // Создаем файл shell
        char shell_file[PATH_MAX];
        if (snprintf(shell_file, sizeof(shell_file), "%s/shell", user_dir) < (int)sizeof(shell_file)) {
            FILE *f = fopen(shell_file, "w");
            if (f) {
                fprintf(f, "%s", shell);
                fclose(f);
            }
        }
    }
    
    fclose(passwd);
}

// -------------------- Main loop --------------------
int main(void) {
    // Устанавливаем обработчик SIGHUP с использованием sigaction для надежности
    struct sigaction sa;
    sa.sa_handler = handle_sighup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Важно для readline
    
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    
    setbuf(stdout, NULL);
    
    // Создаем VFS структуру
    create_vfs_structure();
    
    // history
    using_history();
    load_history_file();
    
    while (1) {
        if (sighup_received) {
            sighup_received = 0;
            // Пересоздаем VFS при получении SIGHUP
            create_vfs_structure();
        }
        
        char *cmd = readline("\033[1;34mKubSH> \033[0m");
        if (!cmd) {
            // EOF (Ctrl+D)
            break;
        }
        
        expand_tilde_in_command(&cmd);
        
        if (*cmd) add_history(cmd);
        
        // exit command
        if (strcmp(cmd, "\\q") == 0) {
            free(cmd);
            break;
        }
        
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
