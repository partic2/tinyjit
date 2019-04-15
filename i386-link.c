

#include "tccdef.h"
#include "tccelf.h"
#include "elf.h"
#include "xxx-link.h"
#include "tccutils.h"

static ElfW_Rel *qrel;

uint32_t output_format;
/* Returns 1 for a code relocation, 0 for a data relocation. For unknown
   relocations, returns -1. */
int code_reloc (int reloc_type)
{
    switch (reloc_type) {
	case R_386_RELATIVE:
	case R_386_16:
        case R_386_32:
	case R_386_GOTPC:
	case R_386_GOTOFF:
	case R_386_GOT32:
	case R_386_GOT32X:
	case R_386_GLOB_DAT:
	case R_386_COPY:
            return 0;

	case R_386_PC16:
	case R_386_PC32:
	case R_386_PLT32:
	case R_386_JMP_SLOT:
            return 1;
    }

    tcc_error ("Unknown relocation type");
    return -1;
}

void relocate_init(Section *sr)
{
    qrel = (ElfW_Rel *) sr->data;
}

void relocate(ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{
    int sym_index, esym_index;

    sym_index = ELFW(R_SYM)(rel->r_info);

    switch (type) {
        case R_386_32:
            add32le(ptr, val);
            return;
        case R_386_PC32:
            add32le(ptr, val - addr);
            return;
        case R_386_PLT32:
            add32le(ptr, val - addr);
            return;
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT:
            write32le(ptr, val);
            return;
        case R_386_16:
            if (output_format != TCC_OUTPUT_FORMAT_BINARY) {
            output_file:
                tcc_error("can only produce 16-bit binary files");
            }
            write16le(ptr, read16le(ptr) + val);
            return;
        case R_386_PC16:
            if (output_format != TCC_OUTPUT_FORMAT_BINARY)
                goto output_file;
            write16le(ptr, read16le(ptr) + val - addr);
            return;
        case R_386_RELATIVE:
            /* XXX:do nothing now.*/
            return;
        case R_386_COPY:
            /* This relocation must copy initialized data from the library
            to the program .bss segment. Currently made like for ARM
            (to remove noise of default case). Is this true?
            */
            return;
        default:
            tcc_error("unhandle reloc type");
            return;
    }
}

