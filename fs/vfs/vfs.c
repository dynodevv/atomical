/*
 * Atomical OS - Virtual File System Core
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/mm.h>

/* Root of the VFS */
static struct dentry *vfs_root = NULL;

/* Registered filesystem types */
static filesystem_type_t *fs_types = NULL;

/* Mount points */
#define MAX_MOUNTS 32
static struct {
    char path[VFS_PATH_MAX];
    struct superblock *sb;
} mount_table[MAX_MOUNTS];
static int mount_count = 0;

void vfs_init(void)
{
    /* Create the root dentry */
    vfs_root = kzalloc(sizeof(struct dentry));
    if (!vfs_root) {
        panic("Failed to allocate VFS root dentry");
    }

    strcpy(vfs_root->name, "/");
    vfs_root->parent = vfs_root;

    /* Create root inode */
    struct inode *root_inode = kzalloc(sizeof(struct inode));
    if (!root_inode) {
        panic("Failed to allocate VFS root inode");
    }

    root_inode->ino  = 1;
    root_inode->type = VFS_DIRECTORY;
    root_inode->mode = 0755;

    vfs_root->inode = root_inode;

    kprintf("[vfs]  Root filesystem initialized\n");
}

int vfs_register_fs(filesystem_type_t *fs)
{
    if (!fs || !fs->name)
        return -1;

    fs->next = fs_types;
    fs_types = fs;

    kprintf("[vfs]  Registered filesystem: %s\n", fs->name);
    return 0;
}

int vfs_mount(const char *source, const char *target, const char *fstype, const void *data)
{
    if (mount_count >= MAX_MOUNTS)
        return -1;

    /* Find the filesystem type */
    filesystem_type_t *fs = fs_types;
    while (fs) {
        if (strcmp(fs->name, fstype) == 0)
            break;
        fs = fs->next;
    }

    if (!fs) {
        kprintf("[vfs]  Unknown filesystem type: %s\n", fstype);
        return -1;
    }

    /* Mount the filesystem */
    struct superblock *sb = fs->mount(source, data);
    if (!sb)
        return -1;

    strncpy(mount_table[mount_count].path, target, VFS_PATH_MAX - 1);
    mount_table[mount_count].sb = sb;
    mount_count++;

    kprintf("[vfs]  Mounted %s on %s (type: %s)\n", source ? source : "none", target, fstype);
    return 0;
}

/* Path resolution: walk from root following path components */
struct dentry *vfs_lookup(const char *path)
{
    if (!path || path[0] != '/')
        return NULL;

    struct dentry *current = vfs_root;

    /* Skip leading slash */
    path++;

    while (*path) {
        /* Skip multiple slashes */
        while (*path == '/')
            path++;

        if (*path == '\0')
            break;

        /* Extract next path component */
        char component[VFS_NAME_MAX + 1];
        int i = 0;
        while (*path && *path != '/' && i < VFS_NAME_MAX) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        /* Look up component in current directory */
        if (!current->inode || !current->inode->i_ops || !current->inode->i_ops->lookup) {
            return NULL;
        }

        struct dentry *child = current->inode->i_ops->lookup(current->inode, component);
        if (!child)
            return NULL;

        current = child;
    }

    return current;
}

struct file *vfs_open(const char *path, uint32_t flags, mode_t mode)
{
    struct dentry *dentry = vfs_lookup(path);

    if (!dentry && (flags & O_CREAT)) {
        /* TODO: Create the file using mode */
        (void)mode;
        return NULL;
    }

    if (!dentry)
        return NULL;

    struct file *file = kzalloc(sizeof(struct file));
    if (!file)
        return NULL;

    file->dentry   = dentry;
    file->inode    = dentry->inode;
    file->f_ops    = dentry->inode->f_ops;
    file->flags    = flags;
    file->offset   = 0;
    file->refcount = 1;

    if (file->f_ops && file->f_ops->open) {
        int err = file->f_ops->open(file->inode, file);
        if (err) {
            kfree(file);
            return NULL;
        }
    }

    return file;
}

int vfs_close(struct file *file)
{
    if (!file)
        return -1;

    file->refcount--;
    if (file->refcount > 0)
        return 0;

    if (file->f_ops && file->f_ops->close)
        file->f_ops->close(file);

    kfree(file);
    return 0;
}

ssize_t vfs_read(struct file *file, void *buf, size_t count)
{
    if (!file || !file->f_ops || !file->f_ops->read)
        return -1;
    return file->f_ops->read(file, buf, count, &file->offset);
}

ssize_t vfs_write(struct file *file, const void *buf, size_t count)
{
    if (!file || !file->f_ops || !file->f_ops->write)
        return -1;
    return file->f_ops->write(file, buf, count, &file->offset);
}

off_t vfs_seek(struct file *file, off_t offset, int whence)
{
    if (!file)
        return -1;

    if (file->f_ops && file->f_ops->seek)
        return file->f_ops->seek(file, offset, whence);

    /* Default seek implementation */
    switch (whence) {
    case SEEK_SET:
        file->offset = offset;
        break;
    case SEEK_CUR:
        file->offset += offset;
        break;
    case SEEK_END:
        if (file->inode)
            file->offset = (off_t)file->inode->size + offset;
        break;
    default:
        return -1;
    }

    return file->offset;
}

int vfs_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    /* TODO: Implement */
    return -1;
}

int vfs_rmdir(const char *path)
{
    (void)path;
    /* TODO: Implement */
    return -1;
}

int vfs_unlink(const char *path)
{
    (void)path;
    /* TODO: Implement */
    return -1;
}

int vfs_unmount(const char *target)
{
    (void)target;
    /* TODO: Implement */
    return -1;
}
