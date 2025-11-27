#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdlib>

using namespace std;

// Получение списка пользователей
vector<passwd> get_users() {
    vector<passwd> users;
    setpwent();
    passwd* p;
    while ((p = getpwent())) {
        if (p->pw_uid >= 1000 && strcmp(p->pw_shell, "/usr/sbin/nologin") != 0)
            users.push_back(*p);
    }
    endpwent();
    return users;
}

bool is_user_dir(const string& path, passwd& out) {
    if (path.size() <= 1) return false;
    string username = path.substr(1);

    for (auto& p : get_users()) {
        if (username == p.pw_name) {
            out = p;
            return true;
        }
    }
    return false;
}

static int vfs_getattr(const char* path, struct stat* st, struct fuse_file_info*) {
    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    passwd user;
    if (is_user_dir(path, user)) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    string p(path);
    size_t pos = p.find('/', 1);
    if (pos == string::npos) return -ENOENT;

    string user_dir = p.substr(1, pos - 1);
    string file = p.substr(pos + 1);

    passwd u;
    if (!is_user_dir("/" + user_dir, u)) return -ENOENT;

    if (file == "id" || file == "home" || file == "shell") {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 64;
        return 0;
    }

    return -ENOENT;
}

static int vfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t, struct fuse_file_info*, enum fuse_readdir_flags) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (strcmp(path, "/") == 0) {
        for (auto& u : get_users())
            filler(buf, u.pw_name, NULL, 0);
    } else {
        filler(buf, "id", NULL, 0);
        filler(buf, "home", NULL, 0);
        filler(buf, "shell", NULL, 0);
    }

    return 0;
}

static int vfs_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info*) {
    string p(path);
    size_t pos = p.find('/', 1);
    string user = p.substr(1, pos - 1);
    string file = p.substr(pos + 1);

    passwd u;
    if (!is_user_dir("/" + user, u)) return -ENOENT;

    string content;
    if (file == "id") content = to_string(u.pw_uid);
    else if (file == "home") content = u.pw_dir;
    else if (file == "shell") content = u.pw_shell;
    else return -ENOENT;

    if (offset >= content.size()) return 0;
    if (offset + size > content.size()) size = content.size() - offset;

    memcpy(buf, content.c_str() + offset, size);
    return size;
}

static int vfs_mkdir(const char* path, mode_t) {
    string username = path + 1;
    string cmd = "sudo adduser --disabled-password --gecos \"\" " + username;
    return system(cmd.c_str());
}

static int vfs_rmdir(const char* path) {
    string username = path + 1;
    string cmd = "sudo userdel -r " + username;
    return system(cmd.c_str());
}

static struct fuse_operations ops = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .read = vfs_read,
    .mkdir = vfs_mkdir,
    .rmdir = vfs_rmdir
};

int main(int argc, char* argv[]) {
    return fuse_main(argc, argv, &ops, NULL);
}
