
#include "tccelf.h"
#include "xxx-gen.h"
#include "arm64-gen.h"
#include "tccutils.h"
#include "tccdef.h"

void relocate_init(Section *sr) {}
/* Returns 1 for a code relocation, 0 for a data relocation. For unknown
   relocations, returns -1. */
int code_reloc (int reloc_type)
{
    switch (reloc_type) {
        case R_AARCH64_ABS32:
        case R_AARCH64_ABS64:
	case R_AARCH64_PREL32:
        case R_AARCH64_MOVW_UABS_G0_NC:
        case R_AARCH64_MOVW_UABS_G1_NC:
        case R_AARCH64_MOVW_UABS_G2_NC:
        case R_AARCH64_MOVW_UABS_G3:
        case R_AARCH64_ADR_PREL_PG_HI21:
        case R_AARCH64_ADD_ABS_LO12_NC:
        case R_AARCH64_ADR_GOT_PAGE:
        case R_AARCH64_LD64_GOT_LO12_NC:
        case R_AARCH64_LDST128_ABS_LO12_NC:
        case R_AARCH64_LDST64_ABS_LO12_NC:
        case R_AARCH64_LDST32_ABS_LO12_NC:
        case R_AARCH64_LDST16_ABS_LO12_NC:
        case R_AARCH64_LDST8_ABS_LO12_NC:
        case R_AARCH64_GLOB_DAT:
        case R_AARCH64_COPY:
            return 0;

        case R_AARCH64_JUMP26:
        case R_AARCH64_CALL26:
        case R_AARCH64_JUMP_SLOT:
            return 1;
    }
    return -1;
}


void relocate( ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{
    int sym_index = ELFW(R_SYM)(rel->r_info), esym_index;

    switch(type) {
        case R_AARCH64_ABS64:
            add64le(ptr, val);
            return;
        case R_AARCH64_ABS32:
            add32le(ptr, val);
            return;
	case R_AARCH64_PREL32:
	    write32le(ptr, val - addr);
	    return;
        case R_AARCH64_MOVW_UABS_G0_NC:
            write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
                            (val & 0xffff) << 5));
            return;
        case R_AARCH64_MOVW_UABS_G1_NC:
            write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
                            (val >> 16 & 0xffff) << 5));
            return;
        case R_AARCH64_MOVW_UABS_G2_NC:
            write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
                            (val >> 32 & 0xffff) << 5));
            return;
        case R_AARCH64_MOVW_UABS_G3:
            write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
                            (val >> 48 & 0xffff) << 5));
            return;
        case R_AARCH64_ADR_PREL_PG_HI21: {
            uint64_t off = (val >> 12) - (addr >> 12);
            if ((off + ((uint64_t)1 << 20)) >> 21)
                tcc_error("R_AARCH64_ADR_PREL_PG_HI21 relocation failed");
            write32le(ptr, ((read32le(ptr) & 0x9f00001f) |
                            (off & 0x1ffffc) << 3 | (off & 3) << 29));
            return;
        }
        case R_AARCH64_ADD_ABS_LO12_NC:
        case R_AARCH64_LDST8_ABS_LO12_NC:
            write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
                            (val & 0xfff) << 10));
            return;
        case R_AARCH64_LDST16_ABS_LO12_NC:
            write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
                            (val & 0xffe) << 9));
            return;
        case R_AARCH64_LDST32_ABS_LO12_NC:
            write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
                            (val & 0xffc) << 8));
            return;
        case R_AARCH64_LDST64_ABS_LO12_NC:
            write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
                            (val & 0xff8) << 7));
            return;
        case R_AARCH64_LDST128_ABS_LO12_NC:
            write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
                            (val & 0xff0) << 6));
            return;
        case R_AARCH64_JUMP26:
        case R_AARCH64_CALL26:
            if (((val - addr) + ((uint64_t)1 << 27)) & ~(uint64_t)0xffffffc)
                tcc_error("R_AARCH64_(JUMP|CALL)26 relocation failed");
            write32le(ptr, (0x14000000 |
                            (uint32_t)(type == R_AARCH64_CALL26) << 31 |
                            ((val - addr) >> 2 & 0x3ffffff)));
            return;
        case R_AARCH64_COPY:
            return;
        case R_AARCH64_GLOB_DAT:
        case R_AARCH64_JUMP_SLOT:
            /* They don't need addend */
            write64le(ptr, val - rel->r_addend);
            return;
        case R_AARCH64_RELATIVE:
            /* do nothing */
            return;
        default:
            fprintf(stderr, "FIXME: handle reloc type %x at %x [%p] to %x\n",
                    type, (unsigned)addr, ptr, (unsigned)val);
            return;
    }
}

