
#ifndef _I386_GEN_H
#define _I386_GEN_H 1



/* number of available registers */
#define NB_REGS         9


/* pretty names for the registers */
enum {
    TREG_EAX = 0,
    TREG_ECX,
    TREG_EDX,
    TREG_EBX,
    TREG_ESP,
    TREG_EBP,
    TREG_ESI,
    TREG_EDI,
    TREG_ST0,
};



/* defined if structures are passed as pointers. Otherwise structures
   are directly pushed on stack. */
/* #define FUNC_STRUCT_PARAM_AS_PTR */

/* pointer size, in bytes */
#define PTR_SIZE 4

/* size of general purpose register */
#define REGISTER_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  12
#define LDOUBLE_ALIGN 4
/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8

extern struct s_abi_config{
	char func_call;
} abi_config;
/* func_call */
#define FUNC_CDECL     0 /* standard c call */
#define FUNC_STDCALL   1 /* pascal c call */
#define FUNC_FASTCALLW 5 /* first parameter in %ecx, %edx */

#endif

