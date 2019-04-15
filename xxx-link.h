#ifndef _XXX_LINK_H
#define _XXX_LINK_H

#include "tccdef.h"
#include <stdint.h>




#define TCC_OUTPUT_FORMAT_BINARY 1
extern uint32_t output_format;

#ifdef TCC_TARGET_I386
#include "i386-link.h"
#endif

#ifdef TCC_TARGET_ARM
#include "arm-link.h"
#endif

/*------------arch-link.c---------------*/
ST_FUNC int code_reloc (int reloc_type);
ST_FUNC void relocate_init(Section *sr);
ST_FUNC void relocate(ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val);

#endif