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
static int ramfs_unlink(struct inode *dir, const char *name);
static ssize_t ramfs_read(struct file *file, void *buf, size_t count, off_t *offset);
static ssize_t ramfs_write(struct file *file, const void *buf, size_t count, off_t *offset);

static ino_t next_ino = 2;

static const inode_operations_t ramfs_dir_i_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir  = ramfs_mkdir,
    .unlink = ramfs_unlink,
};

static const file_operations_t ramfs_file_f_ops = {
    .read  = ramfs_read,
    .write = ramfs_write,
};

/*
 * Find the parent dentry that owns a given inode.
 * We walk from a known dentry (typically the mount root) to find which
 * dentry holds this inode.  For ramfs, directory inodes store a back-pointer
 * to their dentry in private_data->data when the dentry is a directory.
 */
static struct dentry *ramfs_find_dentry_for_inode(struct inode *dir);

static struct dentry *ramfs_lookup(struct inode *dir, const char *name)
{
    /* Find the dentry that corresponds to this directory inode */
    struct dentry *parent = ramfs_find_dentry_for_inode(dir);
    if (!parent)
        return NULL;

    /* Walk the children linked list */
    struct dentry *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0)
            return child;
        child = child->next;
    }
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

/*
 * Add a child dentry to a parent directory.
 */
static struct dentry *ramfs_add_child(struct dentry *parent, const char *name,
                                       struct inode *inode)
{
    struct dentry *child = kzalloc(sizeof(struct dentry));
    if (!child) return NULL;

    strncpy(child->name, name, VFS_NAME_MAX);
    child->name[VFS_NAME_MAX] = '\0';
    child->inode  = inode;
    child->parent = parent;
    child->sb     = parent->sb;

    /* Prepend to parent's children list */
    child->next     = parent->children;
    parent->children = child;

    /* For directory inodes, store the dentry pointer so lookup can find it */
    if (inode->type == VFS_DIRECTORY) {
        ramfs_inode_t *ri = (ramfs_inode_t *)inode->private_data;
        ri->data = child;  /* back-pointer: inode → dentry */
    }

    return child;
}

static struct dentry *ramfs_find_dentry_for_inode(struct inode *dir)
{
    if (!dir || dir->type != VFS_DIRECTORY)
        return NULL;
    ramfs_inode_t *ri = (ramfs_inode_t *)dir->private_data;
    if (!ri)
        return NULL;
    return (struct dentry *)ri->data;
}

static int ramfs_create(struct inode *dir, const char *name, mode_t mode)
{
    struct dentry *parent = ramfs_find_dentry_for_inode(dir);
    if (!parent) return -1;

    /* Check for duplicates */
    struct dentry *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0)
            return -1;  /* Already exists */
        child = child->next;
    }

    struct inode *new_inode = ramfs_alloc_inode(VFS_FILE, mode);
    if (!new_inode) return -1;

    new_inode->sb = dir->sb;

    if (!ramfs_add_child(parent, name, new_inode)) {
        kfree(new_inode->private_data);
        kfree(new_inode);
        return -1;
    }

    return 0;
}

static int ramfs_mkdir(struct inode *dir, const char *name, mode_t mode)
{
    struct dentry *parent = ramfs_find_dentry_for_inode(dir);
    if (!parent) return -1;

    /* Check for duplicates */
    struct dentry *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0)
            return -1;  /* Already exists */
        child = child->next;
    }

    struct inode *new_inode = ramfs_alloc_inode(VFS_DIRECTORY, mode);
    if (!new_inode) return -1;

    new_inode->sb = dir->sb;

    if (!ramfs_add_child(parent, name, new_inode)) {
        kfree(new_inode->private_data);
        kfree(new_inode);
        return -1;
    }

    return 0;
}

static int ramfs_unlink(struct inode *dir, const char *name)
{
    struct dentry *parent = ramfs_find_dentry_for_inode(dir);
    if (!parent) return -1;

    struct dentry *prev = NULL;
    struct dentry *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            /* Remove from sibling list */
            if (prev)
                prev->next = child->next;
            else
                parent->children = child->next;

            /* Free inode data */
            if (child->inode) {
                ramfs_inode_t *ri = (ramfs_inode_t *)child->inode->private_data;
                if (ri) {
                    if (child->inode->type == VFS_FILE && ri->data)
                        kfree(ri->data);
                    kfree(ri);
                }
                kfree(child->inode);
            }
            kfree(child);
            return 0;
        }
        prev = child;
        child = child->next;
    }
    return -1;  /* Not found */
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

        /* Zero the entire new buffer to prevent stale data exposure */
        memset(new_data, 0, new_cap);

        if (ri->data) {
            memcpy(new_data, ri->data, inode->size < ri->capacity ? inode->size : ri->capacity);
            kfree(ri->data);
        }
        ri->data = new_data;
        ri->capacity = new_cap;
    }

    /* Zero-fill gap between old EOF and write start */
    if ((uint64_t)*offset > inode->size) {
        memset((uint8_t *)ri->data + inode->size, 0, (size_t)(*offset - inode->size));
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

    /* Set up back-pointer so ramfs_lookup can find the dentry from the inode */
    ramfs_inode_t *ri = (ramfs_inode_t *)root_inode->private_data;
    ri->data = root_dentry;

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
