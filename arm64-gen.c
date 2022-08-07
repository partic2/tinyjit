/*
 *  A64 code generator for TCC
 *
 *  Copyright (c) 2014-2015 Edmund Grimley Evans
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

/******************************************************/
#define USING_GLOBALS
#include "xxx-gen.h"
#include "tccutils.h"
#include "arm64-gen.h"

#include <assert.h>

static struct reg_attr reg_attrs[NB_REGS] = {
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE}, //X0
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE}, //X8
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},
   {RC_INT | RC_CALLER_SAVED, REGISTER_SIZE},  //X18
   {RC_R30 | RC_SPECIAL, REGISTER_SIZE}, // not in RC_INT as we make special use of x30
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
  {RC_FLOAT | RC_CALLER_SAVED, REGISTER_SIZE},
};



ST_FUNC void arch_gen_init(){
}

ST_FUNC void arch_gen_deinit(){
}

ST_FUNC struct reg_attr *get_reg_attr(int r) { return reg_attrs + r; }

#define IS_FREG(x) ((x) >= TREG_F(0))

static uint32_t intr(int r)
{
    assert(TREG_R(0) <= r && r <= TREG_R30);
    return r < TREG_R30 ? r : 30;
}

static uint32_t fltr(int r)
{
    assert(TREG_F(0) <= r && r <= TREG_F(7));
    return r - TREG_F(0);
}

// Add an instruction to text section:
ST_FUNC void o(unsigned int c)
{
    int ind1 = ind + 4;
    if (ind1 > cur_text_section()->data_allocated)
        section_realloc(cur_text_section(), ind1);
    write32le(cur_text_section()->data + ind, c);
    ind = ind1;
}

static int arm64_encode_bimm64(uint64_t x)
{
    int neg = x & 1;
    int rep, pos, len;

    if (neg)
        x = ~x;
    if (!x)
        return -1;

    if (x >> 2 == (x & (((uint64_t)1 << (64 - 2)) - 1)))
        rep = 2, x &= ((uint64_t)1 << 2) - 1;
    else if (x >> 4 == (x & (((uint64_t)1 << (64 - 4)) - 1)))
        rep = 4, x &= ((uint64_t)1 <<  4) - 1;
    else if (x >> 8 == (x & (((uint64_t)1 << (64 - 8)) - 1)))
        rep = 8, x &= ((uint64_t)1 <<  8) - 1;
    else if (x >> 16 == (x & (((uint64_t)1 << (64 - 16)) - 1)))
        rep = 16, x &= ((uint64_t)1 << 16) - 1;
    else if (x >> 32 == (x & (((uint64_t)1 << (64 - 32)) - 1)))
        rep = 32, x &= ((uint64_t)1 << 32) - 1;
    else
        rep = 64;

    pos = 0;
    if (!(x & (((uint64_t)1 << 32) - 1))) x >>= 32, pos += 32;
    if (!(x & (((uint64_t)1 << 16) - 1))) x >>= 16, pos += 16;
    if (!(x & (((uint64_t)1 <<  8) - 1))) x >>= 8, pos += 8;
    if (!(x & (((uint64_t)1 <<  4) - 1))) x >>= 4, pos += 4;
    if (!(x & (((uint64_t)1 <<  2) - 1))) x >>= 2, pos += 2;
    if (!(x & (((uint64_t)1 <<  1) - 1))) x >>= 1, pos += 1;

    len = 0;
    if (!(~x & (((uint64_t)1 << 32) - 1))) x >>= 32, len += 32;
    if (!(~x & (((uint64_t)1 << 16) - 1))) x >>= 16, len += 16;
    if (!(~x & (((uint64_t)1 << 8) - 1))) x >>= 8, len += 8;
    if (!(~x & (((uint64_t)1 << 4) - 1))) x >>= 4, len += 4;
    if (!(~x & (((uint64_t)1 << 2) - 1))) x >>= 2, len += 2;
    if (!(~x & (((uint64_t)1 << 1) - 1))) x >>= 1, len += 1;

    if (x)
        return -1;
    if (neg) {
        pos = (pos + len) & (rep - 1);
        len = rep - len;
    }
    return ((0x1000 & rep << 6) | (((rep - 1) ^ 31) << 1 & 63) |
            ((rep - pos) & (rep - 1)) << 6 | (len - 1));
}

static uint32_t arm64_movi(int r, uint64_t x)
{
    uint64_t m = 0xffff;
    int e;
    if (!(x & ~m))
        return 0x52800000 | r | x << 5; // movz w(r),#(x)
    if (!(x & ~(m << 16)))
        return 0x52a00000 | r | x >> 11; // movz w(r),#(x >> 16),lsl #16
    if (!(x & ~(m << 32)))
        return 0xd2c00000 | r | x >> 27; // movz x(r),#(x >> 32),lsl #32
    if (!(x & ~(m << 48)))
        return 0xd2e00000 | r | x >> 43; // movz x(r),#(x >> 48),lsl #48
    if ((x & ~m) == m << 16)
        return (0x12800000 | r |
                (~x << 5 & 0x1fffe0)); // movn w(r),#(~x)
    if ((x & ~(m << 16)) == m)
        return (0x12a00000 | r |
                (~x >> 11 & 0x1fffe0)); // movn w(r),#(~x >> 16),lsl #16
    if (!~(x | m))
        return (0x92800000 | r |
                (~x << 5 & 0x1fffe0)); // movn x(r),#(~x)
    if (!~(x | m << 16))
        return (0x92a00000 | r |
                (~x >> 11 & 0x1fffe0)); // movn x(r),#(~x >> 16),lsl #16
    if (!~(x | m << 32))
        return (0x92c00000 | r |
                (~x >> 27 & 0x1fffe0)); // movn x(r),#(~x >> 32),lsl #32
    if (!~(x | m << 48))
        return (0x92e00000 | r |
                (~x >> 43 & 0x1fffe0)); // movn x(r),#(~x >> 32),lsl #32
    if (!(x >> 32) && (e = arm64_encode_bimm64(x | x << 32)) >= 0)
        return 0x320003e0 | r | (uint32_t)e << 10; // movi w(r),#(x)
    if ((e = arm64_encode_bimm64(x)) >= 0)
        return 0xb20003e0 | r | (uint32_t)e << 10; // movi x(r),#(x)
    return 0;
}

static void arm64_movimm(int r, uint64_t x)
{
    uint32_t i;
    if ((i = arm64_movi(r, x)))
        o(i); // a single MOV
    else {
        // MOVZ/MOVN and 1-3 MOVKs
        int z = 0, m = 0;
        uint32_t mov1 = 0xd2800000; // movz
        uint64_t x1 = x;
        for (i = 0; i < 64; i += 16) {
            z += !(x >> i & 0xffff);
            m += !(~x >> i & 0xffff);
        }
        if (m > z) {
            x1 = ~x;
            mov1 = 0x92800000; // movn
        }
        for (i = 0; i < 64; i += 16)
            if (x1 >> i & 0xffff) {
                o(mov1 | r | (x1 >> i & 0xffff) << 5 | i << 17);
                // movz/movn x(r),#(*),lsl #(i)
                break;
            }
        for (i += 16; i < 64; i += 16)
            if (x1 >> i & 0xffff)
                o(0xf2800000 | r | (x >> i & 0xffff) << 5 | i << 17);
                // movk x(r),#(*),lsl #(i)
    }
}

// Patch all branches in list pointed to by t to branch to a:
ST_FUNC void gsym_addr(int t_, int a_)
{
    uint32_t t = t_;
    uint32_t a = a_;
    while (t) {
        unsigned char *ptr = cur_text_section()->data + t;
        uint32_t next = read32le(ptr);
        if (a - t + 0x8000000 >= 0x10000000)
            tcc_error("branch out of range");
        write32le(ptr, (a - t == 4 ? 0xd503201f : // nop
                        0x14000000 | ((a - t) >> 2 & 0x3ffffff))); // b
        t = next;
    }
}

static int arm64_type_size(int t)
{
    /*
     * case values are in increasing order (from 1 to 11).
     * which 'may' help compiler optimizers. See tcc.h
     */
    switch (t & VT_TYPE) {
    case VT_INT8:
    case VT_INT8|VT_UNSIGNED: 
    return 0;
    case VT_INT16:
    case VT_INT16|VT_UNSIGNED:
    return 1;
    case VT_INT32:
    case VT_INT32|VT_UNSIGNED: 
    return 2;
    case VT_INT64:
    case VT_INT64|VT_UNSIGNED:
    return 3;
    case VT_FUNC: return 3;
    case VT_FLOAT32: return 2;
    case VT_FLOAT64: return 3;
    case VT_FLOAT128: return 4;
    }
    assert(0);
    return 0;
}

static void arm64_spoff(int reg, uint64_t off)
{
    uint32_t sub = off >> 63;
    if (sub)
        off = -off;
    if (off < 4096)
        o(0x910003e0 | sub << 30 | reg | off << 10);
        // (add|sub) x(reg),sp,#(off)
    else {
        arm64_movimm(30, off); // use x30 for offset
        o(0x8b3e63e0 | sub << 30 | reg); // (add|sub) x(reg),sp,x30
    }
}

/* invert 0: return value to use for store/load */
/* invert 1: return value to use for arm64_sym */
static uint64_t arm64_check_offset(int invert, int sz_, uint64_t off)
{
    uint32_t sz = sz_;
    if (!(off & ~((uint32_t)0xfff << sz)) ||
        (off < 256 || -off <= 256))
        return invert ? off : 0ul;
    else if ((off & ((uint32_t)0xfff << sz)))
        return invert ? off & ((uint32_t)0xfff << sz)
		      : off & ~((uint32_t)0xfff << sz);
    else if (off & 0x1ff)
        return invert ? off & 0x1ff : off & ~0x1ff;
    else
        return invert ? 0ul : off;
}

static void arm64_ldrx(int sg, int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    if (sz >= 2)
        sg = 0;
    if (!(off & ~((uint32_t)0xfff << sz)))
        o(0x39400000 | dst | bas << 5 | off << (10 - sz) |
          (uint32_t)!!sg << 23 | sz << 30); // ldr(*) x(dst),[x(bas),#(off)]
    else if (off < 256 || -off <= 256)
        o(0x38400000 | dst | bas << 5 | (off & 511) << 12 |
          (uint32_t)!!sg << 23 | sz << 30); // ldur(*) x(dst),[x(bas),#(off)]
    else {
        arm64_movimm(30, off); // use x30 for offset
        o(0x38206800 | dst | bas << 5 | (uint32_t)30 << 16 |
          (uint32_t)(!!sg + 1) << 22 | sz << 30); // ldr(*) x(dst),[x(bas),x30]
    }
}

static void arm64_ldrv(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    if (!(off & ~((uint32_t)0xfff << sz)))
        o(0x3d400000 | dst | bas << 5 | off << (10 - sz) |
          (sz & 4) << 21 | (sz & 3) << 30); // ldr (s|d|q)(dst),[x(bas),#(off)]
    else if (off < 256 || -off <= 256)
        o(0x3c400000 | dst | bas << 5 | (off & 511) << 12 |
          (sz & 4) << 21 | (sz & 3) << 30); // ldur (s|d|q)(dst),[x(bas),#(off)]
    else {
        arm64_movimm(30, off); // use x30 for offset
        o(0x3c606800 | dst | bas << 5 | (uint32_t)30 << 16 |
          sz << 30 | (sz & 4) << 21); // ldr (s|d|q)(dst),[x(bas),x30]
    }
}

static void arm64_ldrs(int reg_, int size)
{
    uint32_t reg = reg_;
    // Use x30 for intermediate value in some cases.
    switch (size) {
    default: assert(0); break;
    case 0:
        /* Can happen with zero size structs */
        break;
    case 1:
        arm64_ldrx(0, 0, reg, reg, 0);
        break;
    case 2:
        arm64_ldrx(0, 1, reg, reg, 0);
        break;
    case 3:
        arm64_ldrx(0, 1, 30, reg, 0);
        arm64_ldrx(0, 0, reg, reg, 2);
        o(0x2a0043c0 | reg | reg << 16); // orr x(reg),x30,x(reg),lsl #16
        break;
    case 4:
        arm64_ldrx(0, 2, reg, reg, 0);
        break;
    case 5:
        arm64_ldrx(0, 2, 30, reg, 0);
        arm64_ldrx(0, 0, reg, reg, 4);
        o(0xaa0083c0 | reg | reg << 16); // orr x(reg),x30,x(reg),lsl #32
        break;
    case 6:
        arm64_ldrx(0, 2, 30, reg, 0);
        arm64_ldrx(0, 1, reg, reg, 4);
        o(0xaa0083c0 | reg | reg << 16); // orr x(reg),x30,x(reg),lsl #32
        break;
    case 7:
        arm64_ldrx(0, 2, 30, reg, 0);
        arm64_ldrx(0, 2, reg, reg, 3);
        o(0x53087c00 | reg | reg << 5); // lsr w(reg), w(reg), #8
        o(0xaa0083c0 | reg | reg << 16); // orr x(reg),x30,x(reg),lsl #32
        break;
    case 8:
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 9:
        arm64_ldrx(0, 0, reg + 1, reg, 8);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 10:
        arm64_ldrx(0, 1, reg + 1, reg, 8);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 11:
        arm64_ldrx(0, 2, reg + 1, reg, 7);
        o(0x53087c00 | (reg+1) | (reg+1) << 5); // lsr w(reg+1), w(reg+1), #8
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 12:
        arm64_ldrx(0, 2, reg + 1, reg, 8);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 13:
        arm64_ldrx(0, 3, reg + 1, reg, 5);
        o(0xd358fc00 | (reg+1) | (reg+1) << 5); // lsr x(reg+1), x(reg+1), #24
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 14:
        arm64_ldrx(0, 3, reg + 1, reg, 6);
        o(0xd350fc00 | (reg+1) | (reg+1) << 5); // lsr x(reg+1), x(reg+1), #16
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 15:
        arm64_ldrx(0, 3, reg + 1, reg, 7);
        o(0xd348fc00 | (reg+1) | (reg+1) << 5); // lsr x(reg+1), x(reg+1), #8
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 16:
        o(0xa9400000 | reg | (reg+1) << 10 | reg << 5);
        // ldp x(reg),x(reg+1),[x(reg)]
        break;
    }
}

static void arm64_strx(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    if (!(off & ~((uint32_t)0xfff << sz)))
        o(0x39000000 | dst | bas << 5 | off << (10 - sz) | sz << 30);
        // str(*) x(dst),[x(bas],#(off)]
    else if (off < 256 || -off <= 256)
        o(0x38000000 | dst | bas << 5 | (off & 511) << 12 | sz << 30);
        // stur(*) x(dst),[x(bas],#(off)]
    else {
        arm64_movimm(30, off); // use x30 for offset
        o(0x38206800 | dst | bas << 5 | (uint32_t)30 << 16 | sz << 30);
        // str(*) x(dst),[x(bas),x30]
    }
}

static void arm64_strv(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    if (!(off & ~((uint32_t)0xfff << sz)))
        o(0x3d000000 | dst | bas << 5 | off << (10 - sz) |
          (sz & 4) << 21 | (sz & 3) << 30); // str (s|d|q)(dst),[x(bas),#(off)]
    else if (off < 256 || -off <= 256)
        o(0x3c000000 | dst | bas << 5 | (off & 511) << 12 |
          (sz & 4) << 21 | (sz & 3) << 30); // stur (s|d|q)(dst),[x(bas),#(off)]
    else {
        arm64_movimm(30, off); // use x30 for offset
        o(0x3c206800 | dst | bas << 5 | (uint32_t)30 << 16 |
          sz << 30 | (sz & 4) << 21); // str (s|d|q)(dst),[x(bas),x30]
    }
}

static void arm64_sym(int r, Sym *sym, unsigned long addend)
{
    //FIXME: R_AARCH64_ADR_GOT_PAGE and R_AARCH64_LD64_GOT_LO12_NC are not support, switch to another way.
    greloca(cur_text_section(), sym, ind, R_AARCH64_ADR_GOT_PAGE, 0);
    o(0x90000000 | r);            // adrp xr, #sym
    greloca(cur_text_section(), sym, ind, R_AARCH64_LD64_GOT_LO12_NC, 0);
    o(0xf9400000 | r | (r << 5)); // ld xr,[xr, #sym]
    if (addend) {
        // add xr, xr, #addend
	if (addend & 0xffful)
           o(0x91000000 | r | r << 5 | (addend & 0xfff) << 10);
        if (addend > 0xffful) {
            // add xr, xr, #addend, lsl #12
	    if (addend & 0xfff000ul)
                o(0x91400000 | r | r << 5 | ((addend >> 12) & 0xfff) << 10);
            if (addend > 0xfffffful) {
		/* very unlikely */
		int t = r ? 0 : 1;
		o(0xf81f0fe0 | t);            /* str xt, [sp, #-16]! */
		arm64_movimm(t, addend & ~0xfffffful); // use xt for addent
		o(0x91000000 | r | (t << 5)); /* add xr, xt, #0 */
		o(0xf84107e0 | t);            /* ldr xt, [sp], #16 */
	    }
        }
    }
}

static void arm64_load_cmp(int r, SValue *sv);

ST_FUNC void load(int r, SValue *sv)
{
    int svtt = sv->type.t;
    int svr = sv->r ;
    int svrv = svr & VT_VALMASK;
    uint64_t svcul = (uint32_t)sv->c.i;
    svcul = svcul >> 31 & 1 ? svcul - ((uint64_t)1 << 32) : svcul;

    if (svr == (VT_LOCAL | VT_LVAL)) {
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 29, svcul);
        else
            arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                       intr(r), 29, svcul);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL)) {
	if (sv->sym)
            arm64_sym(30, sv->sym, // use x30 for address
	              arm64_check_offset(0, arm64_type_size(svtt), sv->c.i));
	else
	    arm64_movimm (30, sv->c.i);
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), sv->c.i));
        else
            arm64_ldrx(!(svtt&VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), sv->c.i));
        return;
    }

    if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) {
        if ((svtt & VT_TYPE) != VT_VOID) {
            if (IS_FREG(r))
                arm64_ldrv(arm64_type_size(svtt), fltr(r), intr(svrv), 0);
            else
                arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                           intr(r), intr(svrv), 0);
        }
        return;
    }

    if (svr == (VT_CONST | VT_LVAL) && sv->sym) {
        arm64_sym(30, sv->sym, // use x30 for address
		  arm64_check_offset(0, arm64_type_size(svtt), svcul));
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcul));
        else
            arm64_ldrx(!(svtt&VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcul));
        return;
    }

    if (svr == VT_CONST && sv->sym) {
        arm64_sym(intr(r), sv->sym, svcul);
        return;
    }

    if (svr == VT_CONST) {
        if ((svtt & VT_TYPE) != VT_VOID)
            arm64_movimm(intr(r), arm64_type_size(svtt) == 3 ?
                         sv->c.i : (uint32_t)svcul);
        return;
    }

    if (svr < VT_CONST) {
        if (IS_FREG(r) && IS_FREG(svr))
            if (svtt == VT_FLOAT128)
                o(0x4ea01c00 | fltr(r) | fltr(svr) << 5);
                    // mov v(r).16b,v(svr).16b
            else
                o(0x1e604000 | fltr(r) | fltr(svr) << 5); // fmov d(r),d(svr)
        else if (!IS_FREG(r) && !IS_FREG(svr))
            o(0xaa0003e0 | intr(r) | intr(svr) << 16); // mov x(r),x(svr)
        else
            assert(0);
      return;
    }

    if (svr == VT_LOCAL) {
        if (-svcul < 0x1000)
            o(0xd10003a0 | intr(r) | -svcul << 10); // sub x(r),x29,#...
        else {
            arm64_movimm(30, -svcul); // use x30 for offset
            o(0xcb0003a0 | intr(r) | (uint32_t)30 << 16); // sub x(r),x29,x30
        }
        return;
    }

    if (svr == VT_JMP || svr == VT_JMPI) {
        int t = (svr == VT_JMPI);
        arm64_movimm(intr(r), t);
        o(0x14000002); // b .+8
        if (t) { gsym_addr(t, ind);}
        arm64_movimm(intr(r), t ^ 1);
        return;
    }

    if (svr == (VT_LLOCAL | VT_LVAL)) {
        arm64_ldrx(0, 3, 30, 29, svcul); // use x30 for offset
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, 0);
        else
            arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                       intr(r), 30, 0);
        return;
    }

    if (svr == VT_CMP) {
        arm64_load_cmp(r, sv);
        return;
    }

    printf("load(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcul);
    assert(0);
}

ST_FUNC void store(int r, SValue *sv)
{
    int svtt = sv->type.t;
    int svr = sv->r ;
    int svrv = svr & VT_VALMASK;
    uint64_t svcul = (uint32_t)sv->c.i;
    svcul = svcul >> 31 & 1 ? svcul - ((uint64_t)1 << 32) : svcul;

    if (svr == (VT_LOCAL | VT_LVAL)) {
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 29, svcul);
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 29, svcul);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL)) {
	if (sv->sym)
            arm64_sym(30, sv->sym, // use x30 for address
		      arm64_check_offset(0, arm64_type_size(svtt), sv->c.i));
	else
	    arm64_movimm (30, sv->c.i);
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), sv->c.i));
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), sv->c.i));
        return;
    }

    if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) {
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), intr(svrv), 0);
        else
            arm64_strx(arm64_type_size(svtt), intr(r), intr(svrv), 0);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL) && sv->sym) {
        arm64_sym(30, sv->sym, // use x30 for address
		  arm64_check_offset(0, arm64_type_size(svtt), svcul));
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcul));
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcul));
        return;
    }

    printf("store(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcul);
    assert(0);
}

static void arm64_gen_bl_or_b(int b)
{
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && vtop->sym) {
	greloca(cur_text_section(), vtop->sym, ind,
                b ? R_AARCH64_JUMP26 :  R_AARCH64_CALL26, 0);
	o(0x14000000 | (uint32_t)!b << 31); // b/bl .
    }
    else {
#ifdef CONFIG_TCC_BCHECK
        vtop->r &= ~VT_MUSTBOUND;
#endif
        gen_load_reg(TREG_R30);
        o(0xd61f0000 | (uint32_t)!b << 21 | intr(TREG_R30) << 5); // br/blr
    }
}


static int arm64_hfa_aux(CType *type, int *fsize, int num)
{
    if (is_float(type->t)) {
        int a, n = size_align_of_type(type->t, &a);
        if (num >= 4 || (*fsize && *fsize != n))
            return -1;
        *fsize = n;
        return num + 1;
    }
    return -1;
}


static unsigned long arm64_pcs_aux(int n, CType **type, unsigned long *a)
{
    int nx = 0; // next integer register
    int nv = 0; // next vector register
    unsigned long ns = 32; // next stack offset
    int i;

    for (i = 0; i < n; i++) {
        int hfa = 0;
        int size, align;

        if ((type[i]->t & VT_TYPE) == VT_FUNC)
            size = align = 8;
        else
            size = size_align_of_type(type[i]->t, &align);

        if (hfa)
            // B.2
            ;
        else if (size > 16) {
            // B.3: replace with pointer
            if (nx < 8)
                a[i] = nx++ << 1 | 1;
            else {
                ns = (ns + 7) & ~7;
                a[i] = ns | 1;
                ns += 8;
            }
            continue;
        }

        // C.1
        if (is_float(type[i]->t) && nv < 8) {
            a[i] = 16 + (nv++ << 1);
            continue;
        }

        // C.2
        if (hfa && nv + hfa <= 8) {
            a[i] = 16 + (nv << 1);
            nv += hfa;
            continue;
        }

        // C.3
        if (hfa) {
            nv = 8;
            size = (size + 7) & ~7;
        }

        // C.4
        if (hfa || (type[i]->t & VT_TYPE) == VT_FLOAT128) {
            ns = (ns + 7) & ~7;
            ns = (ns + align - 1) & -align;
        }

        // C.5
        if ((type[i]->t & VT_TYPE) == VT_FLOAT32)
            size = 8;

        // C.6
        if (hfa || is_float(type[i]->t)) {
            a[i] = ns;
            ns += size;
            continue;
        }

        // C.7
        if (size <= 8 && nx < 8) {
            a[i] = nx++ << 1;
            continue;
        }

        // C.8
        if (align == 16)
            nx = (nx + 1) & ~1;

        // C.9
        if (size == 16 && nx < 7) {
            a[i] = nx << 1;
            nx += 2;
            continue;
        }

        // C.10
        if (size <= (8 - nx) * 8) {
            a[i] = nx << 1;
            nx += (size + 7) >> 3;
            continue;
        }

        // C.11
        nx = 8;

        // C.12
        ns = (ns + 7) & ~7;
        ns = (ns + align - 1) & -align;

        // C.13

        // C.14
        if (size < 8)
            size = 8;

        // C.15
        a[i] = ns;
        ns += size;
    }

    return ns - 32;
}

static unsigned long arm64_pcs(int n, CType **type, unsigned long *a)
{
    unsigned long stack;

    // Return type:
    if ((type[0]->t & VT_TYPE) == VT_VOID)
        a[0] = -1;
    else {
        arm64_pcs_aux(1, type, a);
        assert(a[0] == 0 || a[0] == 1 || a[0] == 16);
    }

    // Argument types:
    stack = arm64_pcs_aux(n, type + 1, a + 1);

    if (0) {
        int i;
        for (i = 0; i <= n; i++) {
            if (!i)
                printf("arm64_pcs return: ");
            else
                printf("arm64_pcs arg %d: ", i);
            if (a[i] == (unsigned long)-1)
                printf("void\n");
            else if (a[i] == 1 && !i)
                printf("X8 pointer\n");
            else if (a[i] < 16)
                printf("X%lu%s\n", a[i] / 2, a[i] & 1 ? " pointer" : "");
            else if (a[i] < 32)
                printf("V%lu\n", a[i] / 2 - 8);
            else
                printf("stack %lu%s\n",
                       (a[i] - 32) & ~1, a[i] & 1 ? " pointer" : "");
        }
    }

    return stack;
}

ST_FUNC void gfunc_call(int nb_args,CType *return_type)
{
    CType **t;
    unsigned long *a, *a1;
    unsigned long stack;
    int i;


    t = tcc_malloc((nb_args + 1) * sizeof(*t));
    a = tcc_malloc((nb_args + 1) * sizeof(*a));
    a1 = tcc_malloc((nb_args + 1) * sizeof(*a1));

    t[0] = return_type;
    for (i = 0; i < nb_args; i++)
        t[nb_args - i] = &vtop[-i].type;

    stack = arm64_pcs(nb_args, t, a);


    stack = (stack + 15) >> 4 << 4;

    /* fetch cpu flag before generating any code */
    if ((vtop->r & VT_VALMASK) == VT_CMP)
      gen_ldr();

    if (stack >= 0x1000000) // 16Mb
        tcc_error("stack size too big");
    if (stack & 0xfff)
        o(0xd10003ff | (stack & 0xfff) << 10); // sub sp,sp,#(n)
    if (stack >> 12)
            o(0xd14003ff | (stack >> 12) << 10);

    // First pass: set all values on stack
    for (i = nb_args; i; i--) {
        vpushv(vtop - nb_args + i);

        if (a[i] >= 32) {
            // value on stack
            if (is_float(vtop->type.t)) {
                gen_ldr();
                arm64_strv(arm64_type_size(vtop[0].type.t),
                           fltr(vtop[0].r), 31, a[i] - 32);
            }
            else {
                gen_ldr();
                arm64_strx(arm64_type_size(vtop[0].type.t),
                           intr(vtop[0].r), 31, a[i] - 32);
            }
        }

        --vtop;
    }

    // Second pass: assign values to registers
    for (i = nb_args; i; i--, vtop--) {
        if (a[i] < 16 && !(a[i] & 1)) {
            // value in general-purpose registers
            gen_load_reg(TREG_R(a[i] / 2));
        }
        else if (a[i] < 16)
            // struct replaced by pointer in register
            arm64_spoff(a[i] / 2, a1[i]);
        else if (a[i] < 32) {
            // value in floating-point registers
            gen_load_reg(TREG_F(a[i] / 2 - 8));
        }
    }

    save_rc_upstack(RC_CALLER_SAVED,0);
    arm64_gen_bl_or_b(0);
    vpop(1);
    if (stack & 0xfff)
        o(0x910003ff | (stack & 0xfff) << 10); // add sp,sp,#(n)
    if (stack >> 12)
        o(0x914003ff | (stack >> 12) << 10);

    tcc_free(a1);
    tcc_free(a);
    tcc_free(t);

    {
        vpushi(0);
        vtop->type = *return_type;
        int bt = return_type->t;
        if (is_integer(bt) && size_align_of_type(bt, NULL) <= 8) {
            vtop->r = TREG_R(0);
            vtop->r2 = VT_CONST;
        } else if (is_same_size_int(bt, VT_INT128)) {
            vtop->r = TREG_R(0);
            vtop->r2 = TREG_R(1);
        } else if (bt == VT_FLOAT32 || bt == VT_FLOAT64) {
            vtop->r = TREG_F(0);
            vtop->r2 = VT_CONST;
        } else {
            tcc_error(TCC_ERROR_UNIMPLEMENTED);
        }
    }
}

static unsigned long arm64_func_va_list_stack;
static int arm64_func_va_list_gr_offs;
static int arm64_func_va_list_vr_offs;
static int arm64_func_sub_sp_offset;
static int func_vc;
ST_FUNC void gfunc_prolog()
{
    int n = 0;
    int i = 0;
    CType **t;
    unsigned long *a;
    int use_x8 = 0;
    int last_int = 0;
    int last_float = 0;
    SValue *sv;

    func_vc = 144; // offset of where x8 is stored

    n=vtop-vstack+2;
    t = n ? tcc_malloc(n * sizeof(*t)) : NULL;
    a = n ? tcc_malloc(n * sizeof(*a)) : NULL;

    //XXX: make a dummy return type, remove it in future.
    CType ct;
    ct.t=VT_INT64;
    t[0]=&ct;
    i++;

    for (sv=vstack;sv<=vtop;sv++)
        t[i++] = &sv->type;

    arm64_func_va_list_stack = arm64_pcs(n - 1, t, a);

    if (a && a[0] == 1)
        use_x8 = 1;
    for (i = 1,sv=vstack; sv<=vtop; i++,sv++) {
        if (a[i] == 1)
	    use_x8 = 1;
        if (a[i] < 16) {
            int last, align, size = size_align_of_type(sv->type.t, &align);
	    last = a[i] / 4 + 1 + (size - 1) / 8;
	    last_int = last > last_int ? last : last_int;
	}
        else if (a[i] < 32) {
            int last, hfa = 0;
            last = a[i] / 4 - 3 + (hfa ? hfa - 1 : 0);
            last_float = last > last_float ? last : last_float;
	    }
    }

    last_int = last_int > 4 ? 4 : last_int;
    last_float = last_float > 4 ? 4 : last_float;

    o(0xa9b27bfd); // stp x29,x30,[sp,#-224]!
    for (i = 0; i < last_float; i++)
        // stp q0,q1,[sp,#16], stp q2,q3,[sp,#48]
        // stp q4,q5,[sp,#80], stp q6,q7,[sp,#112]
        o(0xad0087e0 + i * 0x10000 + (i << 11) + (i << 1));
    if (use_x8)
        o(0xa90923e8); // stp x8,x8,[sp,#144]
    for (i = 0; i < last_int; i++)
        // stp x0,x1,[sp,#160], stp x2,x3,[sp,#176]
        // stp x4,x5,[sp,#192], stp x6,x7,[sp,#208]
        o(0xa90a07e0 + i * 0x10000 + (i << 11) + (i << 1));

    arm64_func_va_list_gr_offs = -64;
    arm64_func_va_list_vr_offs = -128;

    for (i = 1,sv=vstack; sv<=vtop; i++,sv++) {
        int off = (a[i] < 16 ? 160 + a[i] / 2 * 8 :
                   a[i] < 32 ? 16 + (a[i] - 16) / 2 * 16 :
                   224 + ((a[i] - 32) >> 1 << 1));
        sv->r= ((a[i] & 1 ) ? VT_LLOCAL : VT_LOCAL)| VT_LVAL;
        sv->c.i=off;

        if (a[i] < 16) {
            int align, size = size_align_of_type(sv->type.t, &align);
            arm64_func_va_list_gr_offs = (a[i] / 2 - 7 +
                                          (!(a[i] & 1) && size > 8)) * 8;
        }
        else if (a[i] < 32) {
            uint32_t hfa = 0;
            arm64_func_va_list_vr_offs = (a[i] / 2 - 16 +
                                          (hfa ? hfa : 1)) * 16;
        }
    }

    tcc_free(a);
    tcc_free(t);

    o(0x910003fd); // mov x29,sp
    arm64_func_sub_sp_offset = ind;
    // In gfunc_epilog these will be replaced with code to decrement SP:
    o(0xd503201f); // nop
    o(0xd503201f); // nop
    loc = 0;

    vpushi(0);
    vtop->r=TREG_R30;
    vtop->r2=VT_CONST;
}


ST_FUNC void gfunc_return(CType *func_type)
{
    CType *t = func_type;
    unsigned long a;

    arm64_pcs(0, &t, &a);
    switch (a) {
    case -1:
        break;
    case 0:
        gen_load_reg(TREG_R(0));
        break;
    case 16:
        gen_load_reg(TREG_F(0));
        break;
    default:
      assert(0);
    }
    vpop(1);
}

ST_FUNC void gfunc_epilog(void)
{
    gfunc_return(&vtop->type);
    if (loc) {
        // Insert instructions to subtract size of stack frame from SP.
        unsigned char *ptr = cur_text_section()->data + arm64_func_sub_sp_offset;
        uint64_t diff = (-loc + 15) & ~15;
        if (!(diff >> 24)) {
            if (diff & 0xfff) // sub sp,sp,#(diff & 0xfff)
                write32le(ptr, 0xd10003ff | (diff & 0xfff) << 10);
            if (diff >> 12) // sub sp,sp,#(diff >> 12),lsl #12
                write32le(ptr + 4, 0xd14003ff | (diff >> 12) << 10);
        }
        else {
            // In this case we may subtract more than necessary,
            // but always less than 17/16 of what we were aiming for.
            int i = 0;
            int j = 0;
            while (diff >> 20) {
                diff = (diff + 0xffff) >> 16;
                ++i;
            }
            while (diff >> 16) {
                diff = (diff + 1) >> 1;
                ++j;
            }
            write32le(ptr, 0xd2800010 | diff << 5 | i << 21);
            // mov x16,#(diff),lsl #(16 * i)
            write32le(ptr + 4, 0xcb3063ff | j << 10);
            // sub sp,sp,x16,lsl #(j)
        }
    }
    o(0x910003bf); // mov sp,x29
    o(0xa8ce7bfd); // ldp x29,x30,[sp],#224

    o(0xd65f03c0); // ret
}

ST_FUNC void gen_fill_nops(int bytes)
{
    if ((bytes & 3))
      tcc_error("alignment of code section not multiple of 4");
    while (bytes > 0) {
	o(0xd503201f); // nop
	bytes -= 4;
    }
}

// Generate forward branch to label:
ST_FUNC int gjmp(int t)
{
    int r = ind;
    o(t);
    return r;
}

// Generate branch to known address:
ST_FUNC void gjmp_addr(int a)
{
    assert(a - ind + 0x8000000 < 0x10000000);
    o(0x14000000 | ((a - ind) >> 2 & 0x3ffffff));
}


static void arm64_vset_VT_CMP(int op)
{
    if (op >= TOK_ULT && op <= TOK_GT) {
        vtop->c.cmp_r = vtop->r;
        vtop->r = VT_CMP;
        vtop->c.cmp_op = 0x80;
    }
}

static void arm64_gen_opil(int op, uint32_t l);

static void arm64_load_cmp(int r, SValue *sv)
{
    sv->r = sv->c.cmp_r;
    if (sv->c.cmp_op & 1) {
        vpushi(1);
        arm64_gen_opil(TOK_XOR, 0);
    }
    if (r != sv->r) {
        load(r, sv);
        sv->r = r;
    }
}

ST_FUNC int gtst(int inv, int t)
{
    int bt = vtop->type.t & VT_TYPE;

    vtop->r = vtop->c.cmp_r;

    uint32_t ll = (bt == VT_INT64);
    uint32_t a = intr(gen_ldr());
    if(inv){
        o(0x34000040 | a | 1 << 24 | ll << 31); // cbz/cbnz wA,.+8
    }else{
        o(0x34000040 | a | ll << 31); // cbz/cbnz wA,.+8
    }
    
    return gjmp(t);
}

static int arm64_iconst(uint64_t *val, SValue *sv)
{
    if ((sv->r & (VT_VALMASK | VT_LVAL)) != VT_CONST && !sv->sym)
        return 0;
    if (val) {
        int t = sv->type.t;
	int bt = t & VT_TYPE;
        *val = ((bt == VT_INT64) ? sv->c.i :
                (uint32_t)sv->c.i |
                (t & VT_UNSIGNED ? 0 : -(sv->c.i & 0x80000000)));
    }
    return 1;
}

static int arm64_gen_opic(int op, uint32_t l, int rev, uint64_t val,
                          uint32_t x, uint32_t a)
{
    if (op == TOK_SUB && !rev) {
        val = -val;
        op = TOK_ADD;
    }
    val = l ? val : (uint32_t)val;

    switch (op) {

    case TOK_ADD: {
        uint32_t s = l ? val >> 63 : val >> 31;
        val = s ? -val : val;
        val = l ? val : (uint32_t)val;
        if (!(val & ~(uint64_t)0xfff))
            o(0x11000000 | l << 31 | s << 30 | x | a << 5 | val << 10);
        else if (!(val & ~(uint64_t)0xfff000))
            o(0x11400000 | l << 31 | s << 30 | x | a << 5 | val >> 12 << 10);
        else {
            arm64_movimm(30, val); // use x30
            o(0x0b1e0000 | l << 31 | s << 30 | x | a << 5);
        }
        return 1;
      }

    case TOK_SUB:
        if (!val)
            o(0x4b0003e0 | l << 31 | x | a << 16); // neg
        else if (val == (l ? (uint64_t)-1 : (uint32_t)-1))
            o(0x2a2003e0 | l << 31 | x | a << 16); // mvn
        else {
            arm64_movimm(30, val); // use x30
            o(0x4b0003c0 | l << 31 | x | a << 16); // sub
        }
        return 1;

    case TOK_XOR:
        if (val == -1 || (val == 0xffffffff && !l)) {
            o(0x2a2003e0 | l << 31 | x | a << 16); // mvn
            return 1;
        }
        // fall through
    case TOK_AND:
    case TOK_OR: {
        int e = arm64_encode_bimm64(l ? val : val | val << 32);
        if (e < 0)
            return 0;
        o((op == TOK_AND ? 0x12000000 :
           op == TOK_OR ? 0x32000000 : 0x52000000) |
          l << 31 | x | a << 5 | (uint32_t)e << 10);
        return 1;
    }

    case TOK_SAR:
    case TOK_SHL:
    case TOK_SHR: {
        uint32_t n = 32 << l;
        val = val & (n - 1);
        if (rev)
            return 0;
        if (!val) {
            // tcc_warning("shift count >= width of type");
            o(0x2a0003e0 | l << 31 | a << 16);
            return 1;
        }
        else if (op == TOK_SHL)
            o(0x53000000 | l << 31 | l << 22 | x | a << 5 |
              (n - val) << 16 | (n - 1 - val) << 10); // lsl
        else
            o(0x13000000 | (op == TOK_SHR) << 30 | l << 31 | l << 22 |
              x | a << 5 | val << 16 | (n - 1) << 10); // lsr/asr
        return 1;
    }

    }
    return 0;
}

static void arm64_gen_opil(int op, uint32_t l)
{
    uint32_t x, a, b;

    // Special treatment for operations with a constant operand:
    {
        uint64_t val;
        int rev = 1;

        if (arm64_iconst(0, &vtop[0])) {
            vswap();
            rev = 0;
        }
        if (arm64_iconst(&val, &vtop[-1])) {
            gen_ldr();
            a = intr(vtop[0].r);
            --vtop;
            x = get_reg_of_cls(RC_INT);
            ++vtop;
            if (arm64_gen_opic(op, l, rev, val, intr(x), a)) {
                vtop[0].r = x;
                vswap();
                --vtop;
                return;
            }
        }
        if (!rev)
            vswap();
    }
    gen_ldr();
    vswap();
    gen_ldr();
    vswap();
    assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
    a = intr(vtop[-1].r);
    b = intr(vtop[0].r);
    vtop -= 2;
    x = get_reg_of_cls(RC_INT);
    ++vtop;
    vtop[0].r = x;
    x = intr(x);

    switch (op) {
    case TOK_MOD:
        // Use x30 for quotient:
        o(0x1ac00c00 | l << 31 | 30 | a << 5 | b << 16); // sdiv
        o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
          b << 16 | a << 10); // msub
        break;
    case TOK_AND:
        o(0x0a000000 | l << 31 | x | a << 5 | b << 16); // and
        break;
    case TOK_MULL:
        o(0x1b007c00 | l << 31 | x | a << 5 | b << 16); // mul
        break;
    case TOK_ADD:
        o(0x0b000000 | l << 31 | x | a << 5 | b << 16); // add
        break;
    case TOK_SUB:
        o(0x4b000000 | l << 31 | x | a << 5 | b << 16); // sub
        break;
    case TOK_DIV:
        o(0x1ac00c00 | l << 31 | x | a << 5 | b << 16); // sdiv
        break;
    case TOK_XOR:
        o(0x4a000000 | l << 31 | x | a << 5 | b << 16); // eor
        break;
    case TOK_OR:
        o(0x2a000000 | l << 31 | x | a << 5 | b << 16); // orr
        break;
    case TOK_EQ:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f17e0 | x); // cset wA,eq
        break;
    case TOK_GE:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9fb7e0 | x); // cset wA,ge
        break;
    case TOK_GT:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9fd7e0 | x); // cset wA,gt
        break;
    case TOK_LE:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9fc7e0 | x); // cset wA,le
        break;
    case TOK_LT:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9fa7e0 | x); // cset wA,lt
        break;
    case TOK_NE:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f07e0 | x); // cset wA,ne
        break;
    case TOK_SAR:
        o(0x1ac02800 | l << 31 | x | a << 5 | b << 16); // asr
        break;
    case TOK_SHL:
        o(0x1ac02000 | l << 31 | x | a << 5 | b << 16); // lsl
        break;
    case TOK_SHR:
        o(0x1ac02400 | l << 31 | x | a << 5 | b << 16); // lsr
        break;
    case TOK_UDIV:
        o(0x1ac00800 | l << 31 | x | a << 5 | b << 16); // udiv
        break;
    case TOK_UGE:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f37e0 | x); // cset wA,cs
        break;
    case TOK_UGT:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f97e0 | x); // cset wA,hi
        break;
    case TOK_ULT:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f27e0 | x); // cset wA,cc
        break;
    case TOK_ULE:
        o(0x6b00001f | l << 31 | a << 5 | b << 16); // cmp
        o(0x1a9f87e0 | x); // cset wA,ls
        break;
    case TOK_UMOD:
        // Use x30 for quotient:
        o(0x1ac00800 | l << 31 | 30 | a << 5 | b << 16); // udiv
        o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
          b << 16 | a << 10); // msub
        break;
    default:
        assert(0);
    }
}

ST_FUNC void gen_opi(int op)
{
    arm64_gen_opil(op, 0);
    arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opl(int op)
{
    arm64_gen_opil(op, 1);
    arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opf(int op)
{
    uint32_t x, a, b, dbl;


    dbl = vtop[0].type.t != VT_FLOAT32;
    gen_ldr();
    vswap();
    gen_ldr();
    vswap();
    assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
    a = fltr(vtop[-1].r);
    b = fltr(vtop[0].r);
    vtop -= 2;
    switch (op) {
    case TOK_EQ: case TOK_NE:
    case TOK_LT: case TOK_GE: case TOK_LE: case TOK_GT:
        x = get_reg_of_cls(RC_INT);
        ++vtop;
        vtop[0].r = x;
        x = intr(x);
        break;
    default:
        x = get_reg_of_cls(RC_FLOAT);
        ++vtop;
        vtop[0].r = x;
        x = fltr(x);
        break;
    }

    switch (op) {
    case TOK_MULL:
        o(0x1e200800 | dbl << 22 | x | a << 5 | b << 16); // fmul
        break;
    case TOK_ADD:
        o(0x1e202800 | dbl << 22 | x | a << 5 | b << 16); // fadd
        break;
    case TOK_SUB:
        o(0x1e203800 | dbl << 22 | x | a << 5 | b << 16); // fsub
        break;
    case TOK_DIV:
        o(0x1e201800 | dbl << 22 | x | a << 5 | b << 16); // fdiv
        break;
    case TOK_EQ:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9f17e0 | x); // cset w(x),eq
        break;
    case TOK_GE:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9fb7e0 | x); // cset w(x),ge
        break;
    case TOK_GT:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9fd7e0 | x); // cset w(x),gt
        break;
    case TOK_LE:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9f87e0 | x); // cset w(x),ls
        break;
    case TOK_LT:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9f57e0 | x); // cset w(x),mi
        break;
    case TOK_NE:
        o(0x1e202000 | dbl << 22 | a << 5 | b << 16); // fcmp
        o(0x1a9f07e0 | x); // cset w(x),ne
        break;
    default:
        assert(0);
    }
    arm64_vset_VT_CMP(op);
}

// Generate sign extension from 32 to 64 bits:
ST_FUNC void gen_cvt_sxtw(void)
{
    uint32_t r = intr(gen_ldr());
    o(0x93407c00 | r | r << 5); // sxtw x(r),w(r)
}

/* char/short to int conversion */
ST_FUNC void gen_cvt_csti(int t)
{
    int r = intr(gen_ldr());
    o(0x13001c00
        | ((t & VT_TYPE) == VT_INT16) << 13
        | (uint32_t)!!(t & VT_UNSIGNED) << 30
        | r | r << 5); // [su]xt[bh] w(r),w(r)
}

ST_FUNC void gen_cvt_itof(int t)
{
    
    int d, n = intr(gen_ldr());
    int s = !(vtop->type.t & VT_UNSIGNED);
    uint32_t l = ((vtop->type.t & VT_TYPE) == VT_INT64);
    --vtop;
    d = get_reg_of_cls(RC_FLOAT);
    ++vtop;
    vtop[0].r = d;
    o(0x1e220000 | (uint32_t)!s << 16 |
        (uint32_t)(t != VT_FLOAT32) << 22 | fltr(d) |
        l << 31 | n << 5); // [us]cvtf [sd](d),[wx](n)
    
}

ST_FUNC void gen_cvt_ftoi(int t)
{
    int d, n = fltr(gen_ldr());
    uint32_t l = ((vtop->type.t & VT_TYPE) != VT_FLOAT32);
    --vtop;
    d = get_reg_of_cls(RC_INT);
    ++vtop;
    vtop[0].r = d;
    o(0x1e380000 |
        (uint32_t)!!(t & VT_UNSIGNED) << 16 |
        (uint32_t)((t & VT_TYPE) == VT_INT64) << 31 | intr(d) |
        l << 22 | n << 5); // fcvtz[su] [wx](d),[sd](n)
}

ST_FUNC void gen_cvt_ftof(int t)
{
    int f = vtop[0].type.t & VT_TYPE;
    assert(t == VT_FLOAT32 || t == VT_FLOAT64 || t == VT_FLOAT128);
    assert(f == VT_FLOAT32 || f == VT_FLOAT64 || f == VT_FLOAT128);
    if (t == f)
        return;

    int x, a;
    gen_ldr();
    assert(vtop[0].r < VT_CONST);
    a = fltr(vtop[0].r);
    --vtop;
    x = get_reg_of_cls(RC_FLOAT);
    ++vtop;
    vtop[0].r = x;
    x = fltr(x);

    if (f == VT_FLOAT32)
        o(0x1e22c000 | x | a << 5); // fcvt d(x),s(a)
    else
        o(0x1e624000 | x | a << 5); // fcvt s(x),d(a)
}


ST_FUNC void ggoto(void)
{
    arm64_gen_bl_or_b(1);
    --vtop;
}

ST_FUNC void gen_clear_cache(void)
{
    uint32_t beg, end, dsz, isz, p, lab1, b1;
    gen_ldr();
    vswap();
    gen_ldr();
    vswap();
    vpushi(0);
    vtop->r = get_reg_of_cls(RC_INT);
    vpushi(0);
    vtop->r = get_reg_of_cls(RC_INT);
    vpushi(0);
    vtop->r = get_reg_of_cls(RC_INT);
    beg = intr(vtop[-4].r); // x0
    end = intr(vtop[-3].r); // x1
    dsz = intr(vtop[-2].r); // x2
    isz = intr(vtop[-1].r); // x3
    p = intr(vtop[0].r);    // x4
    vtop -= 5;

    o(0xd53b0020 | isz); // mrs x(isz),ctr_el0
    o(0x52800080 | p); // mov w(p),#4
    o(0x53104c00 | dsz | isz << 5); // ubfx w(dsz),w(isz),#16,#4
    o(0x1ac02000 | dsz | p << 5 | dsz << 16); // lsl w(dsz),w(p),w(dsz)
    o(0x12000c00 | isz | isz << 5); // and w(isz),w(isz),#15
    o(0x1ac02000 | isz | p << 5 | isz << 16); // lsl w(isz),w(p),w(isz)
    o(0x51000400 | p | dsz << 5); // sub w(p),w(dsz),#1
    o(0x8a240004 | p | beg << 5 | p << 16); // bic x(p),x(beg),x(p)
    b1 = ind; o(0x14000000); // b
    lab1 = ind;
    o(0xd50b7b20 | p); // dc cvau,x(p)
    o(0x8b000000 | p | p << 5 | dsz << 16); // add x(p),x(p),x(dsz)
    write32le(cur_text_section()->data + b1, 0x14000000 | (ind - b1) >> 2);
    o(0xeb00001f | p << 5 | end << 16); // cmp x(p),x(end)
    o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0)); // b.cc lab1
    o(0xd5033b9f); // dsb ish
    o(0x51000400 | p | isz << 5); // sub w(p),w(isz),#1
    o(0x8a240004 | p | beg << 5 | p << 16); // bic x(p),x(beg),x(p)
    b1 = ind; o(0x14000000); // b
    lab1 = ind;
    o(0xd50b7520 | p); // ic ivau,x(p)
    o(0x8b000000 | p | p << 5 | isz << 16); // add x(p),x(p),x(isz)
    write32le(cur_text_section()->data + b1, 0x14000000 | (ind - b1) >> 2);
    o(0xeb00001f | p << 5 | end << 16); // cmp x(p),x(end)
    o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0)); // b.cc lab1
    o(0xd5033b9f); // dsb ish
    o(0xd5033fdf); // isb
}

ST_FUNC void gen_vla_sp_save(int addr) {
    uint32_t r = intr(get_reg_of_cls(RC_INT));
    o(0x910003e0 | r); // mov x(r),sp
    arm64_strx(3, r, 29, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) {
    // Use x30 because this function can be called when there
    // is a live return value in x0 but there is nothing on
    // the value stack to prevent get_reg_of_cls from returning x0.
    uint32_t r = 30;
    arm64_ldrx(0, 3, r, 29, addr);
    o(0x9100001f | r << 5); // mov sp,x(r)
}

ST_FUNC void gen_vla_alloc(CType *type, int align) {
    uint32_t r;
    r = intr(gen_ldr());
    o(0x91003c00 | r | r << 5); // add x(r),x(r),#15
    o(0x927cec00 | r | r << 5); // bic x(r),x(r),#15
    o(0xcb2063ff | r << 16); // sub sp,sp,x(r)
    vpop(1);
}

/* end of A64 code generator */
/*************************************************************/
