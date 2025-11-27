#ifndef VFS_MANAGER_H
#define VFS_MANAGER_H

#include <fuse3/fuse.h>

extern struct fuse_operations users_operations;

void fuse_start();

#endif
