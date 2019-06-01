
#ifndef _TCCDEF_H
#define _TCCDEF_H 1

#include <stdint.h>

#include "config.h"
#include "elf.h"

#define ST_FUNC extern
#define ST_DATA extern
#define LIBTCCAPI extern


typedef uint32_t FLAG_GROUP;



/* types */
#define VT_TYPE      0xffff
#define VT_VOID           0  /* void type */
#define VT_INT8        0x01  /* signed byte type */
#define VT_INT16       0x02  /* short type */
#define VT_INT32       0x03  /* integer type */
#define VT_INT64       0x04  /* 64 bit integer */
#define VT_INT128      0x05
#define VT_INTLAST      0x07  /* last int type */

#define VT_UNSIGNED    0x08  /* flag for unsigned int */


#define VT_FLOAT32     0x10  /* IEEE float */
#define VT_FLOAT64     0x11  /* IEEE double */
#define VT_FLOAT128    0x12
#define VT_FLOATLAST   0x15  /* last float type */

#define is_float(a) (a >= VT_FLOAT32) && (a <= VT_FLOATLAST)
#define is_integer(a) !(a&0xfff0)
#define VT_FUNC        0x16  /* function type */

#define is_same_size_int(vt,ct) is_integer(vt) && (( vt | VT_UNSIGNED)==( ct | VT_UNSIGNED))





#ifdef TCC_TARGET_I386
#include "i386-gen.h"
#endif

#ifdef TCC_TARGET_ARM
#include "arm-gen.h"
#endif

#ifdef TCC_TARGET_X86_64
#include "x86_64-gen.h"
#endif

#if PTR_SIZE == 8
# define ELFCLASSW ELFCLASS64
# define ElfW(type) Elf##64##_##type
# define ELFW(type) ELF##64##_##type
# define ElfW_Rel ElfW(Rela)
# define SHT_RELX SHT_RELA
# define REL_SECTION_FMT ".rela%s"
#elif PTR_SIZE == 4
# define ELFCLASSW ELFCLASS32
# define ElfW(type) Elf##32##_##type
# define ELFW(type) ELF##32##_##type
# define ElfW_Rel ElfW(Rel)
# define SHT_RELX SHT_REL
# define REL_SECTION_FMT ".rel%s"
#endif

#define addr_t ElfW(Addr)





/* type definition */
typedef struct CType {
    uint8_t t;
} CType;


#define SYM_SCOPE_LOCAL 0
#define SYM_SCOPE_GLOBAL 1
typedef struct BaseSymAttr{
	uint16_t
	aligned     : 5, /* alignment as log2+1 (0 == unspecified) */
	scope	    : 1,
	unsued		: 11;
} BaseSymAttr;

typedef struct Sym {
    char *name; /* symbol name */
	int c; /* associated number or Elf symbol index */
    CType type; /* associated type */
    struct BaseSymAttr a; /* symbol attributes */
} Sym;


#endif