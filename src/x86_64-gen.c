/*
 *  x86-64 code generator for TCC
 *
 *  Copyright (c) 2008 Shinichiro Hamaji
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

#include "x86_64-gen.h"
#include "tccutils.h"
#include "xxx-gen.h"

#define assert(a)
static struct reg_attr regs_attr[NB_REGS] = {
    /* rax */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* rcx */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    /* rdx */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
#ifdef TCC_WINDOWS_ABI /* On Windows, RSI RDI are callee saved */
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
#else
    {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
#endif
    /* r8 */ {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    {RC_INT | RC_CALLEE_SAVED, REGISTER_SIZE},
    /* xmm0 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    /* xmm1 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    /* xmm2 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    /* xmm3 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    /* xmm4 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    /* xmm5 */ {RC_FLOAT | RC_CALLER_SAVED, 128},
    {RC_FLOAT | RC_CALLEE_SAVED, 128},
    {RC_FLOAT | RC_CALLEE_SAVED, 128},
    /* st0 */ {RC_STACK_MODEL | RC_FLOAT | RC_CALLER_SAVED, 80},
};

static unsigned long func_sub_sp_offset;
static int func_ret_sub;

ST_FUNC void arch_gen_init() {}

ST_FUNC void arch_gen_deinit() {}

ST_FUNC struct reg_attr *get_reg_attr(int r) { return &regs_attr[r]; }

static int gvreg(int reg, int rc) {
  if (reg == VT_CONST) {
    reg = get_reg_of_cls(rc);
  } else {
    save_reg_upstack(reg, 0);
  }
  load(reg, vtop);
  vtop->r = reg;
  vtop->r2 = VT_CONST;
  vtop->sym = NULL;
  return reg;
}

/* XXX: make it faster ? */
ST_FUNC void g(int c) {
  int ind1;
  ind1 = ind + 1;
  if (ind1 > cur_text_section()->data_allocated)
    section_realloc(cur_text_section(), ind1);
  cur_text_section()->data[ind] = c;
  ind = ind1;
}

ST_FUNC void o(unsigned int c) {
  while (c) {
    g(c);
    c = c >> 8;
  }
}

ST_FUNC void gen_le16(int v) {
  g(v);
  g(v >> 8);
}

ST_FUNC void gen_le32(int c) {
  g(c);
  g(c >> 8);
  g(c >> 16);
  g(c >> 24);
}

ST_FUNC void gen_le64(int64_t c) {
  g(c);
  g(c >> 8);
  g(c >> 16);
  g(c >> 24);
  g(c >> 32);
  g(c >> 40);
  g(c >> 48);
  g(c >> 56);
}

static void orex(int ll, int r, int r2, int b) {
  if ((r & VT_VALMASK) >= VT_CONST)
    r = 0;
  if ((r2 & VT_VALMASK) >= VT_CONST)
    r2 = 0;
  if (ll || REX_BASE(r) || REX_BASE(r2))
    o(0x40 | REX_BASE(r) | (REX_BASE(r2) << 2) | (ll << 3));
  o(b);
}

/* output a symbol and patch all calls to it */
ST_FUNC void gsym_addr(int t, int a) {
  while (t) {
    unsigned char *ptr = cur_text_section()->data + t;
    uint32_t n = read32le(ptr); /* next value */
    write32le(ptr, a - t - 4);
    t = n;
  }
}

ST_FUNC void gsym(int t) { gsym_addr(t, ind); }

static int is64_type(int t) {
  int align;
  return is_same_size_int(t, VT_INT64);
}

/* instruction + 4 bytes data. Return the address of the data */
static int oad(int c, int s) {
  int t;
  o(c);
  t = ind;
  gen_le32(s);
  return t;
}

/* generate jmp to a label */
#define gjmp2(instr, lbl) oad(instr, lbl)

ST_FUNC void gen_addr32(int r, Sym *sym, int c) {
  if (sym)
    greloca(cur_text_section(), sym, ind, R_X86_64_32S, c), c = 0;
  gen_le32(c);
}

/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addr64(int r, Sym *sym, int64_t c) {
  if (sym)
    greloca(cur_text_section(), sym, ind, R_X86_64_64, c), c = 0;
  gen_le64(c);
}

/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addrpc32(int r, Sym *sym, int c) {
  if (sym)
    greloca(cur_text_section(), sym, ind, R_X86_64_PC32, c - 4), c = 4;
  gen_le32(c - 4);
}

static void gen_modrm_impl(int op_reg, int r, Sym *sym, int c, int is_got) {
  op_reg = REG_VALUE(op_reg) << 3;
  if ((r & VT_VALMASK) == VT_CONST) {
    /* constant memory reference */
    if (!sym) {
      /* Absolute memory reference */
      o(0x04 | op_reg); /* [sib] | destreg */
      oad(0x25, c);     /* disp32 */
    } else {
      o(0x05 | op_reg); /* (%rip)+disp32 | destreg */
      gen_addrpc32(r, sym, c);
    }
  } else if ((r & VT_VALMASK) == VT_LOCAL) {
    /* currently, we use only ebp as base */
    if (c == (char)c) {
      /* short reference */
      o(0x45 | op_reg);
      g(c);
    } else {
      oad(0x85 | op_reg, c);
    }
  } else if ((r & VT_VALMASK) >= VT_CONST) {
    if (c) {
      g(0x80 | op_reg | REG_VALUE(r));
      gen_le32(c);
    } else {
      g(0x00 | op_reg | REG_VALUE(r));
    }
  } else {
    g(0x00 | op_reg | REG_VALUE(r));
  }
}

/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm(int op_reg, int r, Sym *sym, int c) {
  gen_modrm_impl(op_reg, r, sym, c, 0);
}

/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm64(int opcode, int op_reg, int r, Sym *sym, int c) {
  orex(1, r, op_reg, opcode);
  gen_modrm_impl(op_reg, r, sym, c, 0);
}

/* load 'r' from value 'sv' */
ST_FUNC void load(int r, SValue *sv) {
  int v, t, ft, fc, fr;
  SValue v1;

  fr = sv->r;
  ft = sv->type.t;
  fc = sv->c.i;
  if (fc != sv->c.i && (sv->sym))
    tcc_error("64 bit addend in load");

  v = fr & VT_VALMASK;
  if (fr & VT_LVAL) {
    int b, ll;
    if (v == VT_LLOCAL) {
      v1.type.t = VT_INT64;
      v1.r = VT_LOCAL | VT_LVAL;
      v1.c.i = fc;
      fr = r;
      if (!(get_reg_attr(fr)->c & RC_INT))
        fr = gen_ldr();
      load(fr, &v1);
    }
    if (fc != sv->c.i) {
      /* If the addends doesn't fit into a 32bit signed
         we must use a 64bit move.  We've checked above
         that this doesn't have a sym associated.  */
      v1.type.t = VT_INT64;
      v1.r = VT_CONST;
      v1.c.i = sv->c.i;
      fr = r;
      if (!(get_reg_attr(fr)->c & RC_INT))
        fr = get_reg_of_cls(RC_INT);
      load(fr, &v1);
      fc = 0;
    }
    ll = 0;

    if ((ft & VT_TYPE) == VT_FLOAT32) {
      b = 0x6e0f66;
      r = REG_VALUE(r); /* movd */
    } else if ((ft & VT_TYPE) == VT_FLOAT64) {
      b = 0x7e0ff3; /* movq */
      r = REG_VALUE(r);
    } else if ((ft & VT_TYPE) == VT_FLOAT128) {
      b = 0xdb, r = 5; /* fldt */
    } else if ((ft & VT_TYPE) == VT_INT8) {
      b = 0xbe0f; /* movsbl */
    } else if ((ft & VT_TYPE) == (VT_INT8 | VT_UNSIGNED)) {
      b = 0xb60f; /* movzbl */
    } else if ((ft & VT_TYPE) == VT_INT16) {
      b = 0xbf0f; /* movswl */
    } else if ((ft & VT_TYPE) == (VT_INT16 | VT_UNSIGNED)) {
      b = 0xb70f; /* movzwl */
    } else {
      ll = is64_type(ft);
      b = 0x8b;
    }
    if (ll) {
      gen_modrm64(b, r, fr, sv->sym, fc);
    } else {
      orex(ll, fr, r, b);
      gen_modrm(r, fr, sv->sym, fc);
    }
  } else {
    if (v == VT_CONST) {
      if (sv->sym) {
        orex(1, 0, r, 0x8d);
        o(0x05 + REG_VALUE(r) * 8); /* lea xx(%rip), r */
        gen_addrpc32(fr, sv->sym, fc);

      } else if (is64_type(ft)) {
        orex(1, r, 0, 0xb8 + REG_VALUE(r)); /* mov $xx, r */
        gen_le64(sv->c.i);
      } else {
        orex(0, r, 0, 0xb8 + REG_VALUE(r)); /* mov $xx, r */
        gen_le32(fc);
      }
    } else if (v == VT_LOCAL) {
      orex(1, 0, r, 0x8d); /* lea xxx(%ebp), r */
      gen_modrm(r, VT_LOCAL, sv->sym, fc);
    } else if (v == VT_CMP) {
      orex(0, r, 0, 0);
      if ((fc & ~0x100) != TOK_NE)
        oad(0xb8 + REG_VALUE(r), 0); /* mov $0, r */
      else
        oad(0xb8 + REG_VALUE(r), 1); /* mov $1, r */
      if (fc & 0x100) {
        /* This was a float compare.  If the parity bit is
           set the result was unordered, meaning false for everything
           except TOK_NE, and true for TOK_NE.  */
        fc &= ~0x100;
        o(0x037a + (REX_BASE(r) << 8));
      }
      orex(0, r, 0, 0x0f); /* setxx %br */
      o(fc);
      o(0xc0 + REG_VALUE(r));
    } else if (v == VT_JMP || v == VT_JMPI) {
      t = v & 1;
      orex(0, r, 0, 0);
      oad(0xb8 + REG_VALUE(r), t);    /* mov $1, r */
      o(0x05eb + (REX_BASE(r) << 8)); /* jmp after */
      gsym(fc);
      orex(0, r, 0, 0);
      oad(0xb8 + REG_VALUE(r), t ^ 1); /* mov $0, r */
    } else if (v != r) {
      if ((r >= TREG_XMM0) && (r <= TREG_XMM7)) {
        if (v == TREG_ST0) {
          /* gen_cvt_ftof(VT_DOUBLE); */
          o(0xf0245cdd); /* fstpl -0x10(%rsp) */
          /* movsd -0x10(%rsp),%xmmN */
          o(0x100ff2);
          o(0x44 + REG_VALUE(r) * 8); /* %xmmN */
          o(0xf024);
        } else {
          if ((ft & VT_TYPE) == VT_FLOAT32) {
            o(0x100ff3);
          } else {
            assert((ft & VT_TYPE) == VT_FLOAT64);
            o(0x100ff2);
          }
          o(0xc0 + REG_VALUE(v) + REG_VALUE(r) * 8);
        }
      } else if (r == TREG_ST0) {
        assert((v >= TREG_XMM0) && (v <= TREG_XMM7));
        /* gen_cvt_ftof(VT_LDOUBLE); */
        /* movsd %xmmN,-0x10(%rsp) */
        o(0x110ff2);
        o(0x44 + REG_VALUE(r) * 8); /* %xmmN */
        o(0xf024);
        o(0xf02444dd); /* fldl -0x10(%rsp) */
      } else {
        orex(1, r, v, 0x89);
        o(0xc0 + REG_VALUE(r) + REG_VALUE(v) * 8); /* mov v, r */
      }
    }
  }
}

/* store register 'r' in lvalue 'v' */
ST_FUNC void store(int r, SValue *v) {
  int fr, bt, ft, fc;
  int op64 = 0;
  /* store the REX prefix in this variable when PIC is enabled */
  int pic = 0;

  fr = v->r & VT_VALMASK;
  ft = v->type.t;
  fc = v->c.i;
  if (fc != v->c.i && (v->sym))
    tcc_error("64 bit addend in store");
  bt = ft & VT_TYPE;

  /* XXX: incorrect if float reg to reg */
  if (bt == VT_FLOAT32) {
    o(0x66);
    o(pic);
    o(0x7e0f); /* movd */
    r = REG_VALUE(r);
  } else if (bt == VT_FLOAT64) {
    o(0x66);
    o(pic);
    o(0xd60f); /* movq */
    r = REG_VALUE(r);
  } else if (bt == VT_FLOAT128) {
    o(0xc0d9); /* fld %st(0) */
    o(pic);
    o(0xdb); /* fstpt */
    r = 7;
  } else {
    if (is_same_size_int(bt, VT_INT16))
      o(0x66);
    o(pic);
    if (is_same_size_int(bt, VT_INT8))
      orex(0, 0, r, 0x88);
    else if (is64_type(bt))
      op64 = 0x89;
    else
      orex(0, 0, r, 0x89);
  }
  if (op64) {
    if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {
      gen_modrm64(op64, r, v->r, v->sym, fc);
    } else if (fr != r) {
      /* XXX: don't we really come here? */
      o(0xc0 + fr + r * 8); /* mov r, fr */
    }
  } else {
    if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {
      gen_modrm(r, v->r, v->sym, fc);
    } else if (fr != r) {
      /* XXX: don't we really come here? */
      o(0xc0 + fr + r * 8); /* mov r, fr */
    }
  }
}

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp) {
  int r;
  if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
      ((vtop->sym) && (vtop->c.i - 4) == (int)(vtop->c.i - 4))) {
    /* constant symbolic case -> simple relocation */
    greloca(cur_text_section(), vtop->sym, ind + 1, R_X86_64_PC32,
            (int)(vtop->c.i - 4));
    oad(0xe8 + is_jmp, 0); /* call/jmp im */
  } else {
    /* otherwise, indirect call */
    r = TREG_R11;
    load(r, vtop);
    o(0x41); /* REX */
    o(0xff); /* call/jmp *r */
    o(0xd0 + REG_VALUE(r) + (is_jmp << 4));
  }
}

#ifdef TCC_WINDOWS_ABI

#define REGN 4
static const uint8_t arg_regs[REGN] = {TREG_RCX, TREG_RDX, TREG_R8, TREG_R9};

/* Prepare arguments in R10 and R11 rather than RCX and RDX
   because gv() will not ever use these */
static int arg_prepare_reg(int idx) {
  if (idx == 0 || idx == 1)
    /* idx=0: r10, idx=1: r11 */
    return idx + 10;
  else
    return arg_regs[idx];
}

static int func_scratch, func_alloca;

static void gen_offs_sp(int b, int r, int d) {
  orex(1, 0, r & 0x100 ? 0 : r, b);
  if (d == (char)d) {
    o(0x2444 | (REG_VALUE(r) << 3));
    g(d);
  } else {
    o(0x2484 | (REG_VALUE(r) << 3));
    gen_le32(d);
  }
}

static int using_regs(int size) { return !(size > 8 || (size & (size - 1))); }

static int is_sse_float(int t) {
  int bt;
  bt = t & VT_TYPE;
  return bt == VT_FLOAT32 || bt == VT_FLOAT64;
}

static int gfunc_arg_size(CType *type) {
  return size_align_of_type(type->t, NULL);
}

ST_FUNC void gfunc_call(int nb_args, CType *ret_type) {
  int size, r, args_size, i, d, bt, struct_size;
  int arg;
  uint32_t align;

  args_size = (nb_args < REGN ? REGN : nb_args) * PTR_SIZE;
  arg = nb_args;

  arg = nb_args;
  struct_size = args_size;

  for (i = 0; i < nb_args; i++) {
    --arg;
    bt = (vtop->type.t & VT_TYPE);

    size = gfunc_arg_size(&vtop->type);
    if (!using_regs(size)) {
      /* align to stack align size */
      size = (size + 15) & ~15;
      if (arg >= REGN) {
        d = get_reg_of_cls(RC_INT);
        gen_offs_sp(0x8d, d, struct_size);
        gen_offs_sp(0x89, d, arg * 8);
      } else {
        d = arg_prepare_reg(arg);
        gen_offs_sp(0x8d, d, struct_size);
      }
      struct_size += size;
    } else {
      if (is_sse_float(vtop->type.t)) {
        if (arg >= REGN) {
          gen_load_reg(TREG_XMM0);
          /* movq %xmm0, j*8(%rsp) */
          gen_offs_sp(0xd60f66, 0x100, arg * 8);
        } else {
          /* Load directly to xmmN register */
          gen_load_reg(TREG_XMM0 + arg);
          d = arg_prepare_reg(arg);
          /* mov %xmmN, %rxx */
          o(0x66);
          orex(1, d, 0, 0x7e0f);
          o(0xc0 + arg * 8 + REG_VALUE(d));
        }
      } else {

        r = gen_ldr();
        if (arg >= REGN) {
          gen_offs_sp(0x89, r, arg * 8);
        } else {
          d = arg_prepare_reg(arg);
          orex(1, d, r, 0x89); /* mov */
          o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
        }
      }
    }
    vpop(1);
  }
  save_rc_upstack(RC_CALLER_SAVED, 0);
  /* Copy R10 and R11 into RCX and RDX, respectively */
  if (nb_args > 0) {
    o(0xd1894c); /* mov %r10, %rcx */
    if (nb_args > 1) {
      o(0xda894c); /* mov %r11, %rdx */
    }
  }

  gcall_or_jmp(0);

  vpop(1);
  vpushi(0);
  vtop->type = *ret_type;
  bt = ret_type->t;
  if (is_integer(bt) && size_align_of_type(bt, &align) <= 8) {
    vtop->r = TREG_RAX;
    vtop->r2 = VT_CONST;
  } else if (is_same_size_int(bt, VT_INT128)) {
    vtop->r = TREG_RAX;
    vtop->r2 = TREG_RDX;
  } else if (bt == VT_FLOAT32 || bt == VT_FLOAT64) {
    vtop->r = TREG_XMM0;
    vtop->r2 = VT_CONST;
  } else {
    tcc_error(TCC_ERROR_UNIMPLEMENTED);
  }
}

#define FUNC_PROLOG_SIZE 11

/* generate function prolog of type 't' */
ST_FUNC void gfunc_prolog() {
  int addr, reg_param_index, bt, size;
  SValue *sv;
  CType *type;

  func_ret_sub = 0;
  func_scratch = 32;
  func_alloca = 0;
  loc = 0;

  addr = PTR_SIZE * 2;
  ind += FUNC_PROLOG_SIZE;
  func_sub_sp_offset = ind;
  reg_param_index = 0;

  /* define parameters */
  for (sv = vstack; sv <= vtop; sv++) {
    type = &sv->type;
    bt = type->t & (~VT_UNSIGNED);
    size = gfunc_arg_size(type);
    if (!using_regs(size)) {
      if (reg_param_index < REGN) {
        gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
      }
      sv->r = VT_LLOCAL | VT_LVAL;
      sv->c.i = addr;
    } else {
      if (reg_param_index < REGN) {
        /* save arguments passed by register */
        if ((bt == VT_FLOAT32) || (bt == VT_FLOAT64)) {
          o(0xd60f66); /* movq */
          gen_modrm(reg_param_index, VT_LOCAL, NULL, addr);
        } else {
          gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
        }  
      }
      sv->r = VT_LOCAL | VT_LVAL;
      sv->c.i = addr;
    }
    addr += 8;
    reg_param_index++;
  }

  while (reg_param_index < REGN) {
    reg_param_index++;
  }
  /* addr to next inst */
  vpushi(PTR_SIZE);
  vtop->r = VT_LOCAL | VT_LVAL;
}

/* generate function epilog */
ST_FUNC void gfunc_epilog(void) {
  int v, saved_ind;
  int btype;
  uint32_t align;

  btype = vtop->type.t;

  if (size_align_of_type(btype, &align) <= 8) {
    load(TREG_RAX, vtop);
  } else if ((btype == VT_FLOAT32) || (btype == VT_FLOAT64)) {
    load(TREG_XMM0, vtop);
  } else if (btype == VT_VOID) {
    /* do nothing */
  } else {
    tcc_error("Not support return value");
  }
  vpop(1);

  /* align local size to word & save local variables */
  func_scratch = (func_scratch + 15) & -16;
  loc = (loc & -16) - func_scratch;

  o(0xc9); /* leave */
  if (func_ret_sub == 0) {
    o(0xc3); /* ret */
  } else {
    o(0xc2); /* ret n */
    g(func_ret_sub);
    g(func_ret_sub >> 8);
  }

  saved_ind = ind;
  ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
  v = -loc;

  o(0xe5894855); /* push %rbp, mov %rsp, %rbp */
  o(0xec8148);   /* sub rsp, stacksize */
  gen_le32(v);

  /* add the "func_scratch" area after each alloca seen */
  gsym_addr(func_alloca, -func_scratch);

  cur_text_section()->data_offset = saved_ind;
  ind = cur_text_section()->data_offset;
  vpop(1);
}

#else

static void gadd_sp(int val) {
  if (val == (char)val) {
    o(0xc48348);
    g(val);
  } else {
    oad(0xc48148, val); /* add $xxx, %rsp */
  }
}

typedef enum X86_64_Mode {
  x86_64_mode_none,
  x86_64_mode_memory,
  x86_64_mode_integer,
  x86_64_mode_sse,
  x86_64_mode_x87
} X86_64_Mode;

static X86_64_Mode classify_x86_64_merge(X86_64_Mode a, X86_64_Mode b) {
  if (a == b)
    return a;
  else if (a == x86_64_mode_none)
    return b;
  else if (b == x86_64_mode_none)
    return a;
  else if ((a == x86_64_mode_memory) || (b == x86_64_mode_memory))
    return x86_64_mode_memory;
  else if ((a == x86_64_mode_integer) || (b == x86_64_mode_integer))
    return x86_64_mode_integer;
  else if ((a == x86_64_mode_x87) || (b == x86_64_mode_x87))
    return x86_64_mode_memory;
  else
    return x86_64_mode_sse;
}

static X86_64_Mode classify_x86_64_inner(CType *ty) {
  X86_64_Mode mode;
  Sym *f;

  switch (ty->t & VT_TYPE) {
  case VT_VOID:
    return x86_64_mode_none;

  case VT_INT32:
  case VT_INT32 | VT_UNSIGNED:
  case VT_INT8:
  case VT_INT8 | VT_UNSIGNED:
  case VT_INT16:
  case VT_INT16 | VT_UNSIGNED:
  case VT_INT64:
  case VT_INT64 | VT_UNSIGNED:
  case VT_FUNC:
    return x86_64_mode_integer;

  case VT_FLOAT32:
  case VT_FLOAT64:
    return x86_64_mode_sse;

  case VT_FLOAT128:
    return x86_64_mode_x87;
  }
  assert(0);
  return 0;
}

static X86_64_Mode classify_x86_64_arg(CType *ty, CType *ret, int *psize,
                                       int *palign, int *reg_count) {
  X86_64_Mode mode;
  int size, ret_t = 0;
  uint32_t align;


    size = size_align_of_type(ty->t, &align);
    *psize = (size + 7) & ~7;
    *palign = (align + 7) & ~7;

    if (size > 16) {
        tcc_error(TCC_ERROR_UNIMPLEMENTED);
      mode = x86_64_mode_memory;
    } else {
      mode = classify_x86_64_inner(ty);
      switch (mode) {
      case x86_64_mode_integer:
        if (size > 8) {
          *reg_count = 2;
          ret_t = VT_INT128;
        } else {
          *reg_count = 1;
          if (size > 4)
            ret_t = VT_INT64;
          else if (size > 2)
            ret_t = VT_INT32;
          else if (size > 1)
            ret_t = VT_INT16;
          else
            ret_t = VT_INT8;
          if (ty->t & VT_UNSIGNED)
            ret_t |= VT_UNSIGNED;
        }
        break;

      case x86_64_mode_x87:
        *reg_count = 1;
        ret_t = VT_FLOAT128;
        break;

      case x86_64_mode_sse:
        if (size > 8) {
          *reg_count = 2;
          ret_t = VT_FLOAT128;
        } else {
          *reg_count = 1;
          ret_t = (size > 4) ? VT_FLOAT64 : VT_FLOAT32;
        }
        break;
      default:
        break; /* nothing to be done for x86_64_mode_memory and
                  x86_64_mode_none*/
      }
    }
  

  if (ret) {
    ret->t = ret_t;
  }

  return mode;
}


#define REGN 6
static const uint8_t arg_regs[REGN] = {TREG_RDI, TREG_RSI, TREG_RDX,
                                       TREG_RCX, TREG_R8,  TREG_R9};

static int arg_prepare_reg(int idx) {
  if (idx == 2 || idx == 3)
    /* idx=2: r10, idx=3: r11 */
    return idx + 8;
  else
    return idx >= 0 && idx < REGN ? arg_regs[idx] : 0;
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
ST_FUNC void gfunc_call(int nb_args,CType *ret_type) {
  X86_64_Mode mode;
  CType type;
  int size, r, args_size, stack_adjust, i, reg_count, k;
  int align;
  int nb_reg_args = 0;
  int nb_sse_args = 0;
  int sse_reg, gen_reg;
  int btype;
  char *onstack = tcc_malloc((nb_args + 1) * sizeof(char));

  /* calculate the number of integer/float register arguments, remember
     arguments to be passed via stack (in onstack[]), and also remember
     if we have to align the stack pointer to 16 (onstack[i] == 2).  Needs
     to be done in a left-to-right pass over arguments.  */
  stack_adjust = 0;
  for (i = nb_args - 1; i >= 0; i--) {
    mode = classify_x86_64_arg(&vtop[-i].type, NULL, &size, &align, &reg_count);
    if (size == 0)
      continue;
    if (mode == x86_64_mode_sse && nb_sse_args + reg_count <= 8) {
      nb_sse_args += reg_count;
      onstack[i] = 0;
    } else if (mode == x86_64_mode_integer && nb_reg_args + reg_count <= REGN) {
      nb_reg_args += reg_count;
      onstack[i] = 0;
    } else if (mode == x86_64_mode_none) {
      onstack[i] = 0;
    } else {
      if (align == 16 && (stack_adjust &= 15)) {
        onstack[i] = 2;
        stack_adjust = 0;
      } else
        onstack[i] = 1;
      stack_adjust += size;
    }
  }

  /* fetch cpu flag before generating any code */
  if ((vtop->r & VT_VALMASK) == VT_CMP)
    gen_ldr();

  /* for struct arguments, we need to call memcpy and the function
     call breaks register passing arguments we are preparing.
     So, we process arguments which will be passed by stack first. */
  gen_reg = nb_reg_args;
  sse_reg = nb_sse_args;
  args_size = 0;
  stack_adjust &= 15;
  for (i = k = 0; i < nb_args;) {
    mode = classify_x86_64_arg(&vtop[-i].type, NULL, &size, &align, &reg_count);
    if (size) {
      if (!onstack[i + k]) {
        ++i;
        continue;
      }
      /* Possibly adjust stack to align SSE boundary.  We're processing
         args from right to left while allocating happens left to right
         (stack grows down), so the adjustment needs to happen _after_
         an argument that requires it.  */
      if (stack_adjust) {
        o(0x50); /* push %rax; aka sub $8,%rsp */
        args_size += 8;
        stack_adjust = 0;
      }
      if (onstack[i + k] == 2)
        stack_adjust = 1;
    }

    vrotb(i + 1);

    switch (vtop->type.t & VT_TYPE) {

    case VT_FLOAT128:
      gen_ldr();
      oad(0xec8148, size); /* sub $xxx, %rsp */
      o(0x7cdb);           /* fstpt 0(%rsp) */
      g(0x24);
      g(0x00);
      break;

    case VT_FLOAT32:
    case VT_FLOAT64:
      assert(mode == x86_64_mode_sse);
      gen_ldr();
      o(0x50); /* push $rax */
      /* movq %xmmN, (%rsp) */
      o(0xd60f66);
      o(0x04 + REG_VALUE(r) * 8);
      o(0x24);
      break;

    default:
      assert(mode == x86_64_mode_integer);
      /* simple type */
      /* XXX: implicit cast ? */
      r = gen_ldr();;
      orex(0, r, 0, 0x50 + REG_VALUE(r)); /* push r */
      break;
    }
    args_size += size;

    vpop(1);
    --nb_args;
    k++;
  }

  tcc_free(onstack);

  /* XXX This should be superfluous.  */
  save_rc_upstack(RC_CALLER_SAVED,0); /* save used temporary registers */

  /* then, we prepare register passing arguments.
     Note that we cannot set RDX and RCX in this loop because gv()
     may break these temporary registers. Let's use R10 and R11
     instead of them */
  assert(gen_reg <= REGN);
  assert(sse_reg <= 8);
  for (i = 0; i < nb_args; i++) {
    mode = classify_x86_64_arg(&vtop->type, &type, &size, &align, &reg_count);
    if (size == 0)
      continue;
    /* Alter stack entry type so that gv() knows how to treat it */
    vtop->type = type;
    if (mode == x86_64_mode_sse) {
      if (reg_count == 2) {
        sse_reg -= 2;
        gen_load_reg(TREG_XMM0);   /* Use pair load into xmm0 & xmm1 */
        if (sse_reg) { /* avoid redundant movaps %xmm0, %xmm0 */
          /* movaps %xmm1, %xmmN */
          o(0x280f);
          o(0xc1 + ((sse_reg + 1) << 3));
          /* movaps %xmm0, %xmmN */
          o(0x280f);
          o(0xc0 + (sse_reg << 3));
        }
      } else {
        assert(reg_count == 1);
        --sse_reg;
        /* Load directly to register */
        gen_load_reg(TREG_XMM0+sse_reg);
      }
    } else if (mode == x86_64_mode_integer) {
      /* simple type */
      /* XXX: implicit cast ? */
      int d;
      gen_reg -= reg_count;
      r = gen_ldr();
      d = arg_prepare_reg(gen_reg);
      orex(1, d, r, 0x89); /* mov */
      o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
      if (reg_count == 2) {
        d = arg_prepare_reg(gen_reg + 1);
        orex(1, d, vtop->r2, 0x89); /* mov */
        o(0xc0 + REG_VALUE(vtop->r2) * 8 + REG_VALUE(d));
      }
    }
    vpop(1);
  }
  assert(gen_reg == 0);
  assert(sse_reg == 0);

  /* We shouldn't have many operands on the stack anymore, but the
     call address itself is still there, and it might be in %eax
     (or edx/ecx) currently, which the below writes would clobber.
     So evict all remaining operands here.  */
  save_rc_upstack(RC_CALLER_SAVED,0);

  /* Copy R10 and R11 into RDX and RCX, respectively */
  if (nb_reg_args > 2) {
    o(0xd2894c); /* mov %r10, %rdx */
    if (nb_reg_args > 3) {
      o(0xd9894c); /* mov %r11, %rcx */
    }
  }
  gcall_or_jmp(0);
  if (args_size)
    gadd_sp(args_size);
  vpop(1);
  vpushi(0);
  vtop->type=*ret_type;
    btype=ret_type->t;
    if(is_integer(btype)&&size_align_of_type(btype,&align)<=8){
        vtop->r=TREG_RAX;
        vtop->r2=VT_CONST;
    }else if(is_same_size_int(btype,VT_INT128)){
        vtop->r=TREG_RAX;
        vtop->r2=TREG_RDX;
    }else if(btype==VT_FLOAT32 || btype==VT_FLOAT64){
        vtop->r=TREG_XMM0;
        vtop->r2=VT_CONST;
    }else{
        tcc_error("unsuport value type.");
    }
}

#define FUNC_PROLOG_SIZE 11

static void push_arg_reg(int i) {
  loc -= 8;
  gen_modrm64(0x89, arg_regs[i], VT_LOCAL, NULL, loc);
}

/* generate function prolog of type 't' */
ST_FUNC void gfunc_prolog() {
  X86_64_Mode mode;
  int i, addr, align, size, reg_count;
  int param_addr = 0, reg_param_index, sse_param_index;
  CType *type;
  SValue *psv;

  addr = PTR_SIZE * 2;
  loc = 0;
  ind += FUNC_PROLOG_SIZE;
  func_sub_sp_offset = ind;
  func_ret_sub = 0;

  reg_param_index = 0;
  sse_param_index = 0;

  /* define parameters */
  for(psv=vstack;psv<=vtop;psv++){
    type = &psv->type;
    mode = classify_x86_64_arg(type, NULL, &size, &align, &reg_count);
    switch (mode) {
    case x86_64_mode_sse:
      if (sse_param_index + reg_count <= 8) {
        /* save arguments passed by register */
        loc -= reg_count * 8;
        param_addr = loc;
        for (i = 0; i < reg_count; ++i) {
          o(0xd60f66); /* movq */
          gen_modrm(sse_param_index, VT_LOCAL, NULL, param_addr + i * 8);
          ++sse_param_index;
        }
      } else {
        addr = (addr + align - 1) & -align;
        param_addr = addr;
        addr += size;
      }
      break;

    case x86_64_mode_memory:
    case x86_64_mode_x87:
      addr = (addr + align - 1) & -align;
      param_addr = addr;
      addr += size;
      break;

    case x86_64_mode_integer: {
      if (reg_param_index + reg_count <= REGN) {
        /* save arguments passed by register */
        loc -= reg_count * 8;
        param_addr = loc;
        for (i = 0; i < reg_count; ++i) {
          gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL,
                      param_addr + i * 8);
          ++reg_param_index;
        }
      } else {
        addr = (addr + align - 1) & -align;
        param_addr = addr;
        addr += size;
      }
      break;
    }
    default:
      break; /* nothing to be done for x86_64_mode_none */
    }
    psv->r= VT_LOCAL | VT_LVAL;
    psv->c.i=param_addr;
  }
  vpushi(PTR_SIZE);
  vtop->r = VT_LOCAL | VT_LVAL;
}

/* generate function epilog */
ST_FUNC void gfunc_epilog(void) {
  int v, saved_ind,btype;
  uint32_t align;

  btype = vtop->type.t;
  if (size_align_of_type(btype, &align) <= 8) {
    load(TREG_RAX, vtop);
  } else if ((btype == VT_FLOAT32) || (btype == VT_FLOAT64)) {
    load(TREG_XMM0, vtop);
  } else if (btype == VT_VOID) {
    /* do nothing */
  } else {
    tcc_error("Not support return value");
  }
  vpop(1);

  o(0xc9); /* leave */
  if (func_ret_sub == 0) {
    o(0xc3); /* ret */
  } else {
    o(0xc2); /* ret n */
    g(func_ret_sub);
    g(func_ret_sub >> 8);
  }
  /* align local size to word & save local variables */
  v = (-loc + 15) & -16;
  saved_ind = ind;
  ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
  o(0xe5894855); /* push %rbp, mov %rsp, %rbp */
  o(0xec8148);   /* sub rsp, stacksize */
  gen_le32(v);
  ind = saved_ind;
  vpop(1);
}

#endif /* not WINDOWS ABI */

ST_FUNC void gen_fill_nops(int bytes) {
  while (bytes--)
    g(0x90);
}

/* generate a jump to a label */
int gjmp(int t) { return gjmp2(0xe9, t); }

/* generate a jump to a fixed address */
void gjmp_addr(int a) {
  int r;
  r = a - ind - 2;
  if (r == (char)r) {
    g(0xeb);
    g(r);
  } else {
    oad(0xe9, a - ind - 5);
  }
}

ST_FUNC void gtst_addr(int inv, int a) {
  int v = vtop->r & VT_VALMASK;
  if (v == VT_CMP) {
    inv ^= (vtop--)->c.i;
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
    vtop--;
  }
}

/* generate a test. set 'inv' to invert test. Stack entry is popped */
ST_FUNC int gtst(int inv, int t) {
  int v = vtop->r & VT_VALMASK;

  if (v == VT_CMP) {
    /* fast case : can jump directly since flags are set */
    if (vtop->c.i & 0x100) {
      /* This was a float compare.  If the parity flag is set
         the result was unordered.  For anything except != this
         means false and we don't jump (anding both conditions).
         For != this means true (oring both).
         Take care about inverting the test.  We need to jump
         to our target if the result was unordered and test wasn't NE,
         otherwise if unordered we don't want to jump.  */
      vtop->c.i &= ~0x100;
      if (inv == (vtop->c.i == TOK_NE))
        o(0x067a); /* jp +6 */
      else {
        g(0x0f);
        t = gjmp2(0x8a, t); /* jp t */
      }
    }
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
  vtop--;
  return t;
}

/* generate an integer binary operation */
ST_FUNC void gen_opi(int op) {
  int r, fr, opc, c;
  int ll, uu, cc;

  ll = is64_type(vtop[-1].type.t);
  uu = (vtop[-1].type.t & VT_UNSIGNED) != 0;
  cc = (vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->sym != NULL);

  switch (op) {
  case TOK_ADD:
  case TOK_ADDC1: /* add with carry generation */
    opc = 0;
  gen_op8:
    if (cc && (!ll || (int)vtop->c.i == vtop->c.i)) {
      /* constant case */
      vswap();
      r = gen_ldr();
      vswap();
      c = vtop->c.i;
      if (c == (char)c) {
        /* XXX: generate inc and dec for smaller code ? */
        orex(ll, r, 0, 0x83);
        o(0xc0 | (opc << 3) | REG_VALUE(r));
        g(c);
      } else {
        orex(ll, r, 0, 0x81);
        oad(0xc0 | (opc << 3) | REG_VALUE(r), c);
      }
    } else {
      vswap();
      gen_ldr();
      vswap();
      gen_ldr();
      r = vtop[-1].r;
      fr = vtop[0].r;
      orex(ll, r, fr, (opc << 3) | 0x01);
      o(0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
    }
    vtop--;
    if (op >= TOK_ULT && op <= TOK_GT) {
      vtop->r = VT_CMP;
      vtop->c.i = op;
    }
    break;
  case TOK_SUB:
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
    gen_ldr();
    vswap();
    gen_ldr();
    vswap();
    r = vtop[-1].r;
    fr = vtop[0].r;
    orex(ll, fr, r, 0xaf0f); /* imul fr, r */
    o(0xc0 + REG_VALUE(fr) + REG_VALUE(r) * 8);
    vtop--;
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
    if (cc) {
      /* constant case */
      vswap();
      r = gen_ldr();
      vswap();
      orex(ll, r, 0, 0xc1); /* shl/shr/sar $xxx, r */
      o(opc | REG_VALUE(r));
      g(vtop->c.i & (ll ? 63 : 31));
    } else {
      /* we generate the shift in ecx */
      /* we generate the shift in ecx */
      save_reg_upstack(TREG_RCX, 1);
      gen_load_reg(TREG_RCX);
      get_reg_attr(TREG_RCX)->s |= RS_LOCKED;
      vswap();
      gen_ldr();
      r = vtop->r;
      vswap();
      orex(ll, r, 0, 0xd3); /* shl/shr/sar %cl, r */
      o(opc | REG_VALUE(r));
      get_reg_attr(TREG_RCX)->s &= ~RS_LOCKED;
    }
    vtop--;
    break;
  case TOK_UDIV:
  case TOK_UMOD:
    uu = 1;
    goto divmod;
  case TOK_DIV:
  case TOK_MOD:
    uu = 0;
  divmod:
    /* first operand must be in eax */
    /* XXX: need better constraint for second operand */
    gen_load_reg(TREG_RCX);
    vswap();
    gen_load_reg(TREG_RAX);
    vswap();
    r = vtop[-1].r;
    fr = vtop[0].r;
    vtop--;
    save_reg_upstack(TREG_RDX, 0);
    orex(ll, 0, 0, uu ? 0xd231 : 0x99); /* xor %edx,%edx : cqto */
    orex(ll, fr, 0, 0xf7);              /* div fr, %eax */
    o((uu ? 0xf0 : 0xf8) + REG_VALUE(fr));
    if (op == '%' || op == TOK_UMOD)
      r = TREG_RDX;
    else
      r = TREG_RAX;
    vtop->r = r;
    break;
  default:
    opc = 7;
    goto gen_op8;
  }
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranteed to have the same floating point type */
/* XXX: need to use ST1 too */
ST_FUNC void gen_opf(int op) {
  int a, ft, fc, swapped, r;
  int float_type_r =
      (vtop->type.t & VT_TYPE) == VT_FLOAT128 ? TREG_ST0 : VT_CONST;

  /* convert constants to memory references */
  if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
    vswap();
    gvreg(float_type_r, RC_FLOAT);
    vswap();
  }
  if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
    gvreg(float_type_r, RC_FLOAT);

  /* must put at least one value in the floating point register */
  if ((vtop[-1].r & VT_LVAL) && (vtop[0].r & VT_LVAL)) {
    vswap();
    gvreg(float_type_r, RC_FLOAT);
    vswap();
  }
  swapped = 0;
  /* swap the stack if needed so that t1 is the register and t2 is
     the memory reference */
  if (vtop[-1].r & VT_LVAL) {
    vswap();
    swapped = 1;
  }
  if ((vtop->type.t & VT_TYPE) == VT_FLOAT128) {
    if (op >= TOK_ULT && op <= TOK_GT) {
      /* load on stack second operand */
      load(TREG_ST0, vtop);
      save_reg_upstack(TREG_RAX, 0); /* eax is used by FP comparison code */
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
      o(0xe0df);   /* fnstsw %ax */
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
      vtop--;
      vtop->r = VT_CMP;
      vtop->c.i = op;
    } else {
      /* no memory reference possible for long double operations */
      load(TREG_ST0, vtop);
      swapped = !swapped;

      switch (op) {
      default:
        tcc_error(TCC_ERROR_UNIMPLEMENTED);
        return;
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
      case TOK_DIV:
        a = 6;
        if (swapped)
          a++;
        break;
      }
      ft = vtop->type.t;
      fc = vtop->c.i;
      o(0xde); /* fxxxp %st, %st(1) */
      o(0xc1 + (a << 3));
      vtop--;
    }
  } else {
    if (op >= TOK_ULT && op <= TOK_GT) {
      /* if saved lvalue, then we must reload it */
      r = vtop->r;
      fc = vtop->c.i;
      if ((r & VT_VALMASK) == VT_LLOCAL) {
        SValue v1;
        r = get_reg_of_cls(RC_INT);
        v1.type.t = VT_INT64;
        v1.r = VT_LOCAL | VT_LVAL;
        v1.c.i = fc;
        load(r, &v1);
        fc = 0;
      }

      if (op == TOK_EQ || op == TOK_NE) {
        swapped = 0;
      } else {
        if (op == TOK_LE || op == TOK_LT)
          swapped = !swapped;
        if (op == TOK_LE || op == TOK_GE) {
          op = 0x93; /* setae */
        } else {
          op = 0x97; /* seta */
        }
      }

      if (swapped) {
        gvreg(VT_CONST, RC_FLOAT);
        vswap();
      }
      assert(!(vtop[-1].r & VT_LVAL));

      if ((vtop->type.t & VT_TYPE) == VT_FLOAT128)
        o(0x66);
      if (op == TOK_EQ || op == TOK_NE)
        o(0x2e0f); /* ucomisd */
      else
        o(0x2f0f); /* comisd */

      if (vtop->r & VT_LVAL) {
        gen_modrm(vtop[-1].r, r, vtop->sym, fc);
      } else {
        o(0xc0 + REG_VALUE(vtop[0].r) + REG_VALUE(vtop[-1].r) * 8);
      }

      vtop--;
      vtop->r = VT_CMP;
      vtop->c.i = op | 0x100;
    } else {
      assert((vtop->type.t & VT_TYPE) != VT_FLOAT128);
      switch (op) {
      default:
        tcc_error(TCC_ERROR_UNIMPLEMENTED);
        return;
      case TOK_ADD:
        a = 0;
        break;
      case TOK_SUB:
        a = 4;
        break;
      case TOK_MULL:
        a = 1;
        break;
      case TOK_DIV:
        a = 6;
        break;
      }
      ft = vtop->type.t;
      fc = vtop->c.i;
      assert((ft & VT_TYPE) != VT_FLOAT128);

      r = vtop->r;
      /* if saved lvalue, then we must reload it */
      if ((vtop->r & VT_VALMASK) == VT_LLOCAL) {
        SValue v1;
        r = get_reg_of_cls(RC_INT);
        v1.type.t = VT_INT64;
        v1.r = VT_LOCAL | VT_LVAL;
        v1.c.i = fc;
        load(r, &v1);
        fc = 0;
      }

      assert(!(vtop[-1].r & VT_LVAL));
      if (swapped) {
        assert(vtop->r & VT_LVAL);
        gvreg(VT_CONST, RC_FLOAT);
        vswap();
      }

      if ((ft & VT_TYPE) == VT_FLOAT64) {
        o(0xf2);
      } else {
        o(0xf3);
      }
      o(0x0f);
      o(0x58 + a);

      if (vtop->r & VT_LVAL) {
        gen_modrm(vtop[-1].r, r, vtop->sym, fc);
      } else {
        o(0xc0 + REG_VALUE(vtop[0].r) + REG_VALUE(vtop[-1].r) * 8);
      }

      vtop--;
    }
  }
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
ST_FUNC void gen_cvt_itof(int t) {
  if ((t & VT_TYPE) == VT_FLOAT128) {
    save_reg_upstack(TREG_ST0, 0);
    gen_ldr();
    if (is_same_size_int(vtop->type.t & VT_TYPE, VT_INT64)) {
      /* signed long long to float/double/long double (unsigned case
         is handled generically) */
      o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
      o(0x242cdf);                      /* fildll (%rsp) */
      o(0x08c48348);                    /* add $8, %rsp */
    } else if ((vtop->type.t & VT_TYPE) == (VT_INT32 | VT_UNSIGNED)) {
      /* unsigned int to float/double/long double */
      o(0x6a); /* push $0 */
      g(0x00);
      o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
      o(0x242cdf);                      /* fildll (%rsp) */
      o(0x10c48348);                    /* add $16, %rsp */
    } else {
      /* int to float/double/long double */
      o(0x50 + (vtop->r & VT_VALMASK)); /* push r */
      o(0x2404db);                      /* fildl (%rsp) */
      o(0x08c48348);                    /* add $8, %rsp */
    }
    vtop->r = TREG_ST0;
  } else {
    int r = get_reg_of_cls(RC_FLOAT);
    gvreg(VT_CONST, RC_INT);
    o(0xf2 + ((t & VT_TYPE) == VT_FLOAT32 ? 1 : 0));
    if ((vtop->type.t & VT_TYPE) == (VT_INT32 | VT_UNSIGNED) ||
        is_same_size_int(vtop->type.t & VT_TYPE, VT_INT64)) {
      o(0x48); /* REX */
    }
    o(0x2a0f);
    o(0xc0 + (vtop->r & VT_VALMASK) + REG_VALUE(r) * 8); /* cvtsi2sd */
    vtop->r = r;
  }
}

/* computed goto support */
ST_FUNC void ggoto(void) {
  gcall_or_jmp(1);
  vtop--;
}
