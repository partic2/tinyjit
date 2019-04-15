#define EM_TCC_TARGET EM_ARM

/* relocation type for 32 bit data relocation */
#define R_DATA_32   R_ARM_ABS32
#define R_DATA_PTR  R_ARM_ABS32
#define R_JMP_SLOT  R_ARM_JUMP_SLOT
#define R_GLOB_DAT  R_ARM_GLOB_DAT
#define R_COPY      R_ARM_COPY
#define R_RELATIVE  R_ARM_RELATIVE

#define R_NUM       R_ARM_NUM

#define ELF_START_ADDR 0x00008000
#define ELF_PAGE_SIZE  0x1000

#define PCRELATIVE_DLLPLT 1
#define RELOCATE_DLLPLT 0


