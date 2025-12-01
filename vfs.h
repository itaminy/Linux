// vfs.h
#ifndef VFS_H
#define VFS_H

// Запустить VFS (монтируется в ~/users в отдельном потоке)
void fuse_start(void);

// Вывод информации о разделе (используется командой \l /dev/...)
void print_disk_info(const char *device);

#endif // VFS_H
