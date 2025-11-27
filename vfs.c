#include "vfs.h"
#include <fuse3/fuse.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#define USERS_DIR "/users"

static char mountpoint[256];

static int users_getattr(const char* path, struct stat* st,
                         struct fuse_file_info* fi) {
    (void)fi;
    memset(st, 0, sizeof(struct stat));

    if (!strcmp(path, "/")) {               // /users
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    const char *user = path + 1;
    struct passwd *pw = getpwnam(user);
    if (pw) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 3;
        return 0;
    }

    return -ENOENT;
}

// Создание каталога пользователя → adduser
static int users_mkdir(const char *path, mode_t mode) {
    const char *username = path + 1;
    char cmd[256];
    sprintf(cmd, "sudo adduser --disabled-password --gecos \"\" %s", username);
    system(cmd);
    return 0;
}

// Удаление каталога → userdel
static int users_rmdir(const char *path) {
    const char *username = path + 1;
    char cmd[256];
    sprintf(cmd, "sudo userdel -r %s", username);
    system(cmd);
    return 0;
}

static struct fuse_operations ops = {
    .getattr = users_getattr,
    .mkdir = users_mkdir,
    .rmdir = users_rmdir,
};

static void *fuse_thread(void *unused) {
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    char *argv[] = { "usersfs", mountpoint };
    fuse_main(2, argv, &ops, NULL);
    return NULL;
}

void start_vfs() {
    sprintf(mountpoint, "%s%s", getenv("HOME"), USERS_DIR);
    mkdir(mountpoint, 0755);

    pthread_t t;
    pthread_create(&t, NULL, fuse_thread, NULL);
}
