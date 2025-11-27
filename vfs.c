#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

struct fuse_operations users_operations;

void execute_command(char** cmd) {
    pid_t pid = fork();
    if (pid==0) { execvp(cmd[0], cmd); exit(EXIT_FAILURE); }
    else if(pid>0) wait(NULL);
}

int can_login(struct passwd* pwd) {
    FILE* shells = fopen("/etc/shells","r");
    if(!shells) return 0;
    char line[256];
    while(fgets(line,sizeof(line),shells)) {
        line[strcspn(line,"\n")=0;
        if(strcmp(line,pwd->pw_shell)==0){ fclose(shells); return 1;}
    }
    fclose(shells);
    return 0;
}

int users_getattr(const char* path, struct stat* st, struct fuse_file_info* fi) {
    (void)fi;
    memset(st,0,sizeof(struct stat));
    time_t now = time(NULL);

    if(strcmp(path,"/")==0){
        st->st_mode = __S_IFDIR|0755;
        st->st_nlink=2;
        return 0;
    }

    char username[256], filename[256];
    if(sscanf(path,"/%255[^/]/%255[^/]",username,filename)==2){
        struct passwd* pwd=getpwnam(username);
        if(!pwd || !can_login(pwd)) return -ENOENT;
        st->st_mode=__S_IFREG|0644;
        st->st_nlink=1;
        if(strcmp(filename,"id")==0) st->st_size=snprintf(NULL,0,"%d",pwd->pw_uid);
        else if(strcmp(filename,"home")==0) st->st_size=strlen(pwd->pw_dir);
        else st->st_size=strlen(pwd->pw_shell);
        return 0;
    }
    if(sscanf(path,"/%255[^/]",username)==1) {
        struct passwd* pwd=getpwnam(username);
        if(!pwd || !can_login(pwd)) return -ENOENT;
        st->st_mode=__S_IFDIR|0755;
        st->st_nlink=2;
        return 0;
    }
    return -ENOENT;
}

int users_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags){
    (void)offset;(void)fi;(void)flags;
    filler(buf,".",NULL,0,0);
    filler(buf,"..",NULL,0,0);
    if(strcmp(path,"/")==0){
        struct passwd* pwd; setpwent();
        while((pwd=getpwent())!=NULL){ if(can_login(pwd)) filler(buf,pwd->pw_name,NULL,0,0);}
        endpwent();
        return 0;
    }
    filler(buf,"id",NULL,0,0);
    filler(buf,"home",NULL,0,0);
    filler(buf,"shell",NULL,0,0);
    return 0;
}

int users_mkdir(const char* path, mode_t mode){
    (void)mode; char username[256];
    if(sscanf(path,"/%255[^/]",username)==1){
        struct passwd* pwd=getpwnam(username);
        if(pwd) return -EEXIST;
        char* cmd[]={"adduser","--disabled-password","--gecos","",username,NULL};
        execute_command(cmd);
        pwd=getpwnam(username);
        return pwd?0:-EIO;
    }
    return -EINVAL;
}

int users_rmdir(const char* path){
    char username[256];
    if(sscanf(path,"/%255[^/]",username)==1){
        struct passwd* pwd=getpwnam(username);
        if(!pwd) return -ENOENT;
        char* cmd[]={"userdel","--remove",username,NULL};
        execute_command(cmd);
        pwd=getpwnam(username);
        return pwd? -EIO:0;
    }
    return -EPERM;
}

void* fuse_thread(void* arg){
    (void)arg;
    struct fuse_args args = FUSE_ARGS_INIT(0,NULL);
    fuse_opt_add_arg(&args,"");
    fuse_opt_add_arg(&args,"-odefault_permissions");
    fuse_opt_add_arg(&args,"-oauto_unmount");
    struct fuse* fuse_instance = fuse_new(&args,&users_operations,sizeof(users_operations),NULL);
    char* mount = malloc(strlen(getenv("HOME"))+7);
    sprintf(mount,"%s/users",getenv("HOME"));
    mkdir(mount,0755);
    fuse_mount(fuse_instance,mount);
    fuse_loop(fuse_instance);
    return NULL;
}

void vfs_start(){
    pthread_t th;
    users_operations.getattr = users_getattr;
    users_operations.readdir = users_readdir;
    users_operations.mkdir = users_mkdir;
    users_operations.rmdir = users_rmdir;
    pthread_create(&th,NULL,fuse_thread,NULL);
}
