#ifndef INC_MMU_H
#define INC_MMU_H

/*
 * See Chapter 12 of ARM Cortex-A Series Programmer's Guide for ARMv8-A
 * and Chapter D5 of Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile.
 */
#define PGSIZE 4096

/* Memory region attributes */
#define MT_DEVICE_nGnRnE        0x0
#define MT_NORMAL               0x1
#define MT_NORMAL_NC            0x2
#define MT_DEVICE_nGnRnE_FLAGS  0x00
#define MT_NORMAL_FLAGS         0xFF    /* Inner/Outer Write-back Non-transient RW-Allocate */
#define MT_NORMAL_NC_FLAGS      0x44    /* Inner/Outer Non-cacheable */

#define MAIR_VALUE              ((MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | \
                                (MT_NORMAL_FLAGS << (8 * MT_NORMAL)) | \
                                (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NC)))

#define SH_OUTER        (2 << 8)
#define SH_INNER        (3 << 8)       /* Inner shareable */
#define AF_USED         (1 << 10)

#define PTE_NORMAL_NC   ((MT_NORMAL_NC << 2) | AF_USED | SH_OUTER)
#define PTE_NORMAL      ((MT_NORMAL << 2) | AF_USED | SH_OUTER)
// #define PTE_NORMAL      ((MT_NORMAL << 2) | AF_USED | SH_INNER)
#define PTE_DEVICE      ((MT_DEVICE_nGnRnE << 2) | AF_USED)

/* PTE flags */
#define PTE_VALID       0x1

#define PTE_TABLE       0x3             /* Table in level 0/1/2 */
#define PTE_BLOCK       0x1             /* Block in level 1/2 */
#define PTE_PAGE        0x3             /* Page in level 3 */

#define PTE_KERN        (0 << 6)
#define PTE_USER        (1 << 6)
#define PTE_NG          (1 << 11)

/* 1GB/2MB block for kernel, and 4KB page for user. */
#define PTE_KDATA       (PTE_KERN | PTE_NORMAL | PTE_BLOCK)
#define PTE_KDEV        (PTE_KERN | PTE_DEVICE | PTE_BLOCK)
#define PTE_UDATA       (PTE_USER | PTE_NORMAL | PTE_PAGE)
// #define PTE_UDATA       (PTE_USER | PTE_NORMAL_NC | PTE_PAGE)
// #define PTE_UDATA       (PTE_USER | PTE_NORMAL | PTE_PAGE | PTE_NG)

/* Address in table or block entry, only support 32 bit physical address. */
#define PTE_ADDR(pte)   ((pte) & ~0xFFF)
#define PTE_FLAGS(pte)  ((pte) &  0xFFF)

/* Translation Control Register */
#define TCR_T0SZ        (64 - 48) 
#define TCR_T1SZ        ((64 - 48) << 16)
#define TCR_TG0_4K      (0 << 14)
#define TCR_TG1_4K      (2 << 30)           /* Different from TG0 */
#define TCR_SH0_INNER   (3 << 12)
#define TCR_SH1_INNER   (3 << 28)
#define TCR_SH0_OUTER   (2 << 12)
#define TCR_SH1_OUTER   (2 << 28)
// #define TCR_ORGN0_IRGN0 ((0 << 10) | (0 << 8))
#define TCR_ORGN0_IRGN0 ((1 << 10) | (1 << 8))
#define TCR_ORGN1_IRGN1 ((1 << 26) | (1 << 24))

#define TCR_VALUE       (TCR_T0SZ           | TCR_T1SZ      |   \
                         TCR_TG0_4K         | TCR_TG1_4K    |   \
                         TCR_SH0_OUTER      | TCR_SH1_OUTER |   \
                         TCR_ORGN0_IRGN0    | TCR_ORGN1_IRGN1)

#endif
