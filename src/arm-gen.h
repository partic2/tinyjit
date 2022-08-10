
#ifndef _ARM_GEN_H
#define _ARM_GEN_H 1



/* number of available registers */

#define NB_REGS            23


#ifndef TCC_CPU_VERSION
# define TCC_CPU_VERSION 5
#endif

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic integer register */
#define RC_FLOAT   0x0002 /* generic float register */
#define RC_R0      0x0004
#define RC_R1      0x0008
#define RC_R2      0x0010
#define RC_R3      0x0020
#define RC_R12     0x0040
#define RC_F0      0x0080
#define RC_F1      0x0100
#define RC_F2      0x0200
#define RC_F3      0x0400

#define RC_F4      0x0800
#define RC_F5      0x1000
#define RC_F6      0x2000
#define RC_F7      0x4000

#define RC_IRET    RC_R0  /* function return: integer register */
#define RC_LRET    RC_R1  /* function return: second integer register */
#define RC_FRET    RC_F0  /* function return: float register */

/* pretty names for the registers */
enum {
    TREG_R0 = 0,
    TREG_R1,
    TREG_R2,
    TREG_R3,
    TREG_R4,
    TREG_R5,
    TREG_R6,
    TREG_R7,
    TREG_R8,
    TREG_R9,
    TREG_R10,
    TREG_FP = 11,
    TREG_R12,

    TREG_SP = 13,
    TREG_LR,

    TREG_F0,
    TREG_F1,
    TREG_F2,
    TREG_F3,

    TREG_F4,
    TREG_F5,
    TREG_F6,
    TREG_F7,

};


#define T2CPR(t) (((t) & VT_TYPE) != VT_FLOAT32 ? 0x100 : 0)




/* pointer size, in bytes */
#define PTR_SIZE 4
#define REGISTER_SIZE 4




/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8


/* float abi */
#define ARM_SOFT_FLOAT 1
#define ARM_SOFTFP_FLOAT 2
#define ARM_HARD_FLOAT 3

#endif