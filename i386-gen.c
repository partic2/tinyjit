/*
 *  X86 code generator for TCC
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "i386-gen.h"
#include "xxx-gen.h"
#include "tccdef.h"
#include "xxx-link.h"
#include "tccelf.h"
#include "tccutils.h"




static struct reg_attr regs_attr [NB_REGS] = {
    /* eax */ {RC_INT | RC_CALLER_SAVED,REGISTER_SIZE},
    /* ecx */ {RC_INT | RC_CALLER_SAVED,REGISTER_SIZE},
    /* edx */ {RC_INT | RC_CALLER_SAVED,REGISTER_SIZE},
    /* ebx */ {RC_INT | RC_CALLEE_SAVED,REGISTER_SIZE},
    /* esp */ {RC_SPECIAL,REGISTER_SIZE},
    /* ebp */ {RC_SPECIAL,REGISTER_SIZE},
    /* esi */ {RC_INT | RC_CALLEE_SAVED,REGISTER_SIZE},
    /* edi */ {RC_INT | RC_CALLEE_SAVED,REGISTER_SIZE},
    /* st0 */ { RC_SPECIAL , 96 },
};

static unsigned long func_sub_sp_offset;

struct s_abi_config abi_config;


ST_FUNC void arch_gen_init(){
    abi_config.func_call=FUNC_CDECL;
}

ST_FUNC void arch_gen_deinit(){
}


/* XXX: make it faster ? */
static void g(int c)
{
    int ind1;
    ind1 = ind + 1;
    if (ind1 > cur_text_section()->data_allocated)
        section_realloc(cur_text_section(), ind1);
    cur_text_section()->data[ind]=c;
    ind = ind1;
}

static void o(unsigned int c)
{
    while (c) {
        g(c);
        c = c >> 8;
    }
}

static void gen_le16(int v)
{
    g(v);
    g(v >> 8);
}

static void gen_le32(int c)
{
    g(c);
    g(c >> 8);
    g(c >> 16);
    g(c >> 24);
}

/* output a symbol and patch all calls to it */
ST_FUNC void gsym_addr(int t, int a)
{
    while (t) {
        unsigned char *ptr = cur_text_section()->data + t;
        uint32_t n = read32le(ptr); /* next value */
        write32le(ptr, a - t - 4);
        t = n;
    }
}

ST_FUNC void gsym(int t)
{
    gsym_addr(t, ind);
}

/* instruction + 4 bytes data. Return the address of the data */
static int oad(int c, int s)
{
    int t;
    o(c);
    t = ind;
    gen_le32(s);
    return t;
}

static void gen_fill_nops(int bytes)
{
    while (bytes--)
      g(0x90);
}

/* generate jmp to a label */
#define gjmp2(instr,lbl) oad(instr,lbl)

/* output constant with relocation if 'r & VT_SYM' is true */
static void gen_addr32(int r, Sym *sym, int c)
{
    if (sym)
        greloc(cur_text_section() ,sym, ind, R_386_32);
    gen_le32(c);
}

static void gen_addrpc32(int r, Sym *sym, int c)
{
    if (sym)
        greloc(cur_text_section() ,sym, ind,  R_386_PC32);
    gen_le32(c - 4);
}

/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm(int op_reg, int r, Sym *sym, int c)
{
    op_reg = op_reg << 3;
    if ((r & VT_VALMASK) == VT_CONST) {
        /* constant memory reference */
        o(0x05 | op_reg);
        gen_addr32(r, sym, c);
    } else if ((r & VT_VALMASK) == VT_LOCAL) {
        /* currently, we use only ebp as base */
        if (c == (char)c) {
            /* short reference */
            o(0x45 | op_reg);
            g(c);
        } else {
            oad(0x85 | op_reg, c);
        }
    } else {
        g(0x00 | op_reg | (r & VT_VALMASK));
    }
}

/* load 'r' from value 'sv' */
ST_FUNC void load(int r, SValue *sv)
{
    int v, t, ft, fc, fr;
    SValue v1;

#ifdef TCC_TARGET_PE
    SValue v2;
    sv = pe_getimport(sv, &v2);
#endif

    fr = sv->r;
    ft = sv->type.t & VT_TYPE;
    fc = sv->c.i;

    ft &= VT_TYPE;

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        if (v == VT_LLOCAL) {
            v1.type.t = VT_INT32 ;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.i = fc;
            fr = r;
            if (!(regs_attr[fr].c & RC_INT))
                fr = get_reg_of_cls(RC_INT);
            load(fr, &v1);
        }
        if ((ft & VT_TYPE) == VT_FLOAT32) {
            o(0xd9); /* flds */
            r = 0;
        } else if ((ft & VT_TYPE) == VT_FLOAT64) {
            o(0xdd); /* fldl */
            r = 0;
        } else if (0) {/* VT_FLOAT96 */
            o(0xdb); /* fldt */
            r = 5;
        } else if ((ft & VT_TYPE) == VT_INT8) {
            o(0xbe0f);   /* movsbl */
        } else if ((ft & VT_TYPE) == (VT_INT8 | VT_UNSIGNED)) {
            o(0xb60f);   /* movzbl */
        } else if ((ft & VT_TYPE) == VT_INT16) {
            o(0xbf0f);   /* movswl */
        } else if ((ft & VT_TYPE) == (VT_INT16 | VT_UNSIGNED)) {
            o(0xb70f);   /* movzwl */
        } else{
            o(0x8b);     /* movl */
        }
        gen_modrm(r, fr, sv->sym, fc);
    } else {
        if (v == VT_CONST) {
            o(0xb8 + r); /* mov $xx, r */
            gen_addr32(fr, sv->sym, fc);
        } else if (v == VT_LOCAL) {
            if (fc) {
                o(0x8d); /* lea xxx(%ebp), r */
                gen_modrm(r, VT_LOCAL, sv->sym, fc);
            } else {
                o(0x89);
                o(0xe8 + r); /* mov %ebp, r */
            }
        } else if (v == VT_CMP) {
            oad(0xb8 + r, 0); /* mov $0, r */
            o(0x0f); /* setxx %br */
            o(fc);
            o(0xc0 + r);
        } else if (v == VT_JMP || v == VT_JMPI) {
            t = v & 1;
            oad(0xb8 + r, t); /* mov $1, r */
            o(0x05eb); /* jmp after */
            gsym(fc);
            oad(0xb8 + r, t ^ 1); /* mov $0, r */
        } else if (v != r) {
            o(0x89);
            o(0xc0 + r + v * 8); /* mov v, r */
        }
    }
}

/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *v)
{
    int fr, bt, ft, fc;

    ft = v->type.t;
    fc = v->c.i;
    fr = v->r & VT_VALMASK;
    ft &= VT_TYPE;
    bt = ft & VT_TYPE;
    /* XXX: incorrect if float reg to reg */
    if (bt == VT_FLOAT32) {
        o(0xd9); /* fsts */
        r = 2;
    } else if (bt == VT_FLOAT64) {
        o(0xdd); /* fstpl */
        r = 2;
    } else if (0) { /* VT_FLOAT96 */
        o(0xc0d9); /* fld %st(0) */
        o(0xdb); /* fstpt */
        r = 7;
    } else {
        if (is_same_size_int(bt,VT_INT16))
            o(0x66);
        if (is_same_size_int(bt,VT_INT8))
            o(0x88);
        else
            o(0x89);
    }
    if (fr == VT_CONST ||
        fr == VT_LOCAL ||
        (v->r & VT_LVAL)) {
        gen_modrm(r, v->r, v->sym, fc);
    } else if (fr != r) {
        o(0xc0 + fr + r * 8); /* mov r, fr */
    }
}

static void gadd_sp(int val)
{
    if (val == (char)val) {
        o(0xc483);
        g(val);
    } else {
        oad(0xc481, val); /* add $xxx, %esp */
    }
}

#if defined TCC_TARGET_PE
static void gen_static_call(int v)
{
    Sym *sym;

    sym = external_global_sym(v, &func_old_type, 0);
    oad(0xe8, -4);
    greloc(ind-4,sym, R_386_PC32);
}
#endif

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp)
{
    int r;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->sym)) {
        /* constant and relocation case */
        greloc(cur_text_section(),vtop->sym, ind,R_386_PC32);
        oad(0xe8 + is_jmp, vtop->c.i - 4); /* call/jmp im */
    } else {
        /* otherwise, indirect call */
        gen_ldr();
        o(0xff); /* call/jmp *r */
        o(0xd0 + vtop->r + (is_jmp << 4));
    }
}

static uint8_t fastcallw_regs[2] = { TREG_ECX, TREG_EDX };


ST_FUNC void gfunc_call(int nb_args,CType *ret_type)
{
    int size, align, r, args_size, i, i1, func_call,btype;
    args_size = 0;
    for(i = 0;i < nb_args; i++) {
        
        if (is_float(vtop->type.t)) {
            load(TREG_ST0,vtop); /* only one float register */
            vtop->r=TREG_ST0;
            vtop->c.r2=VT_CONST;
            if ((vtop->type.t & VT_TYPE) == VT_FLOAT32)
                size = 4;
            else if ((vtop->type.t & VT_TYPE) == VT_FLOAT64)
                size = 8;
            else{
                size = 12;
            }
            oad(0xec81, size); /* sub $xxx, %esp */
            if (size == 12)
                o(0x7cdb);
            else
                o(0x5cd9 + size - 4); /* fstp[s|l] 0(%esp) */
            g(0x24);
            g(0x00);
            args_size += size;
        } else if(is_integer(vtop->type.t)){
            /* simple type (currently always same size) */
            /* XXX: implicit cast ? */
            size=size_align_of_type(vtop->type.t,&align);
            gen_ldr();
            if(size<REGISTER_SIZE){
                size=REGISTER_SIZE;
            }
            if(size==REGISTER_SIZE){
                o(0x50 + vtop->r&VT_VALMASK); /* push r */
            }else if(size==REGISTER_SIZE*2){
                o(0x50 + vtop->c.r2); /* push r2 */
                o(0x50 + vtop->r&VT_VALMASK); /* push r */
            }else{
                return;
            }
        }
        vpop(1);
    }
    save_rc_upstack(RC_CALLER_SAVED,0);
    func_call = abi_config.func_call;
    /* fast call case */
    if (func_call == FUNC_FASTCALLW) {
        int fastcall_nb_regs;
        uint8_t *fastcall_regs_ptr;
        if (func_call == FUNC_FASTCALLW) {
            fastcall_regs_ptr = fastcallw_regs;
            fastcall_nb_regs = 2;
        }
        for(i = 0;i < fastcall_nb_regs; i++) {
            if (args_size <= 0)
                break;
            o(0x58 + fastcall_regs_ptr[i]); /* pop r */
            /* XXX: incorrect for struct/floats */
            args_size -= 4;
        }
    }
    gcall_or_jmp(0);

    if (args_size && func_call != FUNC_STDCALL && func_call != FUNC_FASTCALLW)
        gadd_sp(args_size);
    vpop(1);
    vpushi(0);
    vtop->type=*ret_type;
    btype=ret_type->t;
    if(is_integer(btype)&&size_align_of_type(btype,&align)<=4){
        vtop->r=TREG_EAX;
    }else if(is_same_size_int(btype,VT_INT64)){
        vtop->r=TREG_EAX;
        vtop->c.r2=TREG_EDX;
    }else if(btype==VT_FLOAT32 || btype==VT_FLOAT64){
        vtop->r=VT_LOCAL|VT_LVAL;
        size=size_align_of_type(btype,&align);
        vtop->c.i=get_temp_local_var(size,align);
        store(TREG_ST0,vtop);
        o(0xd8dd);
    }else{
        tcc_error("unsuport value type.");
    }
}

#ifdef TCC_TARGET_PE
#define FUNC_PROLOG_SIZE (10)
#else
#define FUNC_PROLOG_SIZE (9)
#endif

ST_FUNC void gfunc_prolog()
{
    int align, size, i , i1, func_call,fastcall_nb_regs;
    SValue *sv;
    Sym *sym;
    CType *type;
    struct reg_attr *rAttr;

    func_call = abi_config.func_call;
    loc = 0;

    if (func_call == FUNC_FASTCALLW) {
        fastcall_nb_regs = 2;
    } else {
        fastcall_nb_regs = 0;
    }
    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;
   
    /* argument */
    i1=abi_config.func_call==FUNC_FASTCALLW ? 0 : 8;

    for(sv=__vstack;sv<=vtop;sv++){
        if(abi_config.func_call==FUNC_FASTCALLW && i1 < 8){
            size=size_align_of_type(sv->type.t,&align);
            if(size<=4){
                sv->r=fastcallw_regs[i1];
                i1+=4;
            }else if(size==8){
                sv->r=fastcallw_regs[i1];
                i1+=4;
                if(i1==2){
                    sv->type.t=VT_INT32;
                }else{
                    sv->c.r2=fastcallw_regs[i1];
                    i1+=4;
                }
            }
        }else{
            sv->r=VT_LVAL|VT_LOCAL;
            sv->c.i=i1;
            size=size_align_of_type(sv->type.t,&align);
            i1+=(size+3)&(~3);
        }
	}
    /* start addr of extend(stack) args */
    vpushi(8);
    vtop->r=VT_LOCAL;
    
     /* addr to next inst */
    vpushi(4);
    vtop->r=VT_LOCAL|VT_LVAL;

    for(i=0;i<NB_REGS;i++){
        rAttr=get_reg_attr(i);
        if(rAttr->c&RC_CALLEE_SAVED){
            rAttr->s|=RS_LOCKED;
        }
    }
}


ST_FUNC void gfunc_epilog()
{
    addr_t v, saved_ind;
    SValue *sv;
    int btype,r,n,align;

    /* set return value */
    btype=vtop->type.t;

    if(size_align_of_type(btype,&align)<=4){
        load(TREG_EAX,vtop);
    }else if((btype==VT_INT64) || (btype==(VT_INT64|VT_UNSIGNED))){
        gen_lexpand();
        gen_ldr_reg(TREG_EDX);
        vswap();
        gen_ldr_reg(TREG_EAX);
        vpop(1);
    }else if((btype==VT_FLOAT32) || (btype==VT_FLOAT64)){
        load(TREG_ST0,vtop);
    }else if(btype==VT_VOID){
        /* do nothing */
    }
    vpop(1);

    o(0xc9); /* leave */
    n=0;
    if(abi_config.func_call == FUNC_STDCALL || abi_config.func_call == FUNC_FASTCALLW){
        for(sv=__vstack;sv<vtop-2;sv++){
            n+=size_align_of_type(sv->type.t,&align);
        }
        if(abi_config.func_call==FUNC_FASTCALLW){
            n-=8;
            n=n>=0?n:0;
        }
    }
    
    
    if (n==0) {
        o(0xc3); /* ret */
    } else { 
        o(0xc2); /* ret n */
        g(n);
        g(n);
    }


    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
    /* align local size to word & save local variables */
    v = (-loc + 3) & -4;
    {
        o(0xe58955);  /* push %ebp, mov %esp, %ebp */
        o(0xec81);  /* sub esp, stacksize */
        gen_le32(v);
#ifdef TCC_TARGET_PE
        o(0x90);  /* adjust to FUNC_PROLOG_SIZE */
#endif
    }
    ind=saved_ind;
    vpop(vtop-__vstack);
    loc = 0;
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t)
{
    return gjmp2(0xe9, t);
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a)
{
    int r;
    r = a - ind - 2;
    if (r == (char)r) {
        g(0xeb);
        g(r);
    } else {
        oad(0xe9, a - ind - 5);
    }
}

ST_FUNC void gtst_addr(int inv, int a)
{
    int v = vtop->r & VT_VALMASK;
    if (v == VT_CMP) {
        inv ^= vtop->c.i;
        vpop(1);
        a -= ind + 2;
        if (a == (char)a) {
            g(inv - 32);
            g(a);
        } else {
            g(0x0f);
            oad(inv - 16, a - 4);
        }
    } else if ((v & ~1) == VT_JMP) {
        if ((v & 1) != inv) {
            gjmp_addr(a);
            gsym(vtop->c.i);
        } else {
            gsym(vtop->c.i);
            o(0x05eb);
            gjmp_addr(a);
        }
        vpop(1);
    }
}

/* generate a test. set 'inv' to invert test. Stack entry is popped */
ST_FUNC int gtst(int inv, int t)
{
    int v = vtop->r & VT_VALMASK;
    if (v == VT_CMP) {
        /* fast case : can jump directly since flags are set */
        g(0x0f);
        t = gjmp2((vtop->c.i - 16) ^ inv, t);
    } else if (v == VT_JMP || v == VT_JMPI) {
        /* && or || optimization */
        if ((v & 1) == inv) {
            /* insert vtop->c jump list in t */
            uint32_t n1, n = vtop->c.i;
            if (n) {
                while ((n1 = read32le(cur_text_section()->data + n)))
                    n = n1;
                write32le(cur_text_section()->data + n, t);
                t = vtop->c.i;
            }
        } else {
            t = gjmp(t);
            gsym(vtop->c.i);
        }
    }
    vpop(1);
    return t;
}

/* generate an integer binary operation */
ST_FUNC void gen_opi(int op)
{
    int r, fr, opc, c;

    switch(op) {
    case TOK_ADDC1: /* add with carry generation */
        opc = 0;
    gen_op8:
        if ((vtop->r & (VT_VALMASK | VT_LVAL | !vtop->sym)) == VT_CONST) {
            /* constant case */
            vswap();
            gen_ldr();
            r=vtop->r;
            vswap();
            c = vtop->c.i;
            if (c == (char)c) {
                /* generate inc and dec for smaller code */
                if (c==1 && opc==0 && op != TOK_ADDC1) {
                    o (0x40 | r); // inc
                } else if (c==1 && opc==5 && op != TOK_SUBC1) {
                    o (0x48 | r); // dec
                } else {
                    o(0x83);
                    o(0xc0 | (opc << 3) | r);
                    g(c);
                }
            } else {
                o(0x81);
                oad(0xc0 | (opc << 3) | r, c);
            }
        } else {
            vswap();
            gen_ldr();
            vswap();
            gen_ldr();
            r = vtop[-1].r;
            fr = vtop[0].r;
            o((opc << 3) | 0x01);
            o(0xc0 + r + fr * 8); 
        }
        vpop(1);
        vtop->sym=NULL;
        if (op >= TOK_ULT && op <= TOK_GT) {
            vtop->r = VT_CMP;
            vtop->c.i = op;
        }
        break;
    case TOK_SUBC1: /* sub with carry generation */
        opc = 5;
        goto gen_op8;
    case TOK_ADDC2: /* add with carry use */
        opc = 2;
        goto gen_op8;
    case TOK_SUBC2: /* sub with carry use */
        opc = 3;
        goto gen_op8;
    case TOK_AND:
        opc = 4;
        goto gen_op8;
    case TOK_XOR:
        opc = 6;
        goto gen_op8;
    case TOK_OR:
        opc = 1;
        goto gen_op8;
    case TOK_MULL:
        vswap();
        gen_ldr();
        r=vtop->r;
        vswap();
        gen_ldr();
        fr=vtop->r;
        o(0xaf0f); /* imul fr, r */
        o(0xc0 + fr + r * 8);
        vpop(1);
        vtop->sym=NULL;
        break;
    case TOK_SHL:
        opc = 4;
        goto gen_shift;
    case TOK_SHR:
        opc = 5;
        goto gen_shift;
    case TOK_SAR:
        opc = 7;
    gen_shift:
        opc = 0xc0 | (opc << 3);
        if ((vtop->r & (VT_VALMASK | VT_LVAL | !vtop->sym)) == VT_CONST) {
            /* constant case */
            vswap();
            gen_ldr();
            r=vtop->r;
            vswap();
            c = vtop->c.i & 0x1f;
            o(0xc1); /* shl/shr/sar $xxx, r */
            o(opc | r);
            g(c);
        } else {
            /* we generate the shift in ecx */
            save_reg_upstack(TREG_ECX,1);
            gen_ldr_reg(TREG_ECX);
			get_reg_attr(TREG_ECX)->s|=RS_LOCKED;
            vswap();
            gen_ldr();
            r=vtop->r;
            vswap();
            o(0xd3); /* shl/shr/sar %cl, r */
            o(opc | r);
			get_reg_attr(TREG_ECX)->s&=~RS_LOCKED;
        }
        vpop(1);
        vtop->sym=NULL;
        break;
    case TOK_UDIV:
    case TOK_UMOD:
    case TOK_UMULL:
        /* first operand must be in eax */
        /* XXX: need better constraint for second operand */
        save_reg_upstack(TREG_EDX,2);
        save_reg_upstack(TREG_EAX, 2);
        save_reg_upstack(TREG_ECX,2);
        gen_ldr_reg(TREG_ECX);
        fr=vtop->r;
        vswap();
        gen_ldr_reg(TREG_EAX);
        r=vtop->r;
        vswap();
        
        vpop(1);
        
        if (op == TOK_UMULL) {
            o(0xf7); /* mul fr */
            o(0xe0 + fr);
            vtop->c.r2 = TREG_EDX;
            r = TREG_EAX;
            vtop->type.t=VT_INT64;
        } else {
            if (op == TOK_UDIV || op == TOK_UMOD) {
                o(0xf7d231); /* xor %edx, %edx, div fr, %eax */
                o(0xf0 + fr);
            } else {
                o(0xf799); /* cltd, idiv fr, %eax */
                o(0xf8 + fr);
            }
            if (op == TOK_UMOD)
                r = TREG_EDX;
            else
                r = TREG_EAX;
        }
        vtop->r = r;
        vtop->sym=NULL;
        break;
    default:
        opc = 7;
        goto gen_op8;
    }
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranteed to have the same floating point type */
/* XXX: need to use ST1 too */
ST_FUNC void gen_opf(int op)
{
    int a, ft, fc, swapped, r,size,align;

    /* convert constants to memory references */
    if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        vswap();
        load(TREG_ST0,vtop);
        vtop->r=TREG_ST0;
        vtop->c.r2=VT_CONST;
        vswap();
    }
    if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST){
        load(TREG_ST0,vtop);
        vtop->r=TREG_ST0;
        vtop->c.r2=VT_CONST;
    }

    /* must put at least one value in the floating point register */
    if ((vtop[-1].r & VT_LVAL) &&
        (vtop[0].r & VT_LVAL)) {
        vswap();
        load(TREG_ST0,vtop);
        vtop->r=TREG_ST0;
        vtop->c.r2=VT_CONST;
        vswap();
    }
    swapped = 0;
    /* swap the stack if needed so that t1 is the register and t2 is
       the memory reference */
    if (vtop[-1].r & VT_LVAL) {
        vswap();
        swapped = 1;
    }
    if (op >= TOK_ULT && op <= TOK_GT) {
        /* load on stack second operand */
        load(TREG_ST0, vtop);
        save_reg_upstack(TREG_EAX,0); /* eax is used by FP comparison code */
        if (op == TOK_GE || op == TOK_GT)
            swapped = !swapped;
        else if (op == TOK_EQ || op == TOK_NE)
            swapped = 0;
        if (swapped)
            o(0xc9d9); /* fxch %st(1) */
        if (op == TOK_EQ || op == TOK_NE)
            o(0xe9da); /* fucompp */
        else
            o(0xd9de); /* fcompp */
        o(0xe0df); /* fnstsw %ax */
        if (op == TOK_EQ) {
            o(0x45e480); /* and $0x45, %ah */
            o(0x40fC80); /* cmp $0x40, %ah */
        } else if (op == TOK_NE) {
            o(0x45e480); /* and $0x45, %ah */
            o(0x40f480); /* xor $0x40, %ah */
            op = TOK_NE;
        } else if (op == TOK_GE || op == TOK_LE) {
            o(0x05c4f6); /* test $0x05, %ah */
            op = TOK_EQ;
        } else {
            o(0x45c4f6); /* test $0x45, %ah */
            op = TOK_EQ;
        }
        vpop(1);
        vtop->r = VT_CMP;
        vtop->c.i = op;
    } else {
        /* no memory reference possible for long double operations */
        /*
        if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
            load(TREG_ST0, vtop);
            swapped = !swapped;
        }
        */
        
        switch(op) {
        default:
        case TOK_ADD:
            a = 0;
            break;
        case TOK_SUB:
            a = 4;
            if (swapped)
                a++;
            break;
        case TOK_MULL:
            a = 1;
            break;
        case TOK_UDIV:
            a = 6;
            if (swapped)
                a++;
            break;
        }
        ft = vtop->type.t;
        fc = vtop->c.i;
        /*
        if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            o(0xde); *//* fxxxp %st, %st(1) *//*
            o(0xc1 + (a << 3));
        } else {*/
            /* if saved lvalue, then we must reload it */
            r = vtop->r;
            if ((r & VT_VALMASK) == VT_LLOCAL) {
                SValue v1;
                r = get_reg_of_cls(RC_INT);
                v1.type.t = VT_INT32 ;
                v1.r = VT_LOCAL | VT_LVAL;
                v1.c.i = fc;
                load(r, &v1);
                fc = 0;
            }

            if ((ft & VT_TYPE) == VT_FLOAT64)
                o(0xdc);
            else
                o(0xd8);
            gen_modrm(a, r, vtop->sym, fc);
        //}
        vpop(1);
    }
    size=size_align_of_type(vtop->type.t,&align);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->c.i=get_temp_local_var(vtop->type.t,align);
    store(TREG_ST0,vtop);
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
ST_FUNC void gen_cvt_itof(int t)
{
    int size,align;
    gen_ldr();
    if (vtop->type.t==VT_INT64) {
        /* signed long long to float/double/long double (unsigned case
           is handled generically) */
        o(0x50 + vtop->c.r2); /* push r2 */
        o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
        o(0x242cdf); /* fildll (%esp) */
        o(0x08c483); /* add $8, %esp */
    } else if(vtop->type.t == (VT_INT32 | VT_UNSIGNED)) {
        /* unsigned int to float/double/long double */
        o(0x6a); /* push $0 */
        g(0x00);
        o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
        o(0x242cdf); /* fildll (%esp) */
        o(0x08c483); /* add $8, %esp */
    } else if(vtop->type.t == VT_INT32){
        /* int to float/double/long double */
        o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
        o(0x2404db); /* fildl (%esp) */
        o(0x04c483); /* add $4, %esp */
    }else{
        tcc_error("not supported type.");
    }
    size=size_align_of_type(vtop->type.t,&align);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->c.i=get_temp_local_var(vtop->type.t,align);
    store(TREG_ST0,vtop);
}

ST_FUNC void gen_cvt_ftoi(int type){
    int saved_cw,new_cw,r,l;
    
    load(TREG_ST0,vtop);
    vpop(1);

    /* save fpu control word */
    saved_cw=get_temp_local_var(4,2);
    
    /* fnstcw xxx(%ebp) */
    if(saved_cw == (char) saved_cw){
        g(0xd9);
        g(0x7d);
        g(saved_cw);
    }else{
        g(0xd9);
        g(0xbd);
        o(saved_cw);
    }
    vpushi(saved_cw);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->type.t=VT_INT16;

    /* set fpu control word */
    vpushi(0xffff00ff);
    gen_opi(TOK_AND);
    vpushi(0xc00);
    gen_opi(TOK_OR);
    gen_ldr();
    r=vtop->r;
    get_reg_attr(r)->s|=RS_LOCKED;
    vpop(1);
    
    new_cw=saved_cw+2;
    vpushi(new_cw);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->type.t=VT_INT16;
    store(r,vtop);
    get_reg_attr(r)->s&=~RS_LOCKED;
    vpop(1);

    /* locked temp variable */
    vpushi(saved_cw);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->type.t=VT_INT32;

    /* fldcw  xxx(%ebp) */
    if(new_cw == (char) new_cw){
        g(0xd9);
        g(0x6d);
        g(new_cw);
    }else{
        g(0xd9);
        g(0xad);
        o(saved_cw);
    }
    
    switch(type){
        case VT_INT8:
        case VT_INT16:
        case VT_INT32:
        /* fistpl xxx(%ebp) */
        l=get_temp_local_var(4,4);
        vpushi(l);
        vtop->type.t=type;
        vtop->r=VT_LOCAL|VT_LVAL;
        if(l == (char)l){
            g(0xdb);
            g(0x5d);
            g(l);
        }else{
            g(0xdb);
            g(0x9d);
            o(l);
        }        
        break;
        case VT_INT64:
        /* fistpll xxx(%ebp) */
        l=get_temp_local_var(8,8);
        vpushi(l);
        vtop->type.t=type;
        vtop->r=VT_LOCAL|VT_LVAL;
        if(l == (char)l){
            g(0xdb);
            g(0x7d);
            g(l);
        }else{
            g(0xdf);
            g(0xbd);
            o(l);
        }        
        break;
    }
    /* restore vfp control word */

    /* fldcw  xxx(%ebp) */
    if(saved_cw == (char) saved_cw){
        g(0xd9);
        g(0x6d);
        g(saved_cw);
    }else{
        g(0xd9);
        g(0xad);
        o(saved_cw);
    }

    /* free local variable */
    vswap();
    vpop(1);
    vtop->sym=NULL;
}
/* computed goto support */
ST_FUNC void ggoto(void)
{
    gcall_or_jmp(1);
    vpop(1);
}

ST_FUNC struct reg_attr *get_reg_attr(int r){
    return &regs_attr[r];
}