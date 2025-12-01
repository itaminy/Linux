
// vfs.c
#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include "vfs.h"

static struct fuse_operations users_ops;

static void *fuse_thread(void *arg) {
    (void)arg;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char mountpoint[PATH_MAX];
    snprintf(mountpoint, sizeof(mountpoint), "%s/users", home);

    mkdir(mountpoint, 0755);  // создаём точку монтирования

    char *argv[] = { "kubsh-fuse", NULL };
    struct fuse_args args = FUSE_ARGS_INIT(1, argv);

    struct fuse *fuse = fuse_new(&args, &users_ops, sizeof(users_ops), NULL);
    if (!fuse) { fprintf(stderr, "fuse_new failed\n"); return NULL; }

    if (fuse_mount(fuse, mountpoint) != 0) { fprintf(stderr, "fuse_mount failed\n"); fuse_destroy(fuse); return NULL; }

    fuse_loop(fuse);

    fuse_unmount(fuse);
    fuse_destroy(fuse);
    return NULL;
}



// Проверяем, что пользователь может логиниться по /etc/shells
static int can_login(struct passwd *pwd) {
    if (!pwd) return 0;
    FILE *f = fopen("/etc/shells", "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, pwd->pw_shell) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static int users_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(*stbuf));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char user[128], file[32];
    if (sscanf(path, "/%127[^/]/%31s", user, file) == 2) {
        struct passwd *pwd = getpwnam(user);
        if (!pwd || !can_login(pwd)) return -ENOENT;
        stbuf->st_mode = S_IFREG | 0644;
        if (strcmp(file, "id") == 0) stbuf->st_size = snprintf(NULL, 0, "%d", pwd->pw_uid);
        else if (strcmp(file, "home") == 0) stbuf->st_size = strlen(pwd->pw_dir);
        else stbuf->st_size = strlen(pwd->pw_shell);
        return 0;
    }

    if (sscanf(path, "/%127s", user) == 1) {
        struct passwd *pwd = getpwnam(user);
        if (!pwd || !can_login(pwd)) return -ENOENT;
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

static int users_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    if (strcmp(path, "/") == 0) {
        struct passwd *pwd;
        setpwent();
        while ((pwd = getpwent()) != NULL) {
            if (can_login(pwd)) filler(buf, pwd->pw_name, NULL, 0, 0);
        }
        endpwent();
    } else {
        filler(buf, "id", NULL, 0, 0);
        filler(buf, "home", NULL, 0, 0);
        filler(buf, "shell", NULL, 0, 0);
    }
    return 0;
}

static int users_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    char user[128], file[32];
    if (sscanf(path, "/%127[^/]/%31s", user, file) != 2) return -ENOENT;
    struct passwd *pwd = getpwnam(user);
    if (!pwd || !can_login(pwd)) return -ENOENT;
    char content[512];
    content[0] = '\0';
    if (strcmp(file, "id") == 0) snprintf(content, sizeof(content), "%d\n", pwd->pw_uid);
    else if (strcmp(file, "home") == 0) snprintf(content, sizeof(content), "%s\n", pwd->pw_dir);
    else snprintf(content, sizeof(content), "%s\n", pwd->pw_shell);

    size_t len = strlen(content);
    if ((size_t)offset >= len) return 0;
    if (offset + (off_t)size > (off_t)len) size = len - offset;
    memcpy(buf, content + offset, size);
    return (int)size;
}

// Вспомогательная функция для выполнения команды через fork + exec
static int run_cmd(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, argv);
        perror("execvp failed");
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    } else {
        perror("fork failed");
        return -1;
    }
}

int users_mkdir(const char *path, mode_t mode) {
    (void)mode;
    char username[256];

    // Извлекаем имя пользователя из пути
    if (sscanf(path, "/%255[^/]", username) == 1) {
        struct passwd *pwd = getpwnam(username);
        if (pwd != NULL) {
            return -EEXIST;  // Пользователь уже существует
        }

        // Команда adduser
        char *const argv[] = {
            "adduser",
            "--disabled-password",
            "--gecos",
            "",
            username,
            NULL
        };

        if (run_cmd("adduser", argv) != 0) return -EIO;
    }
    return 0;
}

int users_rmdir(const char *path) {
    char username[256];

    if (sscanf(path, "/%255[^/]", username) == 1) {
        // Проверяем, есть ли вложенные файлы в пути
        if (strchr(path + 1, '/') == NULL) {
            struct passwd *pwd = getpwnam(username);
            if (pwd != NULL) {
                // Команда userdel
                char *const argv[] = {
                    "userdel",
                    "--remove",
                    username,
                    NULL
                };
                if (run_cmd("userdel", argv) != 0) return -EIO;
                return 0;
            }
            return -ENOENT;  // Пользователя нет
        }
        return -EPERM;  // Запрещено удалять вложенные файлы
    }
    return -EPERM;
}


void fuse_start(void) {
    users_ops.getattr = users_getattr;
    users_ops.readdir = users_readdir;
    users_ops.read = users_read;
    users_ops.mkdir = users_mkdir;
    users_ops.rmdir = users_rmdir;

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char mountpoint[PATH_MAX];
    snprintf(mountpoint, sizeof(mountpoint), "%s/users", home);

    // mkdir если не существует
    mkdir(mountpoint, 0755);

    pthread_t th;
    if (pthread_create(&th, NULL, fuse_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create FUSE thread\n");
        return;
    }
    pthread_detach(th);
}


void print_disk_info(const char *device) {
    if (!device) return;
    pid_t pid = fork();
    if (pid == 0) {
        // use lsblk to display partition info (portable)
        execlp("lsblk", "lsblk", "-o", "NAME,SIZE,TYPE,MOUNTPOINT", device, (char*)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}

