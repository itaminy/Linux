#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>

#include "vfs.h"

static struct fuse_operations vfs_ops;

static int vfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { stbuf->st_mode = S_IFDIR | 0755; stbuf->st_nlink = 2; return 0; }
    char username[128]; char file[32];
    if (sscanf(path, "/%127[^/]/%31s", username, file) == 2) {
        struct passwd *pwd = getpwnam(username);
        if (!pwd) return -ENOENT;
        stbuf->st_mode = S_IFREG | 0644;
        if (strcmp(file,"id")==0) stbuf->st_size = snprintf(NULL,0,"%d",pwd->pw_uid);
        else if (strcmp(file,"home")==0) stbuf->st_size = strlen(pwd->pw_dir);
        else stbuf->st_size = strlen(pwd->pw_shell);
        return 0;
    }
    if (sscanf(path, "/%127s", username) == 1) {
        struct passwd *pwd = getpwnam(username);
        if (!pwd) return -ENOENT;
        stbuf->st_mode = S_IFDIR | 0755; stbuf->st_nlink = 2;
        return 0;
    }
    return -ENOENT;
}

static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    filler(buf,".",NULL,0,0);
    filler(buf,"..",NULL,0,0);
    if (strcmp(path,"/")==0) {
        struct passwd *pwd; setpwent();
        while ((pwd = getpwent())) {
            filler(buf,pwd->pw_name,NULL,0,0);
        }
        endpwent();
    } else {
        filler(buf,"id",NULL,0,0);
        filler(buf,"home",NULL,0,0);
        filler(buf,"shell",NULL,0,0);
    }
    return 0;
}

static int vfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    char username[128], file[32];
    if (sscanf(path, "/%127[^/]/%31s", username, file)!=2) return -ENOENT;
    struct passwd *pwd = getpwnam(username);
    if (!pwd) return -ENOENT;
    char content[256]; content[0]='\0';
    if (strcmp(file,"id")==0) snprintf(content,sizeof(content),"%d",pwd->pw_uid);
    else if (strcmp(file,"home")==0) snprintf(content,sizeof(content),"%s",pwd->pw_dir);
    else snprintf(content,sizeof(content),"%s",pwd->pw_shell);
    size_t len = strlen(content);
    if (offset>=len) return 0;
    if (offset+size>len) size=len-offset;
    memcpy(buf, content+offset, size);
    return size;
}

static void *fuse_thread(void *arg) {
    (void)arg;
    char *mount = getenv("HOME"); char mountpoint[256];
    snprintf(mountpoint,sizeof(mountpoint),"%s/users", mount);
    mkdir(mountpoint,0755);
    struct fuse_args args = FUSE_ARGS_INIT(0,NULL);
    struct fuse *fuse_inst = fuse_new(&args, &vfs_ops, sizeof(vfs_ops), NULL);
    fuse_mount(fuse_inst,mountpoint);
    fuse_loop(fuse_inst);
    return NULL;
}

void fuse_start() {
    vfs_ops.getattr = vfs_getattr;
    vfs_ops.readdir = vfs_readdir;
    vfs_ops.read = vfs_read;
    pthread_t tid; pthread_create(&tid,NULL,fuse_thread,NULL);
}

void print_disk_info(const char *disk) {
    printf("Mock disk info for %s (implement reading /dev if needed)\n", disk);
}
