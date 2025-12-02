#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

void users_mkdir(const char *vfs_root) {
    FILE *f;
    struct passwd *pw;
    char path_file[4096];

    setpwent();
    while ((pw = getpwent()) != NULL) {
        char user_dir[4096];
        snprintf(user_dir, sizeof(user_dir), "%s/%s", vfs_root, pw->pw_name);
        mkdir(user_dir, 0755);

        snprintf(path_file, sizeof(path_file), "%s/id", user_dir);
        f = fopen(path_file, "w");
        if (f) { fprintf(f, "%d\n", pw->pw_uid); fclose(f); }

        snprintf(path_file, sizeof(path_file), "%s/home", user_dir);
        f = fopen(path_file, "w");
        if (f) { fprintf(f, "%s\n", pw->pw_dir); fclose(f); }

        snprintf(path_file, sizeof(path_file), "%s/shell", user_dir);
        f = fopen(path_file, "w");
        if (f) { fprintf(f, "%s\n", pw->pw_shell); fclose(f); }
    }
    endpwent();
}
