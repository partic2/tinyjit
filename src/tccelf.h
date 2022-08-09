#ifndef _TCCELF_H
#define _TCCELF_H 1


#include "tccdef.h"
#include "elf.h"
#include <stdio.h>

/* section definition */
typedef struct Section {
    unsigned long data_offset; /* current data offset */
    unsigned char *data;       /* section data */
    unsigned long data_allocated; /* used for realloc() handling */
    int sh_name;             /* elf section name (only used during output) */
    int sh_num;              /* elf section number */
    int sh_type;             /* elf section type */
    int sh_flags;            /* elf section flags */
    int sh_info;             /* elf section info */
    int sh_addralign;        /* elf section alignment */
    int sh_entsize;          /* elf entry size */
    unsigned long sh_size;   /* section size (only used during output) */
    addr_t sh_addr;          /* address at which the section is relocated */
    unsigned long sh_offset; /* file offset */
    int nb_hashed_syms;      /* used to resize the hash table */
    struct Section *link;    /* link to another section */
    struct Section *reloc;   /* corresponding section for relocation, if any */
    struct Section *hash;    /* hash table for symbols */
    struct Section *prev;    /* previous section on section stack */
    char name[1];           /* section name */
} Section;


ST_FUNC void section_realloc(Section *sec,unsigned long size);

ST_FUNC Section *cur_text_section();

/* add a new relocation entry to symbol 'sym' in section 's' */
ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,addr_t addend);

ST_FUNC int put_elf_sym(Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);


ST_FUNC void tccelf_new();

ST_FUNC void tccelf_delete();

ST_FUNC Section *new_section(const char *name, int sh_type, int sh_flags);

ST_FUNC void resolve_common_syms();

ST_FUNC Section *new_symtab(const char *symtab_name, int sh_type, int sh_flags,
                           const char *strtab_name,
                           const char *hash_name, int hash_sh_flags);

ST_FUNC void relocate_section(Section *s);


/* reserve at least 'size' bytes aligned per 'align' in section
   'sec' from current offset, and return the aligned offset */
ST_FUNC size_t section_add(Section *sec, addr_t size, int align);

/* apply storage attributes to Elf symbol */
ST_FUNC void update_storage(Sym *sym);


/* ------------------------------------------------------------------------- */
/* update sym->c so that it points to an external symbol in section
   'section' with value 'value' */
ST_FUNC void put_extern_sym2(Sym *sym, int sh_num,
                            addr_t value, unsigned long size);

ST_FUNC void put_extern_sym(Sym *sym, Section *section,
                           addr_t value, unsigned long size);

ST_FUNC int find_elf_sym(Section *s, const char *name);

/* put relocation */
ST_FUNC void put_elf_reloca(Section *symtab, Section *s, unsigned long offset,
                            int type, int symbol, addr_t addend);

/* return the symbol number */
ST_FUNC int put_elf_sym(Section *s, addr_t value, unsigned long size,int info, int other, int shndx, const char *name);
ST_FUNC int set_elf_sym(Section *s, addr_t value, unsigned long size,int info, int other, int shndx, const char *name);


ST_FUNC int put_elf_str(Section *s, const char *sym);

ST_FUNC int put_elf_str(Section *s, const char *sym);

ST_FUNC struct sym_attr *get_sym_attr(int index, int alloc);

ST_FUNC void tcc_output_object_file(FILE *f);
/* Create an ELF file on disk.
   This function handle ELF specific layout requirements */
ST_FUNC void tcc_output_elf(FILE *f, int phnum, ElfW(Phdr) *phdr,
                           int file_offset, int *sec_order);

ST_FUNC int tcc_load_object_file(FILE *f, unsigned long file_offset);

/* relocate code. Return -1 on error, required size if ptr is NULL,
   otherwise copy code into buffer passed by the caller */
ST_FUNC int tcc_relocate(void *ptr, addr_t ptr_diff);

ST_FUNC int tcc_add_symbol(const char *name, const void *val);

ST_FUNC addr_t get_elf_sym_addr(const char *name, int err);

ST_FUNC void *tcc_get_symbol(const char *name);

#endif