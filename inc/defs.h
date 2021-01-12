#define uint uint32_t
#define ushort uint16_t

// parameters
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NDEV         10  // maximum major device number
#define NINODE       50  // maximum number of active i-nodes
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
