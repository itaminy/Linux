#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <pthread.h>
#include "vfs_manager.h"

struct fuse_operations users_operations;

char* init_mountpoint() {
    char* home=getenv("HOME");
    if(!home){ fprintf(stderr,"No HOME\n"); exit(EXIT_FAILURE); }
    char* mount=malloc(strlen(home)+7);
    sprintf(mount,"%s/users",home);
    mkdir(mount,0755);
    return mount;
}

int can_login(struct passwd* pwd) {
    FILE* f=fopen("/etc/shells","r"); if(!f) return 0;
    char line[256];
    while(fgets(line,sizeof(line),f)){
        line[strcspn(line,"\n")]=0;
        if(strcmp(line,pwd->pw_shell)==0){ fclose(f); return 1; }
    }
    fclose(f); return 0;
}

int users_getattr(const char* path, struct stat* st, struct fuse_file_info* fi){
    (void)fi; time_t now=time(NULL); memset(st,0,sizeof(*st));
    if(strcmp(path,"/")==0){ st->st_mode=S_IFDIR|0755; st->st_nlink=2; st->st_size=4096; st->st_uid=getuid(); st->st_gid=getgid(); return 0; }
    char user[256],file[256];
    if(sscanf(path,"/%255[^/]/%255[^/]",user,file)==2){
        struct passwd* pwd=getpwnam(user);
        if(pwd && can_login(pwd)){ st->st_mode=S_IFREG|0644; st->st_uid=pwd->pw_uid; st->st_gid=pwd->pw_gid; if(strcmp(file,"id")==0) st->st_size=10; else if(strcmp(file,"home")==0) st->st_size=strlen(pwd->pw_dir); else st->st_size=strlen(pwd->pw_shell); return 0; }
        return -ENOENT;
    }
    if(sscanf(path,"/%255[^/]",user)==1){ struct passwd* pwd=getpwnam(user); if(pwd && can_login(pwd)){ st->st_mode=S_IFDIR|0755; st->st_nlink=2; return 0; } }
    return -ENOENT;
}

int users_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info* fi, enum fuse_readdir_flags flags){
    (void)off; (void)fi; (void)flags;
    filler(buf,".",NULL,0,0); filler(buf,"..",NULL,0,0);
    if(strcmp(path,"/")==0){ struct passwd* pwd; setpwent(); while((pwd=getpwent())) if(can_login(pwd)) filler(buf,pwd->pw_name,NULL,0,0); endpwent(); return 0; }
    filler(buf,"id",NULL,0,0); filler(buf,"home",NULL,0,0); filler(buf,"shell",NULL,0,0);
    return 0;
}

void* fuse_thread(void* arg){ (void)arg; struct fuse_args args=FUSE_ARGS_INIT(0,NULL); fuse_opt_add_arg(&args,""); fuse_opt_add_arg(&args,"-odefault_permissions"); fuse_opt_add_arg(&args,"-oauto_unmount"); struct fuse* f=fuse_new(&args,&users_operations,sizeof(users_operations),NULL); fuse_mount(f,"/opt/users"); fuse_loop(f); return NULL; }

void fuse_start(){ pthread_t t; pthread_create(&t,NULL,fuse_thread,NULL); }
