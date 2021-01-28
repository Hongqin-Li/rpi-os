#ifndef INC_FILE_H
#define INC_FILE_H

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
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short nlink; // Number of links to file
  uint size;   // Size of file in bytes
};

void            readsb(int dev, struct superblock *sb);
int             dirlink(struct inode *, char *, uint);
struct inode *  dirlookup(struct inode *, char *, uint *);
struct inode *  ialloc(uint, short);
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
int             readi(struct inode *, char *, uint, uint);
void            stati(struct inode *, struct stat *);
int             writei(struct inode *, char *, uint, uint);

#endif
