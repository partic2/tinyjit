#include "elf.h"
#include "tccdef.h"
#include "arm-link.h"
#include "tccelf.h"

/* Returns 1 for a code relocation, 0 for a data relocation. For unknown
   relocations, returns -1. */
int code_reloc (int reloc_type)
{
    switch (reloc_type) {
	case R_ARM_MOVT_ABS:
	case R_ARM_MOVW_ABS_NC:
	case R_ARM_THM_MOVT_ABS:
	case R_ARM_THM_MOVW_ABS_NC:
	case R_ARM_ABS32:
	case R_ARM_REL32:
	case R_ARM_GOTPC:
	case R_ARM_GOTOFF:
	case R_ARM_GOT32:
	case R_ARM_COPY:
	case R_ARM_GLOB_DAT:
	case R_ARM_NONE:
            return 0;

        case R_ARM_PC24:
        case R_ARM_CALL:
	case R_ARM_JUMP24:
	case R_ARM_PLT32:
	case R_ARM_THM_PC22:
	case R_ARM_THM_JUMP24:
	case R_ARM_PREL31:
	case R_ARM_V4BX:
	case R_ARM_JUMP_SLOT:
            return 1;
    }

    tcc_error ("Unknown relocation type: %d", reloc_type);
    return -1;
}



void relocate_init(Section *sr) {}

void relocate(ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{

    int sym_index;

    sym_index = ELFW(R_SYM)(rel->r_info);
    


    switch(type) {
        case R_ARM_PC24:
        case R_ARM_CALL:
        case R_ARM_JUMP24:
        case R_ARM_PLT32:
            {
                int x, is_thumb, is_call, h, blx_avail, is_bl, th_ko;
                x = (*(int *) ptr) & 0xffffff;
#ifdef DEBUG_RELOC
		printf ("reloc %d: x=0x%x val=0x%x ", type, x, val);
#endif
                (*(int *)ptr) &= 0xff000000;
                if (x & 0x800000)
                    x -= 0x1000000;
                x <<= 2;
                blx_avail = (TCC_CPU_VERSION >= 5);
                is_thumb = val & 1;
                is_bl = (*(unsigned *) ptr) >> 24 == 0xeb;
                is_call = (type == R_ARM_CALL || (type == R_ARM_PC24 && is_bl));
                x += val - addr;
                
                h = x & 2;
                th_ko = (x & 3) && (!blx_avail || !is_call);
                if (th_ko || x >= 0x2000000 || x < -0x2000000)
                    tcc_error("can't relocate value at %x,%d",addr, type);
                x >>= 2;
                x &= 0xffffff;
                /* Only reached if blx is avail and it is a call */
                if (is_thumb) {
                    x |= h << 24;
                    (*(int *)ptr) = 0xfa << 24; /* bl -> blx */
                }
                (*(int *) ptr) |= x;
            }
            return;
        
        case R_ARM_MOVT_ABS:
        case R_ARM_MOVW_ABS_NC:
            {
                int x, imm4, imm12;
                if (type == R_ARM_MOVT_ABS)
                    val >>= 16;
                imm12 = val & 0xfff;
                imm4 = (val >> 12) & 0xf;
                x = (imm4 << 16) | imm12;
                if (type == R_ARM_THM_MOVT_ABS)
                    *(int *)ptr |= x;
                else
                    *(int *)ptr += x;
            }
            return;
        case R_ARM_THM_MOVT_ABS:
        case R_ARM_THM_MOVW_ABS_NC:
            {
                int x, i, imm4, imm3, imm8;
                if (type == R_ARM_THM_MOVT_ABS)
                    val >>= 16;
                imm8 = val & 0xff;
                imm3 = (val >> 8) & 0x7;
                i = (val >> 11) & 1;
                imm4 = (val >> 12) & 0xf;
                x = (imm3 << 28) | (imm8 << 16) | (i << 10) | imm4;
                if (type == R_ARM_THM_MOVT_ABS)
                    *(int *)ptr |= x;
                else
                    *(int *)ptr += x;
            }
            return;
        case R_ARM_PREL31:
            {
                int x;
                x = (*(int *)ptr) & 0x7fffffff;
                (*(int *)ptr) &= 0x80000000;
                x = (x * 2) / 2;
                x += val - addr;
                if((x^(x>>1))&0x40000000)
                    tcc_error("can't relocate value at %x,%d",addr, type);
                (*(int *)ptr) |= x & 0x7fffffff;
            }
        case R_ARM_ABS32:
            *(int *)ptr += val;
            return;
        case R_ARM_REL32:
            *(int *)ptr += val - addr;
            return;
        case R_ARM_COPY:
            return;
        case R_ARM_V4BX:
            /* trade Thumb support for ARMv4 support */
            if ((0x0ffffff0 & *(int*)ptr) == 0x012FFF10)
                *(int*)ptr ^= 0xE12FFF10 ^ 0xE1A0F000; /* BX Rm -> MOV PC, Rm */
            return;
        case R_ARM_GLOB_DAT:
        case R_ARM_JUMP_SLOT:
            *(addr_t *)ptr = val;
            return;
        case R_ARM_NONE:
            /* Nothing to do.  Normally used to indicate a dependency
               on a certain symbol (like for exception handling under EABI).  */
            return;
        case R_ARM_RELATIVE:
            /* do nothing */
            return;
        default:
            fprintf(stderr,"FIXME: handle reloc type %x at %x [%p] to %x\n",
                type, (unsigned)addr, ptr, (unsigned)val);
            return;
    }
}

