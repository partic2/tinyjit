
#include "tccelf.h"
#include "elf.h"
#include "string.h"
#include "tccdef.h"
#include "tccutils.h"

#include "xxx-gen.h"
#include "xxx-link.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct SectionMergeInfo {
  Section *s;           /* corresponding existing section */
  unsigned long offset; /* offset of the new section in the existing section */
  unsigned long oldlen; /* size of existing section before adding this one */
  uint8_t new_section;  /* true if section 's' was added */
} SectionMergeInfo;

/* extra symbol attributes (not in symbol table) */
struct sym_attr {
  int dyn_index;
};

static int section_align = 0;

static Section *text_section;
static Section *_cur_text_section;
static Section *data_section;
static Section *bss_section;
static Section *common_section;
static Section *symtab_section;

static int new_undef_sym =
    0; /* Is there a new undefined sym since last new_undef_sym() */

/* sections */
Section **sections;
int nb_sections; /* number of sections, including first dummy section */

Section **priv_sections;
int nb_priv_sections; /* number of private sections */

/* temporary dynamic symbol sections (for dll loading) */
Section *dynsymtab_section;
/* exported dynamic symbol section */
Section *dynsym;
/* copy of the global symtab_section variable */
Section *symtab;
/* extra attributes (eg. GOT/PLT value) for symtab symbols */
struct sym_attr *sym_attrs;
int nb_sym_attrs;

/* special flag to indicate that the section should not be linked to the other
 * ones */
#define SHF_PRIVATE 0x80000000
/* section is dynsymtab_section */
#define SHF_DYNSYM 0x40000000

#define AFF_BINTYPE_REL 1
#define AFF_BINTYPE_DYN 2
/* target address type */
#define addr_t ElfW(Addr)
#define ElfSym ElfW(Sym)

/* Browse each elem of type <type> in section <sec> starting at elem <startoff>
   using variable <elem> */
#define for_each_elem(sec, startoff, elem, type)                               \
  for (elem = (type *)sec->data + startoff;                                    \
       elem < (type *)(sec->data + sec->data_offset); elem++)

static ElfSym *elfsym(Sym *s);

ST_FUNC void tccelf_new() {
  /* no section zero */
  dynarray_add(&sections, &nb_sections, NULL);

  /* create standard sections */
  text_section = new_section(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
  data_section = new_section(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
  bss_section = new_section(".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE);
  common_section = new_section(".common", SHT_NOBITS, SHF_PRIVATE);
  common_section->sh_num = SHN_COMMON;

  /* symbols are always generated for linking stage */
  symtab_section =
      new_symtab(".symtab", SHT_SYMTAB, 0, ".strtab", ".hashtab", SHF_PRIVATE);
  symtab = symtab_section;

  /* private symbol table for dynamic symbols */
  dynsymtab_section =
      new_symtab(".dynsymtab", SHT_SYMTAB, SHF_PRIVATE | SHF_DYNSYM,
                 ".dynstrtab", ".dynhashtab", SHF_PRIVATE);
  get_sym_attr(0, 1);
  _cur_text_section = text_section;
}

static void free_section(Section *s) { tcc_free(s->data); }

ST_FUNC void tccelf_delete() {
  int i;

  /* free all sections */
  for (i = 1; i < nb_sections; i++)
    free_section(sections[i]);
  dynarray_reset(&sections, &nb_sections);

  for (i = 0; i < nb_priv_sections; i++)
    free_section(priv_sections[i]);
  dynarray_reset(&priv_sections, &nb_priv_sections);
  tcc_free(sym_attrs);
  sym_attrs=0;
  symtab_section = NULL; /* for tccrun.c:rt_printline() */
}

/* reserve at least 'size' bytes in section 'sec' from
   sec->data_offset. */
static void *section_ptr_add(Section *sec, addr_t size) {
  size_t offset = section_add(sec, size, 1);
  return sec->data + offset;
}

ST_FUNC Section *new_section(const char *name, int sh_type, int sh_flags) {
  Section *sec;

  sec = tcc_malloc(sizeof(Section) + strlen(name));
  memset(sec, 0, sizeof(Section) + strlen(name));
  strcpy(sec->name, name);
  sec->sh_type = sh_type;
  sec->sh_flags = sh_flags;
  switch (sh_type) {
  case SHT_HASH:
  case SHT_REL:
  case SHT_RELA:
  case SHT_DYNSYM:
  case SHT_SYMTAB:
  case SHT_DYNAMIC:
    sec->sh_addralign = 4;
    break;
  case SHT_STRTAB:
    sec->sh_addralign = 1;
    break;
  default:
    sec->sh_addralign = PTR_SIZE; /* gcc/pcc default alignment */
    break;
  }

  if (sh_flags & SHF_PRIVATE) {
    dynarray_add(&priv_sections, &nb_priv_sections, sec);
  } else {
    sec->sh_num = nb_sections;
    dynarray_add(&sections, &nb_sections, sec);
  }

  return sec;
}

ST_FUNC void resolve_common_syms() {
  ElfW(Sym) * sym;

  /* Allocate common symbols in BSS.  */
  for_each_elem(symtab_section, 1, sym, ElfW(Sym)) {
    if (sym->st_shndx == SHN_COMMON) {
      /* symbol alignment is in st_value for SHN_COMMONs */
      sym->st_value = section_add(bss_section, sym->st_size, sym->st_value);
      sym->st_shndx = bss_section->sh_num;
    }
  }
}

ST_FUNC Section *new_symtab(const char *symtab_name, int sh_type, int sh_flags,
                            const char *strtab_name, const char *hash_name,
                            int hash_sh_flags) {
  Section *symtab, *strtab, *hash;
  int *ptr, nb_buckets;

  symtab = new_section(symtab_name, sh_type, sh_flags);
  symtab->sh_entsize = sizeof(ElfW(Sym));
  strtab = new_section(strtab_name, SHT_STRTAB, sh_flags);
  put_elf_str(strtab, "");
  symtab->link = strtab;
  put_elf_sym(symtab, 0, 0, 0, 0, 0, NULL);

  nb_buckets = 1;

  hash = new_section(hash_name, SHT_HASH, hash_sh_flags);
  hash->sh_entsize = sizeof(int);
  symtab->hash = hash;
  hash->link = symtab;

  ptr = section_ptr_add(hash, (2 + nb_buckets + 1) * sizeof(int));
  ptr[0] = nb_buckets;
  ptr[1] = 1;
  memset(ptr + 2, 0, (nb_buckets + 1) * sizeof(int));
  return symtab;
}

/* relocate a given section (CPU dependent) by applying the relocations
   in the associated relocation section */
ST_FUNC void relocate_section(Section *s) {
  Section *sr = s->reloc;
  ElfW_Rel *rel;
  ElfW(Sym) * sym;
  int type, sym_index;
  unsigned char *ptr;
  addr_t tgt, addr;

  relocate_init(sr);

  for_each_elem(sr, 0, rel, ElfW_Rel) {
    ptr = s->data + rel->r_offset;
    sym_index = ELFW(R_SYM)(rel->r_info);
    sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
    type = ELFW(R_TYPE)(rel->r_info);
    tgt = sym->st_value;
#if SHT_RELX == SHT_RELA
    tgt += rel->r_addend;
#endif
    addr = s->sh_addr + rel->r_offset;
    relocate(rel, type, ptr, addr, tgt);
  }
  /* if the relocation is allocated, we change its symbol table */
  if (sr->sh_flags & SHF_ALLOC)
    sr->link = dynsym;
}

ST_FUNC Section *cur_text_section() { return _cur_text_section; }

/* realloc section and set its content to zero */
ST_FUNC void section_realloc(Section *sec, unsigned long new_size) {
  unsigned long size;
  unsigned char *data;

  size = sec->data_allocated;
  if (size == 0)
    size = 1;
  while (size < new_size)
    size = size * 2;
  data = tcc_realloc(sec->data, size);
  memset(data + sec->data_allocated, 0, size - sec->data_allocated);
  sec->data = data;
  sec->data_allocated = size;
}

/* reserve at least 'size' bytes aligned per 'align' in section
   'sec' from current offset, and return the aligned offset */
ST_FUNC size_t section_add(Section *sec, addr_t size, int align) {
  size_t offset, offset1;

  offset = (sec->data_offset + align - 1) & -align;
  offset1 = offset + size;
  if (sec->sh_type != SHT_NOBITS && offset1 > sec->data_allocated)
    section_realloc(sec, offset1);
  sec->data_offset = offset1;
  if (align > sec->sh_addralign)
    sec->sh_addralign = align;
  return offset;
}

/* ------------------------------------------------------------------------- */
static ElfSym *elfsym(Sym *s) {
  if (!s || !s->c)
    return NULL;
  return &((ElfSym *)symtab_section->data)[s->c];
}

/* apply storage attributes to Elf symbol */
ST_FUNC void update_storage(Sym *sym) {
  ElfSym *esym;
  int sym_bind, old_sym_bind;

  esym = elfsym(sym);
  if (!esym)
    return;

  if (sym->a.scope == SYM_SCOPE_LOCAL)
    sym_bind = STB_LOCAL;
  else
    sym_bind = STB_GLOBAL;
  old_sym_bind = ELFW(ST_BIND)(esym->st_info);
  if (sym_bind != old_sym_bind) {
    esym->st_info = ELFW(ST_INFO)(sym_bind, ELFW(ST_TYPE)(esym->st_info));
  }
}

/* ------------------------------------------------------------------------- */
/* update sym->c so that it points to an external symbol in section
   'section' with value 'value' */

ST_FUNC void put_extern_sym2(Sym *sym, int sh_num, addr_t value,
                             unsigned long size) {
  int sym_type, sym_bind, info, other, t;
  ElfSym *esym;
  const char *name;
  char buf1[256];

  if (!sym->c) {
    name = sym->name;
    t = sym->type.t;
    if ((t & VT_TYPE) == VT_FUNC) {
      sym_type = STT_FUNC;
    } else if ((t & VT_TYPE) == VT_VOID) {
      sym_type = STT_NOTYPE;
    } else {
      sym_type = STT_OBJECT;
    }
    if (sym->a.scope == SYM_SCOPE_LOCAL)
      sym_bind = STB_LOCAL;
    else
      sym_bind = STB_GLOBAL;
    other = 0;
    info = ELFW(ST_INFO)(sym_bind, sym_type);
    sym->c =
        put_elf_sym(symtab_section, value, size, info, other, sh_num, name);
  } else {
    esym = elfsym(sym);
    esym->st_value = value;
    esym->st_size = size;
    esym->st_shndx = sh_num;
  }
  update_storage(sym);
}

ST_FUNC void put_extern_sym(Sym *sym, Section *section, addr_t value,
                            unsigned long size) {
  int sh_num = section ? section->sh_num : SHN_UNDEF;
  put_extern_sym2(sym, sh_num, value, size);
}

/* add a new relocation entry to symbol 'sym' in section 's' */
ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,
                     addr_t addend) {
  int c = 0;
  if (sym) {
    if (0 == sym->c)
      put_extern_sym(sym, NULL, 0, 0);
    c = sym->c;
  }
  /* now we can add ELF relocation info */
  put_elf_reloca(symtab_section, s, offset, type, c, addend);
}

/* elf symbol hashing function */
static unsigned long elf_hash(const unsigned char *name) {
  unsigned long h = 0, g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    if (g)
      h ^= g >> 24;
    h &= ~g;
  }
  return h;
}

/* rebuild hash table of section s */
/* NOTE: we do factorize the hash table code to go faster */
static void rebuild_hash(Section *s, unsigned int nb_buckets) {
  ElfW(Sym) * sym;
  int *ptr, *hash, nb_syms, sym_index, h;
  unsigned char *strtab;

  strtab = s->link->data;
  nb_syms = s->data_offset / sizeof(ElfW(Sym));

  if (!nb_buckets)
    nb_buckets = ((int *)s->hash->data)[0];

  s->hash->data_offset = 0;
  ptr = section_ptr_add(s->hash, (2 + nb_buckets + nb_syms) * sizeof(int));
  ptr[0] = nb_buckets;
  ptr[1] = nb_syms;
  ptr += 2;
  hash = ptr;
  memset(hash, 0, (nb_buckets + 1) * sizeof(int));
  ptr += nb_buckets + 1;

  sym = (ElfW(Sym) *)s->data + 1;
  for (sym_index = 1; sym_index < nb_syms; sym_index++) {
    if (ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
      h = elf_hash(strtab + sym->st_name) % nb_buckets;
      *ptr = hash[h];
      hash[h] = sym_index;
    } else {
      *ptr = 0;
    }
    ptr++;
    sym++;
  }
}

ST_FUNC int find_elf_sym(Section *s, const char *name) {
  ElfW(Sym) * sym;
  Section *hs;
  int nbuckets, sym_index, h;
  const char *name1;

  hs = s->hash;
  if (!hs)
    return 0;
  nbuckets = ((int *)hs->data)[0];
  h = elf_hash((unsigned char *)name) % nbuckets;
  sym_index = ((int *)hs->data)[2 + h];
  while (sym_index != 0) {
    sym = &((ElfW(Sym) *)s->data)[sym_index];
    name1 = (char *)s->link->data + sym->st_name;
    if (!strcmp(name, name1))
      return sym_index;
    sym_index = ((int *)hs->data)[2 + nbuckets + sym_index];
  }
  return 0;
}

/* put relocation */
ST_FUNC void put_elf_reloca(Section *symtab, Section *s, unsigned long offset,
                            int type, int symbol, addr_t addend) {
  char buf[256];
  Section *sr;
  ElfW_Rel *rel;

  sr = s->reloc;
  if (!sr) {
    /* if no relocation section, create it */
    snprintf(buf, sizeof(buf), REL_SECTION_FMT, s->name);
    /* if the symtab is allocated, then we consider the relocation
       are also */
    sr = new_section(buf, SHT_RELX, symtab->sh_flags);
    sr->sh_entsize = sizeof(ElfW_Rel);
    sr->link = symtab;
    sr->sh_info = s->sh_num;
    s->reloc = sr;
  }
  rel = section_ptr_add(sr, sizeof(ElfW_Rel));
  rel->r_offset = offset;
  rel->r_info = ELFW(R_INFO)(symbol, type);
#if SHT_RELX == SHT_RELA
  rel->r_addend = addend;
#else
  if (addend)
    tcc_error("non-zero addend on REL architecture");
#endif
}

/* return the symbol number */
ST_FUNC int put_elf_sym(Section *s, addr_t value, unsigned long size, int info,
                        int other, int shndx, const char *name) {
  int name_offset, sym_index;
  int nbuckets, h;
  ElfW(Sym) * sym;
  Section *hs;

  sym = section_ptr_add(s, sizeof(ElfW(Sym)));
  if (name && name[0])
    name_offset = put_elf_str(s->link, name);
  else
    name_offset = 0;
  /* XXX: endianness */
  sym->st_name = name_offset;
  sym->st_value = value;
  sym->st_size = size;
  sym->st_info = info;
  sym->st_other = other;
  sym->st_shndx = shndx;
  sym_index = sym - (ElfW(Sym) *)s->data;
  hs = s->hash;
  if (hs) {
    int *ptr, *base;
    ptr = section_ptr_add(hs, sizeof(int));
    base = (int *)hs->data;
    /* only add global or weak symbols. */
    if (ELFW(ST_BIND)(info) != STB_LOCAL) {
      /* add another hashing entry */
      nbuckets = base[0];
      h = elf_hash((unsigned char *)s->link->data + name_offset) % nbuckets;
      *ptr = base[2 + h];
      base[2 + h] = sym_index;
      base[1]++;
      /* we resize the hash table */
      hs->nb_hashed_syms++;
      if (hs->nb_hashed_syms > 2 * nbuckets) {
        rebuild_hash(s, 2 * nbuckets);
      }
    } else {
      *ptr = 0;
      base[1]++;
    }
  }
  return sym_index;
}

ST_FUNC int put_elf_str(Section *s, const char *sym) {
  int offset, len;
  char *ptr;

  len = strlen(sym) + 1;
  offset = s->data_offset;
  ptr = section_ptr_add(s, len);
  memmove(ptr, sym, len);
  return offset;
}

ST_FUNC struct sym_attr *get_sym_attr(int index, int alloc) {
  int n;
  struct sym_attr *tab;

  if (index >= nb_sym_attrs) {
    if (!alloc)
      return sym_attrs;
    /* find immediately bigger power of 2 and reallocate array */
    n = 1;
    while (index >= n)
      n *= 2;
    tab = tcc_realloc(sym_attrs, n * sizeof(*sym_attrs));
    sym_attrs = tab;
    memset(sym_attrs + nb_sym_attrs, 0,
           (n - nb_sym_attrs) * sizeof(*sym_attrs));
    nb_sym_attrs = n;
  }
  return &sym_attrs[index];
}

/* In an ELF file symbol table, the local symbols must appear below
   the global and weak ones. Since TCC cannot sort it while generating
   the code, we must do it after. All the relocation tables are also
   modified to take into account the symbol table sorting */
static void sort_syms(Section *s) {
  int *old_to_new_syms;
  ElfW(Sym) * new_syms;
  int nb_syms, i;
  ElfW(Sym) * p, *q;
  ElfW_Rel *rel;
  Section *sr;
  int type, sym_index;

  nb_syms = s->data_offset / sizeof(ElfW(Sym));
  new_syms = tcc_malloc(nb_syms * sizeof(ElfW(Sym)));
  old_to_new_syms = tcc_malloc(nb_syms * sizeof(int));

  /* first pass for local symbols */
  p = (ElfW(Sym) *)s->data;
  q = new_syms;
  for (i = 0; i < nb_syms; i++) {
    if (ELFW(ST_BIND)(p->st_info) == STB_LOCAL) {
      old_to_new_syms[i] = q - new_syms;
      *q++ = *p;
    }
    p++;
  }
  /* save the number of local symbols in section header */
  if (s->sh_size) /* this 'if' makes IDA happy */
    s->sh_info = q - new_syms;

  /* then second pass for non local symbols */
  p = (ElfW(Sym) *)s->data;
  for (i = 0; i < nb_syms; i++) {
    if (ELFW(ST_BIND)(p->st_info) != STB_LOCAL) {
      old_to_new_syms[i] = q - new_syms;
      *q++ = *p;
    }
    p++;
  }

  /* we copy the new symbols to the old */
  memcpy(s->data, new_syms, nb_syms * sizeof(ElfW(Sym)));
  tcc_free(new_syms);

  /* now we modify all the relocations */
  for (i = 1; i < nb_sections; i++) {
    sr = sections[i];
    if (sr->sh_type == SHT_RELX && sr->link == s) {
      for_each_elem(sr, 0, rel, ElfW_Rel) {
        sym_index = ELFW(R_SYM)(rel->r_info);
        type = ELFW(R_TYPE)(rel->r_info);
        sym_index = old_to_new_syms[sym_index];
        rel->r_info = ELFW(R_INFO)(sym_index, type);
      }
    }
  }

  tcc_free(old_to_new_syms);
}

/* Create an ELF file on disk.
   This function handle ELF specific layout requirements */
ST_FUNC void tcc_output_elf(FILE *f, int phnum, ElfW(Phdr) * phdr,
                            int file_offset, int *sec_order) {
  int i, shnum, offset, size, file_type;
  Section *s;
  ElfW(Ehdr) ehdr;
  ElfW(Shdr) shdr, *sh;

  shnum = nb_sections;

  memset(&ehdr, 0, sizeof(ehdr));

  if (phnum > 0) {
    ehdr.e_phentsize = sizeof(ElfW(Phdr));
    ehdr.e_phnum = phnum;
    ehdr.e_phoff = sizeof(ElfW(Ehdr));
  }

  /* align to 4 */
  file_offset = (file_offset + 3) & -4;

  /* fill header */
  ehdr.e_ident[0] = ELFMAG0;
  ehdr.e_ident[1] = ELFMAG1;
  ehdr.e_ident[2] = ELFMAG2;
  ehdr.e_ident[3] = ELFMAG3;
  ehdr.e_ident[4] = ELFCLASSW;
  ehdr.e_ident[5] = ELFDATA2LSB;
  ehdr.e_ident[6] = EV_CURRENT;

#ifdef TCC_TARGET_ARM
#ifdef TCC_ARM_EABI
  ehdr.e_ident[EI_OSABI] = 0;
  ehdr.e_flags = EF_ARM_EABI_VER4;

  if (abi_config.float_abi == ARM_HARD_FLOAT)
    ehdr.e_flags |= EF_ARM_VFP_FLOAT;
  else
    ehdr.e_flags |= EF_ARM_SOFT_FLOAT;
#else
  ehdr.e_ident[EI_OSABI] = ELFOSABI_ARM;
#endif
#endif

  ehdr.e_type = ET_REL;

  ehdr.e_machine = EM_TCC_TARGET;
  ehdr.e_version = EV_CURRENT;
  ehdr.e_shoff = file_offset;
  ehdr.e_ehsize = sizeof(ElfW(Ehdr));
  ehdr.e_shentsize = sizeof(ElfW(Shdr));
  ehdr.e_shnum = shnum;
  ehdr.e_shstrndx = shnum - 1;

  fwrite(&ehdr, 1, sizeof(ElfW(Ehdr)), f);
  fwrite(phdr, 1, phnum * sizeof(ElfW(Phdr)), f);
  offset = sizeof(ElfW(Ehdr)) + phnum * sizeof(ElfW(Phdr));

  sort_syms(symtab_section);
  for (i = 1; i < nb_sections; i++) {
    s = sections[sec_order[i]];

    if (s->sh_type != SHT_NOBITS) {
      while (offset < s->sh_offset) {
        fputc(0, f);
        offset++;
      }
      size = s->sh_size;
      if (size)
        fwrite(s->data, 1, size, f);
      offset += size;
    }
  }

  /* output section headers */
  while (offset < ehdr.e_shoff) {
    fputc(0, f);
    offset++;
  }

  for (i = 0; i < nb_sections; i++) {
    sh = &shdr;
    memset(sh, 0, sizeof(ElfW(Shdr)));
    s = sections[i];
    if (s) {
      sh->sh_name = s->sh_name;
      sh->sh_type = s->sh_type;
      sh->sh_flags = s->sh_flags;
      sh->sh_entsize = s->sh_entsize;
      sh->sh_info = s->sh_info;
      if (s->link)
        sh->sh_link = s->link->sh_num;
      sh->sh_addralign = s->sh_addralign;
      sh->sh_addr = s->sh_addr;
      sh->sh_offset = s->sh_offset;
      sh->sh_size = s->sh_size;
    }
    fwrite(sh, 1, sizeof(ElfW(Shdr)), f);
  }
}

static int alloc_sec_names(Section *strsec) {
  int i;
  Section *s;
  int textrel = 0;

  /* Allocate strings for section names */
  for (i = 1; i < nb_sections; i++) {
    s = sections[i];
    /* we output all sections if debug or object file */
    s->sh_size = s->data_offset;
    if (s->sh_size || (s->sh_flags & SHF_ALLOC))
      s->sh_name = put_elf_str(strsec, s->name);
  }
  strsec->sh_size = strsec->data_offset;
  return textrel;
}

/* Assign sections to segments and decide how are sections laid out when loaded
   in memory. This function also fills corresponding program headers. */
static int layout_object_sections(int *sec_order) {
  int i, sh_order_index, file_offset;
  unsigned long s_align;
  long long tmp;
  addr_t addr;
  ElfW(Phdr) * ph;
  Section *s;

  sh_order_index = 1;
  file_offset = sizeof(ElfW(Ehdr));
  s_align = ELF_PAGE_SIZE;
  if (section_align)
    s_align = section_align;

  /* all other sections come after */
  for (i = 1; i < nb_sections; i++) {
    s = sections[i];
    sec_order[sh_order_index++] = i;
    file_offset = (file_offset + s->sh_addralign - 1) & ~(s->sh_addralign - 1);
    s->sh_offset = file_offset;
    if (s->sh_type != SHT_NOBITS)
      file_offset += s->sh_size;
  }

  return file_offset;
}

ST_FUNC void tcc_output_object_file(FILE *f) {
  int shnum, file_offset, *sec_order;
  Section *strsec;
  int textrel;

  sec_order = NULL;
  textrel = 0;

  resolve_common_syms();

  /* we add a section for symbols */
  strsec = new_section(".shstrtab", SHT_STRTAB, 0);
  put_elf_str(strsec, "");

  /* Allocate strings for section names */
  textrel = alloc_sec_names(strsec);

  /* compute number of sections */
  shnum = nb_sections;

  /* this array is used to reorder sections in the output file */
  sec_order = tcc_malloc(sizeof(int) * shnum);
  sec_order[0] = 0;

  /* compute section to program header mapping */
  file_offset = layout_object_sections(sec_order);

  /* Create the ELF file with name 'filename' */
  tcc_output_elf(f, 0, NULL, file_offset, sec_order);
the_end:
  tcc_free(sec_order);
}

static ssize_t full_read(FILE *f, void *buf, size_t count) {
  char *cbuf = buf;
  size_t rnum = 0;
  while (1) {
    ssize_t num = fread(cbuf, 1, count - rnum, f);
    if (num < 0)
      return num;
    if (num == 0)
      return rnum;
    rnum += num;
    cbuf += num;
  }
}

static void *load_data(FILE *f, unsigned long file_offset, unsigned long size) {
  void *data;

  data = tcc_malloc(size);
  fseek(f, file_offset, SEEK_SET);

  full_read(f, data, size);
  return data;
}

/* add an elf symbol : check if it is already defined and patch
   it. Return symbol index. NOTE that sh_num can be SHN_UNDEF. */
ST_FUNC int set_elf_sym(Section *s, addr_t value, unsigned long size, int info,
                        int other, int shndx, const char *name) {
  ElfW(Sym) * esym;
  int sym_bind, sym_index, sym_type, esym_bind;
  unsigned char sym_vis, esym_vis, new_vis;

  sym_bind = ELFW(ST_BIND)(info);
  sym_type = ELFW(ST_TYPE)(info);
  sym_vis = ELFW(ST_VISIBILITY)(other);

  if (sym_bind != STB_LOCAL) {
    /* we search global or weak symbols */
    sym_index = find_elf_sym(s, name);
    if (!sym_index)
      goto do_def;
    esym = &((ElfW(Sym) *)s->data)[sym_index];
    if (esym->st_value == value && esym->st_size == size &&
        esym->st_info == info && esym->st_other == other &&
        esym->st_shndx == shndx)
      return sym_index;
    if (esym->st_shndx != SHN_UNDEF) {
      esym_bind = ELFW(ST_BIND)(esym->st_info);
      /* propagate the most constraining visibility */
      /* STV_DEFAULT(0)<STV_PROTECTED(3)<STV_HIDDEN(2)<STV_INTERNAL(1) */
      esym_vis = ELFW(ST_VISIBILITY)(esym->st_other);
      if (esym_vis == STV_DEFAULT) {
        new_vis = sym_vis;
      } else if (sym_vis == STV_DEFAULT) {
        new_vis = esym_vis;
      } else {
        new_vis = (esym_vis < sym_vis) ? esym_vis : sym_vis;
      }
      esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1)) | new_vis;
      other = esym->st_other; /* in case we have to patch esym */
      if (shndx == SHN_UNDEF) {
        /* ignore adding of undefined symbol if the
           corresponding symbol is already defined */
      } else if (sym_bind == STB_GLOBAL && esym_bind == STB_WEAK) {
        /* global overrides weak, so patch */
        goto do_patch;
      } else if (sym_bind == STB_WEAK && esym_bind == STB_GLOBAL) {
        /* weak is ignored if already global */
      } else if (sym_bind == STB_WEAK && esym_bind == STB_WEAK) {
        /* keep first-found weak definition, ignore subsequents */
      } else if (sym_vis == STV_HIDDEN || sym_vis == STV_INTERNAL) {
        /* ignore hidden symbols after */
      } else if ((esym->st_shndx == SHN_COMMON ||
                  esym->st_shndx == bss_section->sh_num) &&
                 (shndx < SHN_LORESERVE && shndx != bss_section->sh_num)) {
        /* data symbol gets precedence over common/bss */
        goto do_patch;
      } else {
      }
    } else {
    do_patch:
      esym->st_info = ELFW(ST_INFO)(sym_bind, sym_type);
      esym->st_shndx = shndx;
      new_undef_sym = 1;
      esym->st_value = value;
      esym->st_size = size;
      esym->st_other = other;
    }
  } else {
  do_def:
    sym_index = put_elf_sym(s, value, size, ELFW(ST_INFO)(sym_bind, sym_type),
                            other, shndx, name);
  }
  return sym_index;
}

ST_FUNC int tcc_object_type(FILE *f, ElfW(Ehdr) * h) {
  int size = full_read(f, h, sizeof *h);
  if (size == sizeof *h && 0 == memcmp(h, ELFMAG, 4)) {
    if (h->e_type == ET_REL)
      return AFF_BINTYPE_REL;
    if (h->e_type == ET_DYN)
      return AFF_BINTYPE_DYN;
  }
  return 0;
}

/* load an object file and merge it with current files */
/* XXX: handle correctly stab (debug) info */
ST_FUNC int tcc_load_object_file(FILE *f, unsigned long file_offset) {
  ElfW(Ehdr) ehdr;
  ElfW(Shdr) * shdr, *sh;
  int size, i, j, offset, offseti, nb_syms, sym_index, ret;
  unsigned char *strsec, *strtab;
  int *old_to_new_syms;
  char *sh_name, *name;
  SectionMergeInfo *sm_table, *sm;
  ElfW(Sym) * sym, *symtab;
  ElfW_Rel *rel;
  Section *s;

  fseek(f, file_offset, SEEK_SET);

  if (tcc_object_type(f, &ehdr) != AFF_BINTYPE_REL)
    goto fail1;
  /* test CPU specific stuff */
  if (ehdr.e_ident[5] != ELFDATA2LSB || ehdr.e_machine != EM_TCC_TARGET) {
  fail1:
    tcc_error("invalid object file");
    return -1;
  }

  /* read sections */
  shdr = load_data(f, file_offset + ehdr.e_shoff,
                   sizeof(ElfW(Shdr)) * ehdr.e_shnum);
  sm_table = tcc_malloc(sizeof(SectionMergeInfo) * ehdr.e_shnum);
  memset(sm_table, 0, sizeof(SectionMergeInfo) * ehdr.e_shnum);

  /* load section names */
  sh = &shdr[ehdr.e_shstrndx];
  strsec = load_data(f, file_offset + sh->sh_offset, sh->sh_size);

  /* load symtab and strtab */
  old_to_new_syms = NULL;
  symtab = NULL;
  strtab = NULL;
  nb_syms = 0;
  for (i = 1; i < ehdr.e_shnum; i++) {
    sh = &shdr[i];
    if (sh->sh_type == SHT_SYMTAB) {
      if (symtab) {
        tcc_error("object must contain only one symtab");
      fail:
        ret = -1;
        goto the_end;
      }
      nb_syms = sh->sh_size / sizeof(ElfW(Sym));
      symtab = load_data(f, file_offset + sh->sh_offset, sh->sh_size);
      sm_table[i].s = symtab_section;

      /* now load strtab */
      sh = &shdr[sh->sh_link];
      strtab = load_data(f, file_offset + sh->sh_offset, sh->sh_size);
    }
  }
  /* now examine each section and try to merge its content with the
     ones in memory */
  for (i = 1; i < ehdr.e_shnum; i++) {
    /* no need to examine section name strtab */
    if (i == ehdr.e_shstrndx)
      continue;
    sh = &shdr[i];
    sh_name = (char *)strsec + sh->sh_name;
    /* ignore sections types we do not handle */
    if (sh->sh_type != SHT_PROGBITS && sh->sh_type != SHT_RELX &&
#ifdef TCC_ARM_EABI
        sh->sh_type != SHT_ARM_EXIDX &&
#endif
        sh->sh_type != SHT_NOBITS && sh->sh_type != SHT_PREINIT_ARRAY &&
        sh->sh_type != SHT_INIT_ARRAY && sh->sh_type != SHT_FINI_ARRAY &&
        strcmp(sh_name, ".stabstr"))
      continue;
    if (sh->sh_addralign < 1)
      sh->sh_addralign = 1;
    /* find corresponding section, if any */
    for (j = 1; j < nb_sections; j++) {
      s = sections[j];
      if (!strcmp(s->name, sh_name)) {
        goto found;
      }
    }
    /* not found: create new section */
    s = new_section(sh_name, sh->sh_type, sh->sh_flags & ~SHF_GROUP);
    /* take as much info as possible from the section. sh_link and
       sh_info will be updated later */
    s->sh_addralign = sh->sh_addralign;
    s->sh_entsize = sh->sh_entsize;
    sm_table[i].new_section = 1;
  found:
    if (sh->sh_type != s->sh_type) {
      tcc_error("invalid section type");
      goto fail;
    }
    /* align start of section */
    sm_table[i].oldlen = s->data_offset;
    offset = s->data_offset;

    size = sh->sh_addralign - 1;
    offset = (offset + size) & ~size;
    if (sh->sh_addralign > s->sh_addralign)
      s->sh_addralign = sh->sh_addralign;
    s->data_offset = offset;
  no_align:
    sm_table[i].offset = offset;
    sm_table[i].s = s;
    /* concatenate sections */
    size = sh->sh_size;
    if (sh->sh_type != SHT_NOBITS) {
      unsigned char *ptr;
      fseek(f, file_offset + sh->sh_offset, SEEK_SET);
      ptr = section_ptr_add(s, size);
      full_read(f, ptr, size);
    } else {
      s->data_offset += size;
    }
  next:;
  }

  /* second short pass to update sh_link and sh_info fields of new
     sections */
  for (i = 1; i < ehdr.e_shnum; i++) {
    s = sm_table[i].s;
    if (!s || !sm_table[i].new_section)
      continue;
    sh = &shdr[i];
    if (sh->sh_link > 0)
      s->link = sm_table[sh->sh_link].s;
    if (sh->sh_type == SHT_RELX) {
      if (sm_table[sh->sh_info].s) {
        s->sh_info = sm_table[sh->sh_info].s->sh_num;
        /* update backward link */
        sections[s->sh_info]->reloc = s;
      } else {
        /* we haven't read the target of relocs, so cancel them */
        s->data_offset = sm_table[i].oldlen;
        sm_table[i].s = NULL;
      }
    }
  }
  sm = sm_table;

  /* resolve symbols */
  old_to_new_syms = tcc_malloc(nb_syms * sizeof(int));
  memset(old_to_new_syms, 0, nb_syms * sizeof(int));

  sym = symtab + 1;
  for (i = 1; i < nb_syms; i++, sym++) {
    if (sym->st_shndx != SHN_UNDEF && sym->st_shndx < SHN_LORESERVE) {
      sm = &sm_table[sym->st_shndx];
      /* if no corresponding section added, no need to add symbol */
      if (!sm->s)
        continue;
      /* convert section number */
      sym->st_shndx = sm->s->sh_num;
      /* offset value */
      sym->st_value += sm->offset;
    }
    /* add symbol */
    name = (char *)strtab + sym->st_name;
    sym_index = set_elf_sym(symtab_section, sym->st_value, sym->st_size,
                            sym->st_info, sym->st_other, sym->st_shndx, name);
    old_to_new_syms[i] = sym_index;
  }

  /* third pass to patch relocation entries */
  for (i = 1; i < ehdr.e_shnum; i++) {
    s = sm_table[i].s;
    if (!s)
      continue;
    sh = &shdr[i];
    offset = sm_table[i].offset;
    switch (s->sh_type) {
    case SHT_RELX:
      /* take relocation offset information */
      offseti = sm_table[sh->sh_info].offset;
      for_each_elem(s, (offset / sizeof(*rel)), rel, ElfW_Rel) {
        int type;
        unsigned sym_index;
        /* convert symbol index */
        type = ELFW(R_TYPE)(rel->r_info);
        sym_index = ELFW(R_SYM)(rel->r_info);
        /* NOTE: only one symtab assumed */
        if (sym_index >= nb_syms)
          goto invalid_reloc;
        sym_index = old_to_new_syms[sym_index];
        /* ignore link_once in rel section. */
        if (!sym_index
#ifdef TCC_TARGET_ARM
            && type != R_ARM_V4BX
#endif
        ) {
        invalid_reloc:
          tcc_error("Invalid relocation entry");
          goto fail;
        }
        rel->r_info = ELFW(R_INFO)(sym_index, type);
        /* offset the relocation offset */
        rel->r_offset += offseti;
      }
      break;
    default:
      break;
    }
  }

  ret = 0;
the_end:
  tcc_free(symtab);
  tcc_free(strtab);
  tcc_free(old_to_new_syms);
  tcc_free(sm_table);
  tcc_free(strsec);
  tcc_free(shdr);
  return ret;
}