
#ifndef _XXX_GEN_H
#define _XXX_GEN_H 1

#include "tccdef.h"
#include "tccelf.h"

/* The current value can be: */
#define VT_VALMASK   0x00ff  /* mask for value location, register or: */
#define VT_CONST     0x00f0  /* constant in vc (must be first non register value) */
#define VT_LLOCAL    0x00f1  /* lvalue, offset on stack */
#define VT_LOCAL     0x00f2  /* offset on stack */
#define VT_CMP       0x00f3  /* the value is stored in processor flags (in vc) */
#define VT_JMP       0x00f4  /* value is the consequence of jmp true (even) */
#define VT_JMPI      0x00f5  /* value is the consequence of jmp false (odd) */

#define VT_LVAL      0x0100  /* var is an lvalue */

#define TCCERR_GEN_START 0x100
#define TCCERR_NOT_IMPLEMENT 0x101


/* constant value */
typedef union CValue {
    double d;
    float f;
    uint64_t i;
    uint8_t r2; /* the second register to store multi-word SValue. should be VT_CONST if not used*/
} CValue;

/* value on vstack 
DO NOT use same register to store different multi-word SValue */
typedef struct SValue {
    CType type;      /* type */
    uint16_t r;      /* register + flags */
    CValue c;              /* constant, if VT_CONST */
	struct Sym *sym;
} SValue;

extern SValue *__vstack;
extern SValue *vtop;
extern SValue *pvtop;

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic integer register */
#define RC_FLOAT   0x0002 /* generic float register */

/* the register is saved by callee. after calling gfunc_prolog, these registers will be locked(r->s|=RS_LOCKED) 
If these register was used, their should be restore before calling  gfunc_epilog.*/
#define RC_CALLEE_SAVED  0x0004 
/*the register is saved by caller. before gfunc_call, these registers will be saved.*/
#define RC_CALLER_SAVED  0x0008 

#define RC_SPECIAL 0x0010

/* TODO: The register should be unique on svalue stack */
#define RC_UNIQUE_ONSTACK 0x0020

/* register status */

/* register has been locked and should not be free */
#define RS_LOCKED 0x0001


struct reg_attr{
    /* register class */
    uint32_t c;
	char size;
    char padding;
	/* register status */
	uint16_t s;
};


/* warning: the following compare tokens depend on i386 asm code */
#define TOK_ULT 0x92
#define TOK_UGE 0x93
#define TOK_EQ  0x94
#define TOK_NE  0x95
#define TOK_ULE 0x96
#define TOK_UGT 0x97
#define TOK_LT  0x9c
#define TOK_GE  0x9d
#define TOK_LE  0x9e
#define TOK_GT  0x9f

#define TOK_SHL   0x01 /* shift left */
#define TOK_SAR   0x02 /* signed shift right */
#define TOK_SHR   0x03 /* unsigned shift right */
#define TOK_OR    0x04 /* bits or */
#define TOK_AND   0x05 /* bits and*/
#define TOK_XOR   0x06 /* bits xor*/


/* optional */
#define TOK_NSET  0x07
#define TOK_NCLEAR 0x08

#define TOK_MULL     '*' /* signed mul */
#define TOK_UMULL    0xc2 /* unsigned 32x32 -> 64 mul */

#define TOK_ADD      '+'
#define TOK_ADDC1    0xc3 /* add with carry generation */
#define TOK_ADDC2    0xc4 /* add with carry use */

#define TOK_SUB      '-'
#define TOK_SUBC1    0xc5 /* add with carry generation */
#define TOK_SUBC2    0xc6 /* add with carry use */

#define TOK_DIV   '/'
#define TOK_UDIV  0xb0 /* unsigned division */
#define TOK_UMOD  0xb1 /* unsigned modulo */

#define ERR_OPERAND_TYPE 0x11;


extern uint32_t ind, loc;

ST_FUNC uint32_t get_VT_INT_TYPE_of_size(unsigned int size);
ST_FUNC unsigned int size_align_of_type(uint32_t type,unsigned int *align);
ST_FUNC void vpush(CType *type);
ST_FUNC void vpop(int n);
ST_FUNC void vpushi(int v);
ST_FUNC void vpushl(int ty, unsigned long long v);
ST_FUNC void vset(CType *type, int r, int v);
ST_FUNC void vsetc(CType *type, int r, CValue *vc);
ST_FUNC void vswap(void);
ST_FUNC void vrotb(int n);
ST_FUNC void vpushv(SValue *v);

ST_FUNC int is_reg_free(int r,int upstack);

/* allocate a local variable on stack, return sv.c.i*/
ST_FUNC int alloc_local(int size,int align);
/* save reg of rc to the memory stack, and mark it as being free,i
f seen up to (vtop - n) stack entry 
rc: only save register if r.c & rc == rc*/
ST_FUNC void save_rc_upstack(int rc,int n);
/* save r to the memory stack, and mark it as being free,
if seen up to (vtop - n) stack entry */
ST_FUNC void save_reg_upstack(int r, int n);
/* find a free register of class 'rc' */
ST_FUNC int get_reg_of_cls(int rc);
/*load vtop into register, return vtop->r if successed */
ST_FUNC int gen_ldr();
/*load vtop into r.*/
ST_FUNC void gen_ldr_reg(int r);
/* get c.i for a temperory local variable */
ST_FUNC int get_temp_local_var(int size,int align);
/* clear all temperory local variables record */
ST_FUNC void clear_temp_local_var_list();

/* get address of vtop (vtop MUST BE an lvalue) */
ST_FUNC void gen_addr_of();
ST_FUNC void gen_lval_of();
ST_FUNC int is_lval();

/* move lvalue */
ST_FUNC int gen_lval_offset(int offset);

/* expand vtop into 2 SValues 
    before calling:
    vtop: the 2-words value.
    after calling:
    vtop: the high-order part.
    vtop-1: the low-order part.
*/
ST_FUNC void gen_lexpand();

static inline void vdup(void)
{
    vpushv(vtop);
}

#if PTR_SIZE == 4
static inline void greloc(Section *s, Sym *sym, unsigned long offset, int type)
{
    greloca(s, sym, offset, type, 0);
}
#endif
ST_FUNC void xxx_gen_init();
ST_FUNC void xxx_gen_deinit();

/* ------------- arch-gen----------- */

ST_FUNC void arch_gen_init();
ST_FUNC void arch_gen_deinit();

/* load 'r' from value 'sv' */
ST_FUNC void load(int r, SValue *sv);
/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *v);
/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address.
   
   after calling, returned data will be pushed on vstack
   
   ret_type:return type.*/
ST_FUNC void gfunc_call(int nb_args,CType *ret_type);

/* generate function prolog, set the latest 'nb_args' SValue on vstack
    vstack change like below:
	before calling:
	vtop:the last argument
	vtop-1:the next to last argument
	...
	vtop-nb_args (__vstack):the first argument
	
	after calling:
	vtop:addr of next instruction(to return caller) 
	vtop-1:start addr of extend(stack) args.
	vtop-2:the last argument
	vtop-3:the next to last argument
	...
*/
ST_FUNC void gfunc_prolog();

/* generate function epilog 
	before calling:
	vtop:the return value.
	vtop-1:addr of next instruction(to return caller) 
	vtop-2:start addr of extend(stack) args.
	vtop-3:the last argument
	...

	after calling:
	vtop-2 to vtop will be poped.
*/
ST_FUNC void gfunc_epilog();

/* output a symbol 'a' and patch all calls (stored as linked-list in 't') to it */
ST_FUNC void gsym_addr(int t, int a);
/* generate a jump to a label */
ST_FUNC int gjmp(int t);
/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a);

ST_FUNC int gtst(int inv, int t);

ST_FUNC void gen_opi(int op);
ST_FUNC void gen_opf(int op);
ST_FUNC void ggoto(void);

ST_FUNC void gen_cvt_itof(int t);
ST_FUNC void gen_cvt_ftoi(int t);
ST_FUNC struct reg_attr *get_reg_attr(int r);

#endif