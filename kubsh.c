#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>

volatile sig_atomic_t sighup_received = 0;

void handle_sighup(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\nConfiguration reloaded\n", 24);
    sighup_received = 1;
}

// ---------------- History ----------------
#define MAX_HISTORY 10
char* history_buffer[MAX_HISTORY];
int history_start = 0;
int history_size = 0;

void add_command_to_history(const char* cmd) {
    char* copy = strdup(cmd);
    if (history_size < MAX_HISTORY) {
        history_buffer[(history_start + history_size) % MAX_HISTORY] = copy;
        history_size++;
    } else {
        free(history_buffer[history_start]);
        history_buffer[history_start] = copy;
        history_start = (history_start + 1) % MAX_HISTORY;
    }
}

// ---------------- Utility ----------------
void expand_tilde(char** command) {
    char* tilde = strchr(*command, '~');
    while (tilde) {
        char* home = getenv("HOME");
        if (!home) exit(EXIT_FAILURE);
        char* expanded = malloc(strlen(*command) + strlen(home) + 1);
        size_t prefix = tilde - *command;
        strncpy(expanded, *command, prefix);
        expanded[prefix] = '\0';
        strcat(expanded, home);
        strcat(expanded, tilde + 1);
        free(*command);
        *command = expanded;
        tilde = strchr(*command, '~');
    }
}

// ---------------- Disk Info ----------------
typedef enum { CMD_OK, CMD_UNKNOWN, CMD_ERROR } CommandStatus;
#define SECTOR_SIZE 512

CommandStatus print_mbr(const char* disk) {
    int fd = open(disk, O_RDONLY);
    if (fd < 0) return CMD_ERROR;
    unsigned char sector[SECTOR_SIZE];
    if (read(fd, sector, SECTOR_SIZE) != SECTOR_SIZE) { close(fd); return CMD_ERROR; }
    close(fd);
    for (int i=0;i<4;i++){
        unsigned char* part = &sector[0x1BE + 16*i];
        if (part[4]==0) continue;
        uint32_t start = *(uint32_t*)&part[8];
        uint32_t sectors = *(uint32_t*)&part[12];
        printf("Partition %d: start=%u, sectors=%u, size=%u MiB\n", i+1, start, sectors, sectors/2048);
    }
    return CMD_OK;
}

// ---------------- Command Executor ----------------
CommandStatus execute_command_str(const char* cmd) {
    if (strncmp(cmd, "echo ", 5) == 0) {
        printf("%s\n", cmd+5);
        return CMD_OK;
    }
    if (strncmp(cmd, "\\e $", 4) == 0) {
        char* var = (char*)cmd+4;
        char* val = getenv(var);
        if (!val) { fprintf(stderr,"Variable not found\n"); return CMD_ERROR; }
        char* token = strtok(strdup(val), ":");
        while (token) { printf("%s\n", token); token = strtok(NULL, ":"); }
        return CMD_OK;
    }
    if (strncmp(cmd, "\\l ", 3) == 0) {
        char disk[256];
        snprintf(disk, sizeof(disk), "/dev/%s", cmd+3);
        return print_mbr(disk);
    }
    if (strcmp(cmd, "\\q")==0) exit(EXIT_SUCCESS);
    // execute binary
    pid_t pid = fork();
    if (pid==0) {
        char* args[] = {"/bin/sh","-c",(char*)cmd,NULL};
        execvp(args[0], args);
        exit(EXIT_FAILURE);
    } else if (pid>0) { wait(NULL); return CMD_OK; }
    return CMD_UNKNOWN;
}

// ---------------- Main ----------------
int main() {
    signal(SIGHUP, handle_sighup);
    setbuf(stdout,NULL);

    char* command = NULL;
    while (1) {
        command = readline("\033[1;34mKubSH> \033[0m");
        if (!command) {
            if (sighup_received) { sighup_received=0; continue; }
            else break;
        }
        if (*command) {
            add_history(command);
            add_command_to_history(command);
            expand_tilde(&command);
            if (execute_command_str(command)==CMD_UNKNOWN) {
                printf("%s: command not found\n", command);
            }
        }
        free(command);
    }
    return 0;
}
