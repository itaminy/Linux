#define FUSE_USE_VERSION 35

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "vfs_manager.h"

volatile sig_atomic_t sighup_received;

void handle_sighup(int sig) {
    (void)sig;
    char msg[] = "\nConfiguration reloaded\n";
    write(STDOUT_FILENO, msg, sizeof(msg)-1);
    sighup_received = 1;
}

typedef enum { CMD_OK, CMD_ERROR } CommandStatus;

int execute_command(char** args) {
    pid_t pid = fork();
    if(pid == 0) {
        execvp(args[0], args);
        fprintf(stderr, "%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    } else if(pid < 0) {
        fprintf(stderr, "Fork error\n");
        return -1;
    } else {
        wait(NULL);
    }
    return 0;
}

// \l /dev/sdX функции
#define SECTOR_SIZE 512
typedef enum { MBR, GPT, UNKNOWN } PartitionStyle;

PartitionStyle detect_partition_style(char* disk) {
    int fd = open(disk, O_RDONLY);
    if(fd < 0) return UNKNOWN;
    unsigned char sector[SECTOR_SIZE];
    if(read(fd, sector, SECTOR_SIZE) != SECTOR_SIZE) { close(fd); return UNKNOWN; }
    close(fd);
    if(sector[0x1FE] != 0x55 || sector[0x1FF] != 0xAA) return UNKNOWN;
    if(sector[0x1BE + 4] == 0xEE) return GPT;
    return MBR;
}

CommandStatus print_mbr(char* disk) {
    int fd = open(disk,O_RDONLY);
    if(fd<0){ perror("Disk open"); return CMD_ERROR; }
    unsigned char sector[SECTOR_SIZE];
    if(read(fd, sector, SECTOR_SIZE)!=SECTOR_SIZE){ close(fd); return CMD_ERROR; }
    close(fd);
    for(int i=0;i<4;i++){
        unsigned char* p=&sector[0x1BE+16*i];
        unsigned char type=p[4];
        if(type==0) continue;
        uint32_t start=*(uint32_t*)&p[8];
        uint32_t size=*(uint32_t*)&p[12];
        printf("Partition %d: start=%u, sectors=%u, size=%u MiB\n",i+1,start,size,size/2048);
    }
    return CMD_OK;
}

CommandStatus print_gpt(char* disk) {
    int fd=open(disk,O_RDONLY);
    if(fd<0){ perror("Disk open"); return CMD_ERROR; }
    if(lseek(fd,SECTOR_SIZE,SEEK_SET)<0){ close(fd); return CMD_ERROR; }
    unsigned char sector[SECTOR_SIZE];
    if(read(fd,sector,SECTOR_SIZE)!=SECTOR_SIZE){ close(fd); return CMD_ERROR; }
    uint32_t entries=*(uint32_t*)&sector[0x50];
    uint32_t entry_size=*(uint32_t*)&sector[0x54];
    uint64_t first_lba=*(uint64_t*)&sector[0x48];
    unsigned char* table=malloc(entries*entry_size);
    if(!table){ close(fd); return CMD_ERROR; }
    if(lseek(fd,first_lba*SECTOR_SIZE,SEEK_SET)<0 || read(fd,table,entries*entry_size)!=entries*entry_size){ free(table); close(fd); return CMD_ERROR; }
    for(uint32_t i=0;i<entries;i++){
        unsigned char* e=table+i*entry_size;
        int empty=1;
        for(int j=0;j<16;j++){ if(e[j]!=0){ empty=0; break; } }
        if(!empty){
            uint64_t start=*(uint64_t*)&e[32];
            uint64_t end=*(uint64_t*)&e[40];
            printf("Partition %d: first LBA=%" PRIu64 ", last LBA=%" PRIu64 ", size=%" PRIu64 " MiB\n", i+1,start,end,(end-start+1)/2048);
        }
    }
    free(table);
    close(fd);
    return CMD_OK;
}

int main() {
    signal(SIGHUP, handle_sighup);
    setbuf(stdout,NULL);
    fuse_start();

    char* command=NULL;
    char* prompt="\033[1;34mKubSH> \033[0m";

    while(1) {
        command=readline(prompt);
        if(!command){ printf("\n"); break; }

        if(sighup_received){ sighup_received=0; free(command); continue; }

        if(strlen(command)==0){ free(command); continue; }

        add_history(command);

        if(strcmp(command,"\\q")==0){ free(command); break; }

        if(strncmp(command,"\\e $",4)==0){
            char* var=command+4;
            char* val=getenv(var);
            if(val){
                char* copy=strdup(val);
                char* tok=strtok(copy,":");
                while(tok){ printf("%s\n",tok); tok=strtok(NULL,":"); }
                free(copy);
            } else { fprintf(stderr,"Variable %s not found\n",var); }
            free(command); continue;
        }

        if(strncmp(command,"\\l ",3)==0){
            char* disk=command+3;
            while(*disk==' ') disk++;
            PartitionStyle ps=detect_partition_style(disk);
            if(ps==MBR) print_mbr(disk);
            else if(ps==GPT) print_gpt(disk);
            else fprintf(stderr,"Unknown or unreadable partition style\n");
            free(command); continue;
        }

        // Выполнение бинарника
        char* args[128]; int i=0;
        char* token=strtok(command," ");
        while(token && i<127){ args[i++]=token; token=strtok(NULL," "); }
        args[i]=NULL;
        execute_command(args);

        free(command);
    }
    return 0;
}
