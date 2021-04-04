//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <fcntl.h>

#include "types.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "string.h"
#include "console.h"
#include "log.h"
#include "fs.h"
#include "file.h"

extern int execve(const char *, char *const, char *const);

struct iovec {
    void *iov_base;             /* Starting address. */
    size_t iov_len;             /* Number of bytes to transfer. */
};

/*
 * Fetch the nth word-sized system call argument as a file descriptor
 * and return both the descriptor and the corresponding struct file.
 */
static int
argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0)
        return -1;
    if (fd < 0 || fd >= NOFILE || (f = thisproc()->ofile[fd]) == 0)
        return -1;
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
static int
fdalloc(struct file *f)
{
    struct proc *curproc = thisproc();

    for (int fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd] == 0) {
            curproc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int
sys_dup()
{
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0)
        return -1;
    if ((fd = fdalloc(f)) < 0)
        return -1;
    trace("fd %d", fd);
    filedup(f);
    return fd;
}

ssize_t
sys_read()
{
    struct file *f;
    ssize_t n;
    char *p;

    if (argfd(0, 0, &f) < 0 || argu64(2, &n) < 0 || argptr(1, &p, n) < 0)
        return -1;
    return fileread(f, p, n);
}

ssize_t
sys_write()
{
    struct file *f;
    ssize_t n;
    char *p;

    if (argfd(0, 0, &f) < 0 || argu64(2, &n) < 0 || argptr(1, &p, n) < 0)
        return -1;
    return filewrite(f, p, n);
}


ssize_t
sys_writev()
{
    struct file *f;
    int fd, iovcnt;
    struct iovec *iov, *p;
    if (argfd(0, &fd, &f) < 0 ||
        argint(2, &iovcnt) < 0 ||
        argptr(1, (char **)&iov, iovcnt * sizeof(struct iovec)) < 0) {
        return -1;
    }
    trace("fd %d, iovcnt: %d", fd, iovcnt);

    size_t tot = 0;
    for (p = iov; p < iov + iovcnt; p++) {
        if (!in_user(p->iov_base, p->iov_len))
            return -1;
        tot += filewrite(f, p->iov_base, p->iov_len);
    }
    return tot;
}

int
sys_close()
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -1;
    thisproc()->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

int
sys_fstat()
{
    int fd;
    struct file *f;
    struct stat *st;

    if (argfd(0, &fd, &f) < 0 || argptr(1, (void *)&st, sizeof(*st)) < 0)
        return -1;
    trace("fd %d", fd);
    return filestat(f, st);
}

int
sys_fstatat()
{
    int dirfd, flags;
    char *path;
    struct stat *st;

    if (argint(0, &dirfd) < 0 ||
        argstr(1, &path) < 0 ||
        argptr(2, (void *)&st, sizeof(*st)) < 0 || argint(3, &flags) < 0)
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    if (flags != 0) {
        warn("flags unimplemented");
        return -1;
    }

    struct inode *ip;
    begin_op();
    if ((ip = namei(path)) == 0) {
        end_op();
        return -1;
    }
    ilock(ip);
    stati(ip, st);
    iunlockput(ip);
    end_op();

    return 0;
}

/* Create the path new as a link to the same inode as old. */
int
sys_link()
{
    char name[DIRSIZ], *new, *old;
    struct inode *dp, *ip;

    if (argstr(0, &old) < 0 || argstr(1, &new) < 0)
        return -1;

    begin_op();
    if ((ip = namei(old)) == 0) {
        end_op();
        return -1;
    }

    ilock(ip);
    if (ip->type == T_DIR) {
        iunlockput(ip);
        end_op();
        return -1;
    }

    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    if ((dp = nameiparent(new, name)) == 0)
        goto bad;
    ilock(dp);
    if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
        iunlockput(dp);
        goto bad;
    }
    iunlockput(dp);
    iput(ip);

    end_op();

    return 0;

  bad:
    ilock(ip);
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_op();
    return -1;
}

/* Is the directory dp empty except for "." and ".." ? */
static int
isdirempty(struct inode *dp)
{
    struct dirent de;

    for (ssize_t off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
        if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: readi");
        if (de.inum != 0)
            return 0;
    }
    return 1;
}

int
sys_unlink()
{
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], *path;
    ssize_t off;

    if (argstr(0, &path) < 0)
        return -1;

    begin_op();
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -1;
    }

    ilock(dp);

    /* Cannot unlink "." or "..". */
    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
        goto bad;

    if ((ip = dirlookup(dp, name, &off)) == 0)
        goto bad;
    ilock(ip);

    if (ip->nlink < 1)
        panic("unlink: nlink < 1");
    if (ip->type == T_DIR && !isdirempty(ip)) {
        iunlockput(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: writei");
    if (ip->type == T_DIR) {
        dp->nlink--;
        iupdate(dp);
    }
    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);

    end_op();

    return 0;

  bad:
    iunlockput(dp);
    end_op();
    return -1;
}

static struct inode *
create(char *path, short type, short major, short minor)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name)) == 0)
        return 0;
    ilock(dp);

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && ip->type == T_FILE)
            return ip;
        iunlockput(ip);
        return 0;
    }

    if ((ip = ialloc(dp->dev, type)) == 0)
        panic("create: ialloc");

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    iupdate(ip);

    if (type == T_DIR) {        // Create . and .. entries.
        dp->nlink++;            // for ".."
        iupdate(dp);
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (dirlink(ip, ".", ip->inum) < 0
            || dirlink(ip, "..", dp->inum) < 0)
            panic("create dots");
    }

    if (dirlink(dp, name, ip->inum) < 0)
        panic("create: dirlink");

    iunlockput(dp);

    return ip;
}

int
sys_openat()
{
    char *path;
    int dirfd, fd, omode;
    struct file *f;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, &omode) < 0)
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    if ((omode & O_LARGEFILE) == 0) {
        warn("expect O_LARGEFILE in open flags");
        return -1;
    }
    trace("dirfd %d, path '%s', flag 0x%x", dirfd, path, omode);

    begin_op();
    if (omode & O_CREAT) {
        // FIXME: acl mode are ignored.
        ip = create(path, T_FILE, 0, 0);
        if (ip == 0) {
            end_op();
            return -1;
        }
    } else {
        if ((ip = namei(path)) == 0) {
            end_op();
            return -1;
        }
        ilock(ip);
        if (ip->type == T_DIR && omode != (O_RDONLY | O_LARGEFILE)) {
            iunlockput(ip);
            end_op();
            return -1;
        }
    }

    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        iunlockput(ip);
        end_op();
        return -1;
    }
    iunlock(ip);
    end_op();

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    return fd;
}

int
sys_mkdirat()
{
    int dirfd, mode;
    char *path;
    struct inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, &mode) < 0)
        return -1;
    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    if (mode != 0) {
        warn("mode unimplemented");
        return -1;
    }
    trace("path '%s', mode 0x%x", path, mode);

    begin_op();
    if ((ip = create(path, T_DIR, 0, 0)) == 0) {
        end_op();
        return -1;
    }
    iunlockput(ip);
    end_op();
    return 0;
}

int
sys_mknodat()
{
    struct inode *ip;
    char *path;
    int dirfd, major, minor;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0
        || argint(2, &major) < 0 || argint(3, &minor))
        return -1;

    if (dirfd != AT_FDCWD) {
        warn("dirfd unimplemented");
        return -1;
    }
    trace("path '%s', major:minor %d:%d", path, major, minor);

    begin_op();
    if ((ip = create(path, T_DEV, major, minor)) == 0) {
        end_op();
        return -1;
    }
    iunlockput(ip);
    end_op();
    return 0;
}

int
sys_chdir()
{
    char *path;
    struct inode *ip;
    struct proc *curproc = thisproc();

    begin_op();
    if (argstr(0, &path) < 0 || (ip = namei(path)) == 0) {
        end_op();
        return -1;
    }
    ilock(ip);
    if (ip->type != T_DIR) {
        iunlockput(ip);
        end_op();
        return -1;
    }
    iunlock(ip);
    iput(curproc->cwd);
    end_op();
    curproc->cwd = ip;
    return 0;
}

int
sys_execve()
{
    char *p;
    void *argv, *envp;
    if (argstr(0, &p) < 0 || argu64(1, (uint64_t *) & argv) < 0
        || argu64(2, (uint64_t *) & envp) < 0)
        return -1;
    return execve(p, argv, envp);
}

int
sys_pipe2()
{
    int *fd, flag;
    struct file *rf, *wf;
    int fd0, fd1;

    if (argint(1, &flag) < 0
        || argptr(0, (void *)&fd, 2 * sizeof(fd[0])) < 0)
        return -1;
    trace("flag 0x%x", flag);
    if (flag) {
        warn("pipe with flag unimplemented");
        return -1;
    }
    if (pipealloc(&rf, &wf) < 0)
        return -1;
    fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
        if (fd0 >= 0)
            thisproc()->ofile[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    fd[0] = fd0;
    fd[1] = fd1;
    return 0;
}
