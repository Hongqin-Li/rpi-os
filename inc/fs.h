#ifndef INC_FS_H
#define INC_FS_H

#include <stdint.h>

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint32_t size;         // Size of file system image (blocks)
  uint32_t nblocks;      // Number of data blocks
  uint32_t ninodes;      // Number of inodes.
  uint32_t nlog;         // Number of log blocks
  uint32_t logstart;     // Block number of first log block
  uint32_t inodestart;   // Block number of first inode block
  uint32_t bmapstart;    // Block number of first free map block
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint32_t))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  uint16_t type;           // File type
  uint16_t major;          // Major device number (T_DEV only)
  uint16_t minor;          // Minor device number (T_DEV only)
  uint16_t nlink;          // Number of links to inode in file system
  uint32_t size;            // Size of file (bytes)
  uint32_t addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  uint16_t inum;
  char name[DIRSIZ];
};

#define NINODE 200

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

#endif
