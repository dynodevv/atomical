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

    /* Initialize RamFS and mount it at root */
    ramfs_init();

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

    strncpy(mount_table[mount_count].path, target ? target : "", VFS_PATH_MAX - 1);
    mount_table[mount_count].path[VFS_PATH_MAX - 1] = '\0';
    mount_table[mount_count].sb = sb;
    mount_count++;

    kprintf("[vfs]  Mounted %s on %s (type: %s)\n", source ? source : "none", target, fstype);
    return 0;
}

/*
 * Find the best-matching mount point for a path and return the
 * remaining path suffix after the mount point.
 */
static struct superblock *find_mount(const char *path, const char **remaining)
{
    int best = -1;
    size_t best_len = 0;

    for (int i = 0; i < mount_count; i++) {
        size_t mlen = strlen(mount_table[i].path);
        if (strncmp(path, mount_table[i].path, mlen) == 0) {
            /* Ensure exact prefix: path continues with '/' or ends */
            if (mlen > best_len &&
                (path[mlen] == '/' || path[mlen] == '\0' || mlen == 1)) {
                best = i;
                best_len = mlen;
            }
        }
    }

    if (best >= 0) {
        const char *rest = path + best_len;
        /* Skip leading slash in remainder */
        while (*rest == '/')
            rest++;
        *remaining = rest;
        return mount_table[best].sb;
    }
    return NULL;
}

/* Path resolution: walk from root following path components */
struct dentry *vfs_lookup(const char *path)
{
    if (!path || path[0] != '/')
        return NULL;

    /* Check if this path falls within a mount point */
    const char *remaining = NULL;
    struct superblock *sb = find_mount(path, &remaining);

    struct dentry *current;
    const char *walk;

    if (sb && sb->root) {
        current = sb->root;
        walk = remaining;
    } else {
        current = vfs_root;
        walk = path + 1;  /* skip leading '/' */
    }

    while (*walk) {
        /* Skip multiple slashes */
        while (*walk == '/')
            walk++;

        if (*walk == '\0')
            break;

        /* Extract next path component */
        char component[VFS_NAME_MAX + 1];
        int i = 0;
        while (*walk && *walk != '/' && i < VFS_NAME_MAX) {
            component[i++] = *walk++;
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

/*
 * Split a path into parent directory path and the final name component.
 * Returns 0 on success.
 */
static int vfs_split_path(const char *path, char *parent_path, size_t parent_size,
                           char *name, size_t name_size)
{
    if (!path || path[0] != '/')
        return -1;

    size_t len = strlen(path);

    /* Find the last '/' */
    int last_slash = -1;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/')
            last_slash = (int)i;
    }

    if (last_slash < 0)
        return -1;

    /* Extract name (after last slash) */
    const char *name_start = path + last_slash + 1;
    if (*name_start == '\0')
        return -1;  /* Path ends with '/' */

    strncpy(name, name_start, name_size - 1);
    name[name_size - 1] = '\0';

    /* Extract parent path */
    if (last_slash == 0) {
        /* Parent is root */
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        size_t plen = (size_t)last_slash;
        if (plen >= parent_size) plen = parent_size - 1;
        memcpy(parent_path, path, plen);
        parent_path[plen] = '\0';
    }

    return 0;
}

struct file *vfs_open(const char *path, uint32_t flags, mode_t mode)
{
    struct dentry *dentry = vfs_lookup(path);

    if (!dentry && (flags & O_CREAT)) {
        /* Try to create the file */
        char parent_path[VFS_PATH_MAX];
        char name[VFS_NAME_MAX + 1];
        if (vfs_split_path(path, parent_path, sizeof(parent_path),
                           name, sizeof(name)) != 0)
            return NULL;

        struct dentry *parent = vfs_lookup(parent_path);
        if (!parent || !parent->inode || !parent->inode->i_ops ||
            !parent->inode->i_ops->create)
            return NULL;

        if (parent->inode->i_ops->create(parent->inode, name, mode) != 0)
            return NULL;

        dentry = vfs_lookup(path);
        if (!dentry)
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
    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX + 1];

    if (vfs_split_path(path, parent_path, sizeof(parent_path),
                       name, sizeof(name)) != 0)
        return -1;

    struct dentry *parent = vfs_lookup(parent_path);
    if (!parent || !parent->inode || !parent->inode->i_ops ||
        !parent->inode->i_ops->mkdir)
        return -1;

    return parent->inode->i_ops->mkdir(parent->inode, name, mode);
}

int vfs_rmdir(const char *path)
{
    (void)path;
    /* TODO: Implement */
    return -1;
}

int vfs_unlink(const char *path)
{
    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX + 1];

    if (vfs_split_path(path, parent_path, sizeof(parent_path),
                       name, sizeof(name)) != 0)
        return -1;

    struct dentry *parent = vfs_lookup(parent_path);
    if (!parent || !parent->inode || !parent->inode->i_ops ||
        !parent->inode->i_ops->unlink)
        return -1;

    return parent->inode->i_ops->unlink(parent->inode, name);
}

int vfs_unmount(const char *target)
{
    (void)target;
    /* TODO: Implement */
    return -1;
}

struct dentry *vfs_get_children(const char *path)
{
    struct dentry *dentry = vfs_lookup(path);
    if (!dentry)
        return NULL;
    return dentry->children;
}
