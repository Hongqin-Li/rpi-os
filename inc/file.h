#ifndef INC_FILE_H
#define INC_FILE_H

#include <sys/stat.h>
#include "types.h"
#include "defs.h"
#include "sleeplock.h"
#include "fs.h"

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  size_t off;
};


// in-memory copy of an inode
struct inode {
  size_t dev;           // Device number
  size_t inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  uint16_t type;         // copy of disk inode
  uint16_t major;
  uint16_t minor;
  uint16_t nlink;
  uint32_t size;
  uint32_t addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

void            readsb(int dev, struct superblock *sb);
int             dirlink(struct inode *, char *, uint32_t);
struct inode *  dirlookup(struct inode *, char *, size_t *);
struct inode *  ialloc(uint32_t, short);
struct inode *  idup(struct inode *);
void            iinit(int dev);
void            ilock(struct inode *);
void            iput(struct inode *);
void            iunlock(struct inode *);
void            iunlockput(struct inode *);
void            iupdate(struct inode *);
int             namecmp(const char *, const char *);
struct inode *  namei(char *);
struct inode *  nameiparent(char *, char *);
void            stati(struct inode *, struct stat *);
ssize_t         readi(struct inode *, char *, size_t, size_t);
ssize_t         writei(struct inode *, char *, size_t, size_t);

struct file *   filealloc();
struct file *   filedup(struct file *f);
void            fileclose(struct file *f);
int             filestat(struct file *f, struct stat *st);
ssize_t         fileread(struct file *f, char *addr, ssize_t n);
ssize_t         filewrite(struct file *f, char *addr, ssize_t n);

#endif
