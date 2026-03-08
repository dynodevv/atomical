/*
 * Atomical OS - VFS (Virtual File System) Header
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/types.h>

/* File types */
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEV     0x03
#define VFS_BLOCKDEV    0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06
#define VFS_MOUNTPOINT  0x08

/* Open flags */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_CREAT         0x0040
#define O_TRUNC         0x0200
#define O_APPEND        0x0400

/* Seek whence */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* Maximum name length */
#define VFS_NAME_MAX    255
#define VFS_PATH_MAX    4096

/* Forward declarations */
struct inode;
struct file;
struct dentry;
struct superblock;

/* File operations */
typedef struct file_operations {
    ssize_t (*read)(struct file *file, void *buf, size_t count, off_t *offset);
    ssize_t (*write)(struct file *file, const void *buf, size_t count, off_t *offset);
    int     (*open)(struct inode *inode, struct file *file);
    int     (*close)(struct file *file);
    off_t   (*seek)(struct file *file, off_t offset, int whence);
    int     (*ioctl)(struct file *file, uint32_t cmd, void *arg);
} file_operations_t;

/* Inode operations */
typedef struct inode_operations {
    struct dentry *(*lookup)(struct inode *dir, const char *name);
    int            (*create)(struct inode *dir, const char *name, mode_t mode);
    int            (*mkdir)(struct inode *dir, const char *name, mode_t mode);
    int            (*rmdir)(struct inode *dir, const char *name);
    int            (*unlink)(struct inode *dir, const char *name);
    int            (*rename)(struct inode *old_dir, const char *old_name,
                             struct inode *new_dir, const char *new_name);
} inode_operations_t;

/* Superblock operations */
typedef struct superblock_operations {
    struct inode *(*alloc_inode)(struct superblock *sb);
    void          (*free_inode)(struct inode *inode);
    int           (*write_super)(struct superblock *sb);
    int           (*sync_fs)(struct superblock *sb);
} superblock_operations_t;

/* Inode: represents a file/directory in the filesystem */
struct inode {
    ino_t                        ino;
    mode_t                       mode;
    uint32_t                     type;
    uid_t                        uid;
    gid_t                        gid;
    uint64_t                     size;
    time_t                       atime;
    time_t                       mtime;
    time_t                       ctime;
    nlink_t                      nlink;
    blkcnt_t                     blocks;
    dev_t                        dev;
    struct superblock           *sb;
    const file_operations_t     *f_ops;
    const inode_operations_t    *i_ops;
    void                        *private_data;
};

/* Directory entry: maps names to inodes */
struct dentry {
    char             name[VFS_NAME_MAX + 1];
    struct inode    *inode;
    struct dentry   *parent;
    struct dentry   *children;
    struct dentry   *next;          /* sibling list */
    struct superblock *sb;
};

/* Open file descriptor */
struct file {
    struct dentry              *dentry;
    struct inode               *inode;
    const file_operations_t    *f_ops;
    off_t                       offset;
    uint32_t                    flags;
    uint32_t                    refcount;
};

/* Superblock: represents a mounted filesystem */
struct superblock {
    dev_t                            dev;
    uint64_t                         block_size;
    uint64_t                         total_blocks;
    uint64_t                         free_blocks;
    uint64_t                         total_inodes;
    uint64_t                         free_inodes;
    struct dentry                   *root;
    const superblock_operations_t   *s_ops;
    void                            *private_data;
    char                             fs_type[32];
};

/* Filesystem type registration */
typedef struct filesystem_type {
    const char *name;
    struct superblock *(*mount)(const char *source, const void *data);
    void (*unmount)(struct superblock *sb);
    struct filesystem_type *next;
} filesystem_type_t;

/* VFS core API */
void vfs_init(void);
int  vfs_register_fs(filesystem_type_t *fs);
int  vfs_mount(const char *source, const char *target, const char *fstype, const void *data);
int  vfs_unmount(const char *target);

/* File operations */
struct file *vfs_open(const char *path, uint32_t flags, mode_t mode);
int          vfs_close(struct file *file);
ssize_t      vfs_read(struct file *file, void *buf, size_t count);
ssize_t      vfs_write(struct file *file, const void *buf, size_t count);
off_t        vfs_seek(struct file *file, off_t offset, int whence);

/* Directory operations */
int          vfs_mkdir(const char *path, mode_t mode);
int          vfs_rmdir(const char *path);
int          vfs_unlink(const char *path);

/* Path resolution */
struct dentry *vfs_lookup(const char *path);

#endif /* _KERNEL_VFS_H */
