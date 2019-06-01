
/* number of available registers */
#define NB_REGS         25
#define CONFIG_TCC_ASM

/* pretty names for the registers */
enum {
    TREG_RAX = 0,
    TREG_RCX,
    TREG_RDX,
    TREG_RBX,
    TREG_RSP,
    TREG_RBP,
    TREG_RSI,
    TREG_RDI,

    TREG_R8  = 8,
    TREG_R9,
    TREG_R10,
    TREG_R11,
    TREG_R12,
    TREG_R13,
    TREG_R14,
    TREG_R15,

    TREG_XMM0 = 16,
    TREG_XMM1 = 17,
    TREG_XMM2 = 18,
    TREG_XMM3 = 19,
    TREG_XMM4 = 20,
    TREG_XMM5 = 21,
    TREG_XMM6 = 22,
    TREG_XMM7 = 23,

    TREG_ST0 = 24,

    TREG_MEM = 0x20
};

#define REX_BASE(reg) (((reg) >> 3) & 1)
#define REG_VALUE(reg) ((reg) & 7)

/* return registers for function */
#define REG_IRET TREG_RAX /* single word int return register */
#define REG_LRET TREG_RDX /* second word return register (for long long) */
#define REG_FRET TREG_XMM0 /* float return register */
#define REG_QRET TREG_XMM1 /* second float return register */

/* defined if function parameters must be evaluated in reverse order */
#define INVERT_FUNC_PARAMS

/* pointer size, in bytes */
#define PTR_SIZE 8

#define REGISTER_SIZE 8

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  16
#define LDOUBLE_ALIGN 16
/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     16
