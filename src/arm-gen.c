/*
 *  ARMv4 code generator for TCC
 *
 *  Copyright (c) 2003 Daniel Glöckner
 *  Copyright (c) 2012 Thomas Preud'homme
 *
 *  Based on i386-gen.c by Fabrice Bellard
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

#include "arm-gen.h"
#include "xxx-gen.h"
#include "tccutils.h"



static struct reg_attr reg_attrs[NB_REGS] = {
    /* r0 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* r1 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* r2 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* r3 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* r4 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r5 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r6 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r7 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r8 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r9 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* r10 */ {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* fp */ {RC_SPECIAL, REGISTER_SIZE},
    /* r12 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* sp */ {RC_SPECIAL, REGISTER_SIZE},
    /* lr */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},

    /* f0 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* f1 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* f2 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* f3 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},

    /* d4/s8 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* d5/s10 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* d6/s12 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* d7/s14 */ {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
};

static int func_sub_sp_offset,last_itod_magic;
static int leaffunc;
struct s_abi_config abi_config;


ST_FUNC void arch_gen_init() {
  abi_config.float_abi=ARM_SOFT_FLOAT;
  abi_config.eabi=1;
}

ST_FUNC void arch_gen_deinit() {
}


ST_FUNC struct reg_attr *get_reg_attr(int r) { return reg_attrs + r; }

/******************************************************/


static void o(uint32_t i) {
  /* this is a good place to start adding big-endian support*/
  int ind1;
  ind1 = ind + 4;
  if (!cur_text_section())
    tcc_error("compiler error! This happens f.ex. if the compiler\n"
              "can't evaluate constant expressions outside of a function.");
  if (ind1 > cur_text_section()->data_allocated)
    section_realloc(cur_text_section(), ind1);
  cur_text_section()->data[ind++] = i & 255;
  i >>= 8;
  cur_text_section()->data[ind++] = i & 255;
  i >>= 8;
  cur_text_section()->data[ind++] = i & 255;
  i >>= 8;
  cur_text_section()->data[ind++] = i;
}

static uint32_t stuff_const(uint32_t op, uint32_t c) {
  int try_neg = 0;
  uint32_t nc = 0, negop = 0;

  switch (op & 0x1F00000) {
  case 0x800000: // add
  case 0x400000: // sub
    try_neg = 1;
    negop = op ^ 0xC00000;
    nc = -c;
    break;
  case 0x1A00000: // mov
  case 0x1E00000: // mvn
    try_neg = 1;
    negop = op ^ 0x400000;
    nc = ~c;
    break;
  case 0x200000: // xor
    if (c == ~0)
      return (op & 0xF010F000) | ((op >> 16) & 0xF) | 0x1E00000;
    break;
  case 0x0: // and
    if (c == ~0)
      return (op & 0xF010F000) | ((op >> 16) & 0xF) | 0x1A00000;
  case 0x1C00000: // bic
    try_neg = 1;
    negop = op ^ 0x1C00000;
    nc = ~c;
    break;
  case 0x1800000: // orr
    if (c == ~0)
      return (op & 0xFFF0FFFF) | 0x1E00000;
    break;
  }
  do {
    uint32_t m;
    int i;
    if (c < 256) /* catch undefined <<32 */
      return op | c;
    for (i = 2; i < 32; i += 2) {
      m = (0xff >> i) | (0xff << (32 - i));
      if (!(c & ~m))
        return op | (i << 7) | (c << i) | (c >> (32 - i));
    }
    op = negop;
    c = nc;
  } while (try_neg--);
  return 0;
}

// only add,sub
ST_FUNC void stuff_const_harder(uint32_t op, uint32_t v) {
  uint32_t x;
  x = stuff_const(op, v);
  if (x)
    o(x);
  else {
    uint32_t a[16], nv, no, o2, n2;
    int i, j, k;
    a[0] = 0xff;
    o2 = (op & 0xfff0ffff) | ((op & 0xf000) << 4);
    ;
    for (i = 1; i < 16; i++)
      a[i] = (a[i - 1] >> 2) | (a[i - 1] << 30);
    for (i = 0; i < 12; i++)
      for (j = i < 4 ? i + 12 : 15; j >= i + 4; j--)
        if ((v & (a[i] | a[j])) == v) {
          o(stuff_const(op, v & a[i]));
          o(stuff_const(o2, v & a[j]));
          return;
        }
    no = op ^ 0xC00000;
    n2 = o2 ^ 0xC00000;
    nv = -v;
    for (i = 0; i < 12; i++)
      for (j = i < 4 ? i + 12 : 15; j >= i + 4; j--)
        if ((nv & (a[i] | a[j])) == nv) {
          o(stuff_const(no, nv & a[i]));
          o(stuff_const(n2, nv & a[j]));
          return;
        }
    for (i = 0; i < 8; i++)
      for (j = i + 4; j < 12; j++)
        for (k = i < 4 ? i + 12 : 15; k >= j + 4; k--)
          if ((v & (a[i] | a[j] | a[k])) == v) {
            o(stuff_const(op, v & a[i]));
            o(stuff_const(o2, v & a[j]));
            o(stuff_const(o2, v & a[k]));
            return;
          }
    no = op ^ 0xC00000;
    nv = -v;
    for (i = 0; i < 8; i++)
      for (j = i + 4; j < 12; j++)
        for (k = i < 4 ? i + 12 : 15; k >= j + 4; k--)
          if ((nv & (a[i] | a[j] | a[k])) == nv) {
            o(stuff_const(no, nv & a[i]));
            o(stuff_const(n2, nv & a[j]));
            o(stuff_const(n2, nv & a[k]));
            return;
          }
    o(stuff_const(op, v & a[0]));
    o(stuff_const(o2, v & a[4]));
    o(stuff_const(o2, v & a[8]));
    o(stuff_const(o2, v & a[12]));
  }
}

ST_FUNC uint32_t encbranch(int pos, int addr, int fail) {
  addr -= pos + 8;
  addr /= 4;
  if (addr >= 0x1000000 || addr < -0x1000000) {
    if (fail)
      tcc_error("FIXME: function bigger than 32MB");
    return 0;
  }
  return 0x0A000000 | (addr & 0xffffff);
}

ST_FUNC int decbranch(int pos) {
  int x;
  x = *(uint32_t *)(cur_text_section()->data + pos);
  x &= 0x00ffffff;
  if (x & 0x800000)
    x -= 0x1000000;
  return x * 4 + pos + 8;
}

/* output a symbol and patch all calls to it */
ST_FUNC void gsym_addr(int t, int a) {
  uint32_t *x;
  int lt;
  while (t) {
    x = (uint32_t *)(cur_text_section()->data + t);
    t = decbranch(lt = t);
    if (a == lt + 4)
      *x = 0xE1A00000; // nop
    else {
      *x &= 0xff000000;
      *x |= encbranch(lt, a, 1);
    }
  }
}

ST_FUNC void gsym(int t) { gsym_addr(t, ind); }


static uint32_t vfpr(int r) {
  if (r < TREG_F0 || r > TREG_F7)
    tcc_error("compiler error! register is no vfp register");
  return r - TREG_F0;
}



static uint32_t intr(int r) {
  if (r == TREG_R12)
    return 12;
  if (r >= TREG_R0 && r <= TREG_LR)
    return r - TREG_R0;
  tcc_error("compiler error! register is no int register");
  return 0;
}

static void calcaddr(uint32_t *base, int *off, int *sgn, int maxoff,
                     unsigned shift) {
  if (*off > maxoff || *off & ((1 << shift) - 1)) {
    uint32_t x, y;
    x = 0xE280E000;
    if (*sgn)
      x = 0xE240E000;
    x |= (*base) << 16;
    *base = 14; // lr
    y = stuff_const(x, *off & ~maxoff);
    if (y) {
      o(y);
      *off &= maxoff;
      return;
    }
    y = stuff_const(x, (*off + maxoff) & ~maxoff);
    if (y) {
      o(y);
      *sgn = !*sgn;
      *off = ((*off + maxoff) & ~maxoff) - *off;
      return;
    }
    stuff_const_harder(x, *off & ~maxoff);
    *off &= maxoff;
  }
}

static uint32_t mapcc(int cc) {
  switch (cc) {
  case TOK_ULT:
    return 0x30000000; /* CC/LO */
  case TOK_UGE:
    return 0x20000000; /* CS/HS */
  case TOK_EQ:
    return 0x00000000; /* EQ */
  case TOK_NE:
    return 0x10000000; /* NE */
  case TOK_ULE:
    return 0x90000000; /* LS */
  case TOK_UGT:
    return 0x80000000; /* HI */
  case TOK_NSET:
    return 0x40000000; /* MI */
  case TOK_NCLEAR:
    return 0x50000000; /* PL */
  case TOK_LT:
    return 0xB0000000; /* LT */
  case TOK_GE:
    return 0xA0000000; /* GE */
  case TOK_LE:
    return 0xD0000000; /* LE */
  case TOK_GT:
    return 0xC0000000; /* GT */
  }
  tcc_error("unexpected condition code");
  return 0xE0000000; /* AL */
}

static int negcc(int cc) {
  switch (cc) {
  case TOK_ULT:
    return TOK_UGE;
  case TOK_UGE:
    return TOK_ULT;
  case TOK_EQ:
    return TOK_NE;
  case TOK_NE:
    return TOK_EQ;
  case TOK_ULE:
    return TOK_UGT;
  case TOK_UGT:
    return TOK_ULE;
  case TOK_NSET:
    return TOK_NCLEAR;
  case TOK_NCLEAR:
    return TOK_NSET;
  case TOK_LT:
    return TOK_GE;
  case TOK_GE:
    return TOK_LT;
  case TOK_LE:
    return TOK_GT;
  case TOK_GT:
    return TOK_LE;
  }
  tcc_error("unexpected condition code");
  return TOK_NE;
}

/* load 'r' from value 'sv' */
ST_FUNC void load(int r, SValue *sv) {
  int v, ft, fc, fr, tr, sign;
  uint32_t op;
  SValue v1;

  fr = sv->r;
  ft = sv->type.t;
  fc = sv->c.i;

  if (fc >= 0)
    sign = 0;
  else {
    sign = 1;
    fc = -fc;
  }

  v = fr & VT_VALMASK;
  if (fr & VT_LVAL) {
    uint32_t base = 0xB; // fp
    if (v == VT_LLOCAL) {
      v1.type.t = VT_INT32;
      v1.r = VT_LOCAL | VT_LVAL;
      v1.c.i = sv->c.i;
      tr = get_reg_of_cls(RC_INT);
      load(tr, &v1);
      base = tr;
      fc = sign = 0;
      v = VT_LOCAL;
    } else if (v == VT_CONST) {
      v1.type.t = VT_INT32;
      v1.r = fr & ~VT_LVAL;
      v1.c.i = sv->c.i;
      v1.sym = sv->sym;
      tr = get_reg_of_cls(RC_INT);
      load(TREG_LR, &v1);
      base = tr;
      fc = sign = 0;
      v = VT_LOCAL;
    } else if (v < VT_CONST) {
      base = intr(v);
      fc = sign = 0;
      v = VT_LOCAL;
    }
    if (v == VT_LOCAL) {
      if (is_float(ft)) {
        calcaddr(&base, &fc, &sign, 1020, 2);

        op = 0xED100A00; /* flds */
        if (!sign)
          op |= 0x800000;
        if ((ft & VT_TYPE) != VT_FLOAT32)
          op |= 0x100; /* flds -> fldd */
        o(op | (vfpr(r) << 12) | (fc >> 2) | (base << 16));

      } else if (ft == VT_INT8 || is_same_size_int(ft, VT_INT16)) {
        calcaddr(&base, &fc, &sign, 255, 0);
        op = 0xE1500090;
        if (is_same_size_int(ft, VT_INT16))
          op |= 0x20;
        if ((ft & VT_UNSIGNED) == 0)
          op |= 0x40;
        if (!sign)
          op |= 0x800000;
        o(op | (intr(r) << 12) | (base << 16) | ((fc & 0xf0) << 4) |
          (fc & 0xf));
      } else {
        calcaddr(&base, &fc, &sign, 4095, 0);
        op = 0xE5100000;
        if (!sign)
          op |= 0x800000;
        if (ft == (VT_INT8 | VT_UNSIGNED)){
          op |= 0x400000;
        }
        o(op | (intr(r) << 12) | fc | (base << 16));
      }
      return;
    }
  } else {
    if (v == VT_CONST) {
      op = stuff_const(0xE3A00000 | (intr(r) << 12), sv->c.i);
      if (sv->sym || !op) {
        o(0xE59F0000 | (intr(r) << 12));
        o(0xEA000000);
        if (sv->sym)
          greloc(cur_text_section(), sv->sym, ind, R_ARM_ABS32);
        o(sv->c.i);
      } else
        o(op);
      return;
    } else if (v == VT_LOCAL) {
      op = stuff_const(0xE28B0000 | (intr(r) << 12), sv->c.i);
      if (sv->sym || !op) {
        o(0xE59F0000 | (intr(r) << 12));
        o(0xEA000000);
        if (sv->sym) // needed ?
          greloc(cur_text_section(), sv->sym, ind, R_ARM_ABS32);
        o(sv->c.i);
        o(0xE08B0000 | (intr(r) << 12) | intr(r));
      } else
        o(op);
      return;
    } else if (v == VT_CMP) {
      o(mapcc(sv->c.i) | 0x3A00001 | (intr(r) << 12));
      o(mapcc(negcc(sv->c.i)) | 0x3A00000 | (intr(r) << 12));
      return;
    } else if (v == VT_JMP || v == VT_JMPI) {
      int t;
      t = v & 1;
      o(0xE3A00000 | (intr(r) << 12) | t);
      o(0xEA000000);
      gsym(sv->c.i);
      o(0xE3A00000 | (intr(r) << 12) | (t ^ 1));
      return;
    } else if (v < VT_CONST) {
      if (is_float(ft))

        o(0xEEB00A40 | (vfpr(r) << 12) | vfpr(v) | T2CPR(ft)); /* fcpyX */

      else
        o(0xE1A00000 | (intr(r) << 12) | intr(v));
      return;
    }
  }
  tcc_error("load unimplemented!");
}

/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *sv) {
  SValue v1;
  int v, ft, fc, fr, sign;
  uint32_t op;

  fr = sv->r;
  ft = sv->type.t;
  fc = sv->c.i;

  if (fc >= 0)
    sign = 0;
  else {
    sign = 1;
    fc = -fc;
  }

  v = fr & VT_VALMASK;
  if (fr & VT_LVAL || fr == VT_LOCAL) {
    uint32_t base = 0xb; /* fp */
    if (v < VT_CONST) {
      base = intr(v);
      v = VT_LOCAL;
      fc = sign = 0;
    } else if (v == VT_CONST) {
      v1.type.t = ft;
      v1.r = fr & ~VT_LVAL;
      v1.c.i = sv->c.i;
      v1.sym = sv->sym;
      load(TREG_LR, &v1);
      base = 14; /* lr */
      fc = sign = 0;
      v = VT_LOCAL;
    }
    if (v == VT_LOCAL) {
      if (is_float(ft)) {
        calcaddr(&base, &fc, &sign, 1020, 2);

        op = 0xED000A00; /* fsts */
        if (!sign)
          op |= 0x800000;
        if ((ft & VT_TYPE) != VT_FLOAT32)
          op |= 0x100; /* fsts -> fstd */
        o(op | (vfpr(r) << 12) | (fc >> 2) | (base << 16));

        return;
      } else if (is_same_size_int(ft,VT_INT16)) {
        calcaddr(&base, &fc, &sign, 255, 0);
        op = 0xE14000B0;
        if (!sign)
          op |= 0x800000;
        o(op | (intr(r) << 12) | (base << 16) | ((fc & 0xf0) << 4) |
          (fc & 0xf));
      } else {
        calcaddr(&base, &fc, &sign, 4095, 0);
        op = 0xE5000000;
        if (!sign)
          op |= 0x800000;
        if (is_same_size_int(ft,VT_INT8))
          op |= 0x400000;
        o(op | (intr(r) << 12) | fc | (base << 16));
      }
      return;
    }
  }
  tcc_error("store unimplemented");
}

static void gadd_sp(int val) { stuff_const_harder(0xE28DD000, val); }

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp) {
  int r;
  uint32_t x;
  if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
    /* constant case */
    if (vtop->sym) {
      x = encbranch(ind, ind + vtop->c.i, 0);
      if (x) {
        /* relocation case */
        greloc(cur_text_section(), vtop->sym, ind, R_ARM_PC24);
        o(x | (is_jmp ? 0xE0000000 : 0xE1000000));
      } else {
        if (!is_jmp)
          o(0xE28FE004); // add lr,pc,#4
        o(0xE51FF004);   // ldr pc,[pc,#-4]
        greloc(cur_text_section(), vtop->sym, ind, R_ARM_ABS32);
        o(vtop->c.i);
      }
    } else {
      if (!is_jmp)
        o(0xE28FE004); // add lr,pc,#4
      o(0xE51FF004);   // ldr pc,[pc,#-4]
      o(vtop->c.i);
    }
  } else {
    /* otherwise, indirect call */
    r = gen_ldr();
    if (!is_jmp)
      o(0xE1A0E00F);         // mov lr,pc
    o(0xE1A0F000 | intr(r)); // mov pc,r
  }
}

static int unalias_ldbl(int btype) {
  return btype;
}

struct avail_regs {
  signed char avail[3]; /* 3 holes max with only float and double alignments */
  int first_hole;       /* first available hole */
  int last_hole;        /* last available hole (none if equal to first_hole) */
  int first_free_reg;   /* next free register in the sequence, hole excluded */
};

#define AVAIL_REGS_INITIALIZER                                                 \
  (struct avail_regs) { {0, 0, 0}, 0, 0, 0 }

/* Find suitable registers for a VFP Co-Processor Register Candidate (VFP CPRC
   param) according to the rules described in the procedure call standard for
   the ARM architecture (AAPCS). If found, the registers are assigned to this
   VFP CPRC parameter. Registers are allocated in sequence unless a hole exists
   and the parameter is a single float.

   avregs: opaque structure to keep track of available VFP co-processor regs
   align: alignment constraints for the param, as returned by type_size()
   size: size of the parameter, as returned by type_size() */
ST_FUNC int assign_vfpreg(struct avail_regs *avregs, int align, int size) {
  int first_reg = 0;

  if (avregs->first_free_reg == -1)
    return -1;
  if (align >> 3) { /* double alignment */
    first_reg = avregs->first_free_reg;
    /* alignment constraint not respected so use next reg and record hole */
    if (first_reg & 1)
      avregs->avail[avregs->last_hole++] = first_reg++;
  } else { /* no special alignment (float or array of float) */
    /* if single float and a hole is available, assign the param to it */
    if (size == 4 && avregs->first_hole != avregs->last_hole)
      return avregs->avail[avregs->first_hole++];
    else
      first_reg = avregs->first_free_reg;
  }
  if (first_reg + size / 4 <= 16) {
    avregs->first_free_reg = first_reg + size / 4;
    return first_reg;
  }
  avregs->first_free_reg = -1;
  return -1;
}


/* Parameters are classified according to how they are copied to their final
   destination for the function call. Because the copying is performed class
   after class according to the order in the union below, it is important that
   some constraints about the order of the members of this union are respected:
   - CORE_STRUCT_CLASS must come after STACK_CLASS;
   - CORE_CLASS must come after STACK_CLASS, CORE_STRUCT_CLASS and
     VFP_STRUCT_CLASS;
   - VFP_STRUCT_CLASS must come after VFP_CLASS.
   See the comment for the main loop in copy_params() for the reason. */
enum reg_class {
  STACK_CLASS = 0,
  CORE_STRUCT_CLASS,
  VFP_CLASS,
  VFP_STRUCT_CLASS,
  CORE_CLASS,
  NB_CLASSES
};

struct param_plan {
  int start;    /* first reg or addr used depending on the class */
  int end;      /* last reg used or next free addr depending on the class */
  SValue *sval; /* pointer to SValue on the value stack */
  struct param_plan *prev; /*  previous element in this class */
};

struct plan {
  struct param_plan *pplans;               /* array of all the param plans */
  struct param_plan *clsplans[NB_CLASSES]; /* per class lists of param plans */
  int nb_plans;
};

static void add_param_plan(struct plan* plan, int cls, int start, int end, SValue *v)
{
    struct param_plan *p = &plan->pplans[plan->nb_plans++];
    p->prev = plan->clsplans[cls];
    plan->clsplans[cls] = p;
    p->start = start, p->end = end, p->sval = v;
}

/* Assign parameters to registers and stack with alignment according to the
   rules in the procedure call standard for the ARM architecture (AAPCS).
   The overall assignment is recorded in an array of per parameter structures
   called parameter plans. The parameter plans are also further organized in a
   number of linked lists, one per class of parameter (see the comment for the
   definition of union reg_class).

   nb_args: number of parameters of the function for which a call is generated
   float_abi: float ABI in use for this function call
   plan: the structure where the overall assignment is recorded
   todo: a bitmap that record which core registers hold a parameter

   Returns the amount of stack space needed for parameter passing
*/
static int assign_regs(int nb_args, int float_abi, struct plan *plan, int *todo)
{
  int i, size;
  uint32_t align;
  int ncrn /* next core register number */, nsaa /* next stacked argument address*/;
  struct avail_regs avregs = {{0}};

  ncrn = nsaa = 0;
  *todo = 0;

  for(i = nb_args; i-- ;) {
    int j, start_vfpreg = 0;
    CType type = vtop[-i].type;
    size = size_align_of_type(type.t, &align);
    size = (size + 3) & ~3;
    align = (align + 3) & ~3;
    switch(vtop[-i].type.t & VT_TYPE) {
      case VT_FLOAT32:
      case VT_FLOAT64:
      if (float_abi == ARM_HARD_FLOAT) {

        if (is_float(vtop[-i].type.t)) {
          int end_vfpreg;

          start_vfpreg = assign_vfpreg(&avregs, align, size);
          end_vfpreg = start_vfpreg + ((size - 1) >> 2);
          if (start_vfpreg >= 0) {
            add_param_plan(plan, VFP_CLASS,
                start_vfpreg, end_vfpreg, &vtop[-i]);
            continue;
          } else
            break;
        }
      }
      ncrn = (ncrn + (align-1)/4) & ~((align/4) - 1);
      if (ncrn + size/4 <= 4 || (ncrn < 4 && start_vfpreg != -1)) {
        /* The parameter is allocated both in core register and on stack. As
	 * such, it can be of either class: it would either be the last of
	 * CORE_STRUCT_CLASS or the first of STACK_CLASS. */
        for (j = ncrn; j < 4 && j < ncrn + size / 4; j++)
          *todo|=(1<<j);
        add_param_plan(plan, CORE_STRUCT_CLASS, ncrn, j, &vtop[-i]);
        ncrn += size/4;
        if (ncrn > 4)
          nsaa = (ncrn - 4) * 4;
      } else {
        ncrn = 4;
        break;
      }
      continue;
      default:
      if (ncrn < 4) {
        int is_long = is_same_size_int(vtop[-i].type.t & VT_TYPE, VT_INT64);

        if (is_long) {
          ncrn = (ncrn + 1) & -2;
          if (ncrn == 4)
            break;
        }
        add_param_plan(plan, CORE_CLASS, ncrn, ncrn + is_long, &vtop[-i]);
        ncrn += 1 + is_long;
        continue;
      }
    }
    nsaa = (nsaa + (align - 1)) & ~(align - 1);
    add_param_plan(plan, STACK_CLASS, nsaa, nsaa + size, &vtop[-i]);
    nsaa += size; /* size already rounded up before */
  }
  return nsaa;
}


/* Copy parameters to their final destination (core reg, VFP reg or stack) for
   function call.

   nb_args: number of parameters the function take
   plan: the overall assignment plan for parameters
   todo: a bitmap indicating what core reg will hold a parameter

   Returns the number of SValue added by this function on the value stack */
static int copy_params(int nb_args, struct plan *plan, int todo) {
  int size, align, r, i, nb_extra_sval = 0;
  struct param_plan *pplan;
  int pass = 0;

  /* Several constraints require parameters to be copied in a specific order:
      - structures are copied to the stack before being loaded in a reg;
      - floats loaded to an odd numbered VFP reg are first copied to the
        preceding even numbered VFP reg and then moved to the next VFP reg.

      It is thus important that:
      - structures assigned to core regs must be copied after parameters
        assigned to the stack but before structures assigned to VFP regs because
        a structure can lie partly in core registers and partly on the stack;
      - parameters assigned to the stack and all structures be copied before
        parameters assigned to a core reg since copying a parameter to the stack
        require using a core reg;
      - parameters assigned to VFP regs be copied before structures assigned to
        VFP regs as the copy might use an even numbered VFP reg that already
        holds part of a structure. */
again:
  for (i = 0; i < NB_CLASSES; i++) {
    for (pplan = plan->clsplans[i]; pplan; pplan = pplan->prev) {

      if (pass && (i != CORE_CLASS || pplan->sval->r < VT_CONST))
        continue;

      vpushv(pplan->sval);
      pplan->sval->r = pplan->sval->r2 = VT_CONST; /* disable entry */
      switch (i) {
      case STACK_CLASS:
      case CORE_STRUCT_CLASS:
      case VFP_STRUCT_CLASS:
        if (is_float(pplan->sval->type.t)) {

          r = vfpr(gen_ldr()) << 12;
          if ((pplan->sval->type.t & VT_TYPE) == VT_FLOAT32)
            size = 4;
          else if ((pplan->sval->type.t & VT_TYPE) == VT_FLOAT64){
            size = 8;
            r |= 0x101; /* vpush.32 -> vpush.64 */
          }
          o(0xED2D0A01 + r); /* vpush */

        } else {
          /* simple type (currently always same size) */
          /* XXX: implicit cast ? */
          size = 4;
          if ((pplan->sval->type.t & VT_TYPE) == VT_INT64) {
            gen_lexpand();
            size = 8;
            r = gen_ldr();
            o(0xE52D0004 | (intr(r) << 12)); /* push r */
            vpop(1);
          }
          r = gen_ldr();
          o(0xE52D0004 | (intr(r) << 12)); /* push r */
        }
        if (i == STACK_CLASS && pplan->prev)
          gadd_sp(pplan->prev->end - pplan->start); /* Add padding if any */
        break;

      case VFP_CLASS:
        gen_load_reg(TREG_F0 + (pplan->start >> 1));
        if (pplan->start & 1) { /* Must be in upper part of double register */
          o(0xEEF00A40 | ((pplan->start >> 1) << 12) |
            (pplan->start >> 1)); /* vmov.f32 s(n+1), sn */
          vtop->r = VT_CONST; /* avoid being saved on stack by gv for next float */
        }
        break;

      case CORE_CLASS:
        if ((pplan->sval->type.t & VT_TYPE) == VT_INT64) {
          gen_lexpand();
          gen_load_reg(pplan->end);
          pplan->sval->r2 = vtop->r;
          vtop--;
        }
        gen_load_reg(pplan->start);
        /* Mark register as used so that gcall_or_jmp use another one
             (regs >=4 are free as never used to pass parameters) */
        pplan->sval->r = vtop->r;
        break;
      }
      vtop--;
    }
  }

  /* second pass to restore registers that were saved on stack by accident.(reconfirm?) */
  if (++pass < 2)
    goto again;

  /* Manually free remaining registers since next parameters are loaded
   * manually, without the help of gen_ldr(int). */
  save_rc_upstack(RC_CALLER_SAVED,nb_args);

  if (todo) {
    o(0xE8BD0000 | todo); /* pop {todo} */
    for (pplan = plan->clsplans[CORE_STRUCT_CLASS]; pplan;
         pplan = pplan->prev) {
      int r;
      pplan->sval->r = pplan->start;
      /* An SValue can only pin 2 registers at best (r and r2) but a structure
         can occupy more than 2 registers. Thus, we need to push on the value
         stack some fake parameter to have on SValue for each registers used
         by a structure (r2 is not used). */
      for (r = pplan->start + 1; r <= pplan->end; r++) {
        if (todo & (1 << r)) {
          nb_extra_sval++;
          vpushi(0);
          vtop->r = r;
        }
      }
    }
  }
  return nb_extra_sval;
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
ST_FUNC void gfunc_call(int nb_args,CType *ret_type) {
  int r, args_size,btype;
  uint32_t align;
  int def_float_abi = abi_config.float_abi;
  int todo;
  struct plan plan;

  /* cannot let cpu flags if other instruction are generated. Also avoid leaving
     VT_JMP anywhere except on the top of the stack because it would complicate
     the code generator. */
  r = vtop->r & VT_VALMASK;
  if (r == VT_CMP || (r & ~1) == VT_JMP)
    gen_ldr();

  memset(&plan, 0, sizeof plan);
  if (nb_args)
    plan.pplans = tcc_malloc(nb_args * sizeof(*plan.pplans));

  args_size = assign_regs(nb_args, abi_config.float_abi, &plan, &todo);


  if (args_size & 7) { /* Stack must be 8 byte aligned at fct call for EABI */
    args_size = (args_size + 7) & ~7;
    o(0xE24DD004); /* sub sp, sp, #4 */
  }

  if (abi_config.eabi && (args_size & 7)) { /* Stack must be 8 byte aligned at fct call for EABI */
    args_size = (args_size + 7) & ~7;
    o(0xE24DD004); /* sub sp, sp, #4 */
  }

  nb_args += copy_params(nb_args, &plan, todo);
  tcc_free(plan.pplans);

  /* Move fct SValue on top as required by gcall_or_jmp */
  vrotb(nb_args + 1);
  gcall_or_jmp(0);
  if (args_size)
      gadd_sp(args_size); /* pop all parameters passed on the stack */
  if(abi_config.eabi){
    if(abi_config.float_abi == ARM_SOFTFP_FLOAT && is_float(ret_type->t)) {
      if((ret_type->t & VT_TYPE) == VT_FLOAT32) {
        o(0xEE000A10); /*vmov s0, r0 */
      } else {
        o(0xEE000B10); /* vmov.32 d0[0], r0 */
        o(0xEE201B10); /* vmov.32 d0[1], r1 */
      }
    }
  }
  vpop(nb_args + 1); /* Pop all params and fct address from value stack */
  leaffunc = 0; /* we are calling a function, so we aren't in a leaf function */
  abi_config.float_abi = def_float_abi;

  vpushi(0);
  vtop->type=*ret_type;
  btype=ret_type->t;
  if(is_integer(btype)&&size_align_of_type(btype,&align)<=4){
      vtop->r=TREG_R0;
      vtop->r2=VT_CONST;
  }else if(is_same_size_int(btype,VT_INT64)){
      vtop->r=TREG_R0;
      vtop->r=TREG_R1;
  }else if(btype==VT_FLOAT32 || btype==VT_FLOAT64){
      vtop->r=TREG_F0;
      vtop->r2=VT_CONST;
  }else{
      tcc_error("unsuport value type.");
  }
}

/* generate function prolog of type 't' */
ST_FUNC void gfunc_prolog()
{
  SValue *sv;
  int n, nf, size, rs, struct_ret = 0;
  uint32_t align;
  int addr, pn, sn; /* pn=core, sn=stack */
  CType ret_type;

  struct avail_regs avregs = {{0}};

  n = nf = 0;

  for(sv=vstack; sv<=vtop && (n < 4 || nf < 16); sv++) {
    size = size_align_of_type(sv->type.t, &align);
    if (abi_config.eabi && abi_config.float_abi == ARM_HARD_FLOAT  &&
        is_float(sv->type.t) ) {
      int tmpnf = assign_vfpreg(&avregs, align, size);
      tmpnf += (size + 3) / 4;
      nf = (tmpnf > nf) ? tmpnf : nf;
    } else{
      if (n < 4)
        n += (size + 3) / 4;
    }
    
  }
  o(0xE1A0C00D); /* mov ip,sp */
  if (n) {
    if(n>4)
      n=4;
  if(abi_config.eabi) n=(n+1)&-2;
    o(0xE92D0000|((1<<n)-1)); /* save r0-r4 on stack if needed */
  }
  if (nf) {
    if (nf>16)
      nf=16;
    nf=(nf+1)&-2; /* nf => HARDFLOAT => EABI */
    o(0xED2D0A00|nf); /* save s0-s15 on stack if needed */
  }
  o(0xE92D5800); /* save fp, ip, lr */
  o(0xE1A0B00D); /* mov fp, sp */
  func_sub_sp_offset = ind;
  o(0xE1A00000); /* nop, leave space for stack adjustment in epilog */

  if (abi_config.eabi && abi_config.float_abi == ARM_HARD_FLOAT) {
    memset(&avregs, 0, sizeof avregs);
  }
  
  pn = struct_ret, sn = 0;
  for(sv=vstack; sv<=vtop ; sv++) {
    CType *type;
    type = &sv->type;
    size = size_align_of_type(type->t, &align);
    size = (size + 3) >> 2;
    align = (align + 3) & ~3;
    if (abi_config.eabi && abi_config.float_abi == ARM_HARD_FLOAT  && is_float(sv->type.t)) {
      int fpn = assign_vfpreg(&avregs, align, size << 2);
      if (fpn >= 0)
        addr = fpn * 4;
      else
        goto from_stack;
    } else if (pn < 4) {
      if(abi_config.eabi)pn = (pn + (align-1)/4) & -(align/4);
      addr = (nf + pn) * 4;
      pn += size;
      if (!sn && pn > 4)
        sn = (pn - 4);
    } else {

from_stack:
      if(abi_config.eabi)sn = (sn + (align-1)/4) & -(align/4);
      addr = (n + nf + sn) * 4;
      sn += size;
    }
    sv->r= VT_LOCAL | VT_LVAL;
    sv->c.i = addr+12;
  }
  last_itod_magic=0;
  leaffunc = 1;
  loc = 0;

  vpushi(0);
  vtop->r=TREG_LR;
  vtop->r2=VT_CONST;
}



/* generate function epilog */
ST_FUNC void gfunc_epilog(void)
{
  uint32_t x;
  int diff,btype;
  uint32_t align;
  int rettype=vtop->type.t;

  if(size_align_of_type(btype,&align)<=4){
        load(TREG_R0,vtop);
    }else if((btype==VT_INT64) || (btype==(VT_INT64|VT_UNSIGNED))){
        gen_lexpand();
        gen_load_reg(TREG_R0);
        vswap();
        gen_load_reg(TREG_R1);
        vpop(1);
    }else if((btype==VT_FLOAT32) || (btype==VT_FLOAT64)){
        load(TREG_F0,vtop);
    }else if(btype==VT_VOID){
        /* do nothing */
    }
    vpop(1);
  /* Copy float return value to core register if base standard is used and
     float computation is made with VFP */
  if (abi_config.eabi && (abi_config.float_abi == ARM_SOFTFP_FLOAT) && is_float(rettype)) {
    if((rettype & VT_TYPE) == VT_FLOAT32)
      o(0xEE100A10); /* fmrs r0, s0 */
    else {
      o(0xEE100B10); /* fmrdl r0, d0 */
      o(0xEE301B10); /* fmrdh r1, d0 */
    }
  }
  o(0xE89BA800); /* restore fp, sp, pc */
  diff = (-loc + 3) & -4;

  if(abi_config.eabi && !leaffunc)
    diff = ((diff + 11) & -8) - 4;

  if(diff > 0) {
    x=stuff_const(0xE24BD000, diff); /* sub sp,fp,# */
    if(x)
      *(uint32_t *)(cur_text_section()->data + func_sub_sp_offset) = x;
    else {
      int addr;
      addr=ind;
      o(0xE59FC004); /* ldr ip,[pc+4] */
      o(0xE04BD00C); /* sub sp,fp,ip  */
      o(0xE1A0F00E); /* mov pc,lr */
      o(diff);
      *(uint32_t *)(cur_text_section()->data + func_sub_sp_offset) = 0xE1000000|encbranch(func_sub_sp_offset,addr,1);
    }
  }
  vpop(1);
}


ST_FUNC void gen_fill_nops(int bytes) {
  if ((bytes & 3))
    tcc_error("alignment of code section not multiple of 4");
  while (bytes > 0) {
    o(0xE1A00000);
    bytes -= 4;
  }
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t) {
  int r;
  r = ind;
  o(0xE0000000 | encbranch(r, t, 1));
  return r;
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a) { gjmp(a); }

/* generate a test. set 'inv' to invert test. Stack entry is popped */
ST_FUNC int gtst(int inv, int t) {
  int v, r;
  uint32_t op;

  v = vtop->r & VT_VALMASK;
  r = ind;

  if (v == VT_CMP) {
    op = mapcc(inv ? negcc(vtop->c.i) : vtop->c.i);
    op |= encbranch(r, t, 1);
    o(op);
    t = r;
  } else if (v == VT_JMP || v == VT_JMPI) {
    if ((v & 1) == inv) {
      if (!vtop->c.i)
        vtop->c.i = t;
      else {
        uint32_t *x;
        int p, lp;
        if (t) {
          p = vtop->c.i;
          do {
            p = decbranch(lp = p);
          } while (p);
          x = (uint32_t *)(cur_text_section()->data + lp);
          *x &= 0xff000000;
          *x |= encbranch(lp, t, 1);
        }
        t = vtop->c.i;
      }
    } else {
      t = gjmp(t);
      gsym(vtop->c.i);
    }
  }
  vtop--;
  return t;
}

/* generate an integer binary operation */
ST_FUNC void gen_opi(int op) {
  int c, func = 0;
  uint32_t opc = 0, r, fr;

  c = 0;
  switch (op) {
  case TOK_ADD:
    opc = 0x8;
    c = 1;
    break;
  case TOK_ADDC1: /* add with carry generation */
    opc = 0x9;
    c = 1;
    break;
  case TOK_SUB:
    opc = 0x4;
    c = 1;
    break;
  case TOK_SUBC1: /* sub with carry generation */
    opc = 0x5;
    c = 1;
    break;
  case TOK_ADDC2: /* add with carry use */
    opc = 0xA;
    c = 1;
    break;
  case TOK_SUBC2: /* sub with carry use */
    opc = 0xC;
    c = 1;
    break;
  case TOK_AND:
    opc = 0x0;
    c = 1;
    break;
  case TOK_XOR:
    opc = 0x2;
    c = 1;
    break;
  case TOK_OR:
    opc = 0x18;
    c = 1;
    break;
  case TOK_MULL:
    gen_ldr();
    vswap();
    gen_ldr();
    vswap();
    r = vtop[-1].r;
    fr = vtop[0].r;
    vpop(1);
    o(0xE0000090 | (intr(r) << 16) | (intr(r) << 8) | intr(fr));
    return;
  case TOK_SHL:
    opc = 0;
    c = 2;
    break;
  case TOK_SHR:
    opc = 1;
    c = 2;
    break;
  case TOK_SAR:
    opc = 2;
    c = 2;
    break;
  case TOK_UDIV:
  case TOK_DIV:
  case TOK_UMOD:
    tcc_error(TCC_ERROR_UNIMPLEMENTED);
    return;
  case TOK_UMULL:
    gen_ldr();
    vswap();
    gen_ldr();
    r = intr(vtop[-1].r2 = vtop->r);
    c = vtop[-1].r;
    vpop(1);
    o(0xE0800090 | (r << 16) | (intr(vtop->r) << 12) | (intr(c) << 8) | intr(r));
    vtop->type.t=VT_INT64;
    return;
  default:
    if(op>=TOK_ULT&&op<=TOK_GT){
       opc = 0x15;
      c = 1;
      break;
    }else{
      tcc_error(TCC_ERROR_UNIMPLEMENTED);
      return;
    }
  }
  
  switch (c) {
  case 1:
    if (((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) && (vtop->sym==NULL)) {
      if (opc == 4 || opc == 5 || opc == 0xc) {
        vswap();
        opc |= 2; // sub -> rsb
      }
    }
    if ((vtop->r & VT_VALMASK) == VT_CMP ||
        (vtop->r & (VT_VALMASK & ~1)) == VT_JMP)
      gen_ldr();
    vswap();
    c = intr(gen_ldr());
    vswap();
    opc = 0xE0000000 | (opc << 20) | (c << 16);
    if (((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) && (vtop->sym==NULL)) {
      uint32_t x;
      x = stuff_const(opc | 0x2000000, vtop->c.i);
      if (x) {
        r = vtop[-1].r;
        o(x | (r << 12));
        goto done;
      }
    }
    fr = intr(gen_ldr());
    
    r = intr(vtop[-1].r);
    o(opc | (r << 12) | fr);
  done:
    vpop(1);
    if (op >= TOK_ULT && op <= TOK_GT) {
      vtop->r = VT_CMP;
      vtop->c.i = op;
    }
    break;
  case 2:
    opc = 0xE1A00000 | (opc << 5);
    if ((vtop->r & VT_VALMASK) == VT_CMP ||
        (vtop->r & (VT_VALMASK & ~1)) == VT_JMP)
      gen_ldr();
    vswap();
    r = intr(gen_ldr());
    vswap();
    opc |= r;
    if (((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) && vtop->sym == NULL) {
      fr = intr(vtop[-1].r);
      c = vtop->c.i & 0x1f;
      o(opc | (c << 7) | (fr << 12));
    } else {
      fr = intr(gen_ldr());
      c = intr(vtop[-1].r);
      o(opc | (c << 12) | (fr << 8) | 0x10);
    }
    vtop--;
    break;
  case 3:
    tcc_error(TCC_ERROR_UNIMPLEMENTED);
    break;
  default:
    tcc_error(TCC_ERROR_UNIMPLEMENTED);
  }
}


static int is_zero(int i) {
  if ((vtop[i].r & (VT_VALMASK | VT_LVAL)) != VT_CONST || vtop[i].sym!=NULL)
    return 0;
  if (vtop[i].type.t == VT_FLOAT32)
    return (vtop[i].c.f == 0.f);
  else if (vtop[i].type.t == VT_FLOAT64)
    return (vtop[i].c.d == 0.0);
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
 *    two operands are guaranteed to have the same floating point type */
ST_FUNC void gen_opf(int op) {
  uint32_t x;
  int fneg = 0, r;
  x = 0xEE000A00 | T2CPR(vtop->type.t);
  switch (op) {
  case TOK_ADD:
    if (is_zero(-1))
      vswap();
    if (is_zero(0)) {
      vtop--;
      return;
    }
    x |= 0x300000;
    break;
  case TOK_SUB:
    x |= 0x300040;
    if (is_zero(0)) {
      vtop--;
      return;
    }
    if (is_zero(-1)) {
      x |= 0x810000; /* fsubX -> fnegX */
      vswap();
      vtop--;
      fneg = 1;
    }
    break;
  case TOK_MULL:
    x |= 0x200000;
    break;
  case TOK_DIV:
    x |= 0x800000;
    break;
  default:
    if (op < TOK_ULT || op > TOK_GT) {
      tcc_error(TCC_ERROR_UNIMPLEMENTED);
      return;
    }
    if (is_zero(-1)) {
      vswap();
      switch (op) {
      case TOK_LT:
        op = TOK_GT;
        break;
      case TOK_GE:
        op = TOK_ULE;
        break;
      case TOK_LE:
        op = TOK_GE;
        break;
      case TOK_GT:
        op = TOK_ULT;
        break;
      }
    }
    x |= 0xB40040; /* fcmpX */
    if (op != TOK_EQ && op != TOK_NE)
      x |= 0x80; /* fcmpX -> fcmpeX */
    if (is_zero(0)) {
      vtop--;
      o(x | 0x10000 | (vfpr(gen_ldr()) << 12)); /* fcmp(e)X -> fcmp(e)zX */
    } else {
      x |= vfpr(gen_ldr());
      vswap();
      o(x | (vfpr(gen_ldr()) << 12));
      vtop--;
    }
    o(0xEEF1FA10); /* fmstat */

    switch (op) {
    case TOK_LE:
      op = TOK_ULE;
      break;
    case TOK_LT:
      op = TOK_ULT;
      break;
    case TOK_UGE:
      op = TOK_GE;
      break;
    case TOK_UGT:
      op = TOK_GT;
      break;
    }

    vtop->r = VT_CMP;
    vtop->c.i = op;
    return;
  }
  r = gen_ldr();
  x |= vfpr(r);

  if (!fneg) {
    int r2;
    vswap();
    r2 = gen_ldr();
    x |= vfpr(r2) << 16;

  }

  if (!fneg)
    vtop--;
  o(x | (vfpr(vtop->r) << 12));
}


/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
ST_FUNC void gen_cvt_itof(int t) {
  uint32_t r, r2;
  int bt,align;
  bt = vtop->type.t & VT_TYPE;
  if (size_align_of_type(bt,&align)<=4) {
#ifndef TCC_ARM_VFP
    uint32_t dsize = 0;
#endif
    r = intr(gen_ldr());

    r2 = vfpr(vtop->r = get_reg_of_cls(RC_FLOAT));
    o(0xEE000A10 | (r << 12) | (r2 << 16)); /* fmsr */
    r2 |= r2 << 12;
    if (!(vtop->type.t & VT_UNSIGNED))
      r2 |= 0x80;                  /* fuitoX -> fsituX */
    o(0xEEB80A40 | r2 | T2CPR(t)); /* fYitoX*/
    return;
  } else if (bt == VT_INT64) {
    tcc_error(TCC_ERROR_UNIMPLEMENTED);
  }
  tcc_error(TCC_ERROR_UNIMPLEMENTED);
}

/* convert fp to int 't' type */
ST_FUNC void gen_cvt_ftoi(int t) {
  uint32_t r, r2;
  int u, func = 0;
  u = t & VT_UNSIGNED;
  t &= VT_TYPE;
  r2 = vtop->type.t & VT_TYPE;
  if (is_same_size_int(t,VT_INT32)) {

    r = vfpr(gen_ldr());
    u = u ? 0 : 0x10000;
    o(0xEEBC0AC0 | (r << 12) | r | T2CPR(r2) | u); /* ftoXizY */
    r2 = intr(vtop->r = get_reg_of_cls(RC_INT));
    o(0xEE100A10 | (r << 16) | (r2 << 12));
    return;

  } else if (t == VT_INT64) { // unsigned handled in gen_cvt_ftoi1
    tcc_error(TCC_ERROR_UNIMPLEMENTED); 
  }
  tcc_error(TCC_ERROR_UNIMPLEMENTED);
}


/* computed goto support */
ST_FUNC void ggoto(void) {
  gcall_or_jmp(1);
  vtop--;
}

