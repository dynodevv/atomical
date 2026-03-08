/*
 * Atomical OS - RamFS (Memory-based Filesystem)
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/mm.h>

/* RamFS inode private data */
typedef struct ramfs_inode {
    void   *data;       /* File contents (for regular files) */
    size_t  capacity;   /* Allocated size */
} ramfs_inode_t;

/* Forward declarations */
static struct dentry *ramfs_lookup(struct inode *dir, const char *name);
static int ramfs_create(struct inode *dir, const char *name, mode_t mode);
static int ramfs_mkdir(struct inode *dir, const char *name, mode_t mode);
static ssize_t ramfs_read(struct file *file, void *buf, size_t count, off_t *offset);
static ssize_t ramfs_write(struct file *file, const void *buf, size_t count, off_t *offset);

static ino_t next_ino = 2;

static const inode_operations_t ramfs_dir_i_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir  = ramfs_mkdir,
};

static const file_operations_t ramfs_file_f_ops = {
    .read  = ramfs_read,
    .write = ramfs_write,
};

static struct dentry *ramfs_lookup(struct inode *dir, const char *name)
{
    (void)dir;
    /* Search through the directory's children dentries */
    /* The parent dentry would need to be found first; simplified for scaffolding */
    (void)name;
    return NULL;
}

static struct inode *ramfs_alloc_inode(uint32_t type, mode_t mode)
{
    struct inode *inode = kzalloc(sizeof(struct inode));
    if (!inode) return NULL;

    ramfs_inode_t *ri = kzalloc(sizeof(ramfs_inode_t));
    if (!ri) {
        kfree(inode);
        return NULL;
    }

    inode->ino          = next_ino++;
    inode->type         = type;
    inode->mode         = mode;
    inode->nlink        = 1;
    inode->private_data = ri;

    if (type == VFS_DIRECTORY) {
        inode->i_ops = &ramfs_dir_i_ops;
    } else {
        inode->f_ops = &ramfs_file_f_ops;
    }

    return inode;
}

static int ramfs_create(struct inode *dir, const char *name, mode_t mode)
{
    (void)dir;
    (void)name;
    (void)mode;
    /* TODO: Create a new file inode and add a dentry */
    return -1;
}

static int ramfs_mkdir(struct inode *dir, const char *name, mode_t mode)
{
    (void)dir;
    (void)name;
    (void)mode;
    /* TODO: Create a new directory inode and add a dentry */
    return -1;
}

static ssize_t ramfs_read(struct file *file, void *buf, size_t count, off_t *offset)
{
    struct inode *inode = file->inode;
    ramfs_inode_t *ri = (ramfs_inode_t *)inode->private_data;

    if (!ri || !ri->data)
        return 0;

    if ((uint64_t)*offset >= inode->size)
        return 0;

    size_t available = (size_t)(inode->size - *offset);
    if (count > available)
        count = available;

    memcpy(buf, (uint8_t *)ri->data + *offset, count);
    *offset += (off_t)count;
    return (ssize_t)count;
}

static ssize_t ramfs_write(struct file *file, const void *buf, size_t count, off_t *offset)
{
    struct inode *inode = file->inode;
    ramfs_inode_t *ri = (ramfs_inode_t *)inode->private_data;

    if (!ri)
        return -1;

    size_t needed = (size_t)*offset + count;
    if (needed > ri->capacity) {
        /* Grow the buffer */
        size_t new_cap = ALIGN_UP(needed, PAGE_SIZE);
        void *new_data = kmalloc(new_cap);
        if (!new_data)
            return -1;

        if (ri->data) {
            memcpy(new_data, ri->data, ri->capacity);
            kfree(ri->data);
        }
        ri->data = new_data;
        ri->capacity = new_cap;
    }

    memcpy((uint8_t *)ri->data + *offset, buf, count);
    *offset += (off_t)count;

    if ((uint64_t)*offset > inode->size)
        inode->size = (uint64_t)*offset;

    return (ssize_t)count;
}

/* Mount function */
static struct superblock *ramfs_mount(const char *source, const void *data)
{
    (void)source;
    (void)data;

    struct superblock *sb = kzalloc(sizeof(struct superblock));
    if (!sb) return NULL;

    sb->block_size = PAGE_SIZE;
    strcpy(sb->fs_type, "ramfs");

    /* Create root directory for this mount */
    struct inode *root_inode = ramfs_alloc_inode(VFS_DIRECTORY, 0755);
    if (!root_inode) {
        kfree(sb);
        return NULL;
    }
    root_inode->sb = sb;

    struct dentry *root_dentry = kzalloc(sizeof(struct dentry));
    if (!root_dentry) {
        kfree(root_inode->private_data);
        kfree(root_inode);
        kfree(sb);
        return NULL;
    }

    strcpy(root_dentry->name, "/");
    root_dentry->inode  = root_inode;
    root_dentry->parent = root_dentry;
    root_dentry->sb     = sb;

    sb->root = root_dentry;
    return sb;
}

/* RamFS filesystem type */
static filesystem_type_t ramfs_type = {
    .name    = "ramfs",
    .mount   = ramfs_mount,
    .unmount = NULL,
};

/* Register ramfs with VFS */
void ramfs_init(void)
{
    vfs_register_fs(&ramfs_type);
    kprintf("[ramfs] RamFS registered\n");
}
