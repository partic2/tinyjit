
#ifndef _TCC_UTILS_H
#define _TCC_UTILS_H 1

#include "config.h"


#include "tccdef.h"
#include <stdlib.h>
#include <memory.h>

extern char TCC_ERROR_UNIMPLEMENTED[];

/* return NULL if no error occured. */
ST_FUNC char *tcc_last_error();

/* set error message, call tcc_error(NULL) to clear error*/
ST_FUNC void tcc_error(char *message);

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data);
ST_FUNC void dynarray_reset(void *pp, int *n);


ST_FUNC const char *get_tok_str(int v);

static inline uint16_t read16le(unsigned char *p) {
    return p[0] | (uint16_t)p[1] << 8;
}
static inline void write16le(unsigned char *p, uint16_t x) {
    p[0] = x & 255;  p[1] = x >> 8 & 255;
}
static inline uint32_t read32le(unsigned char *p) {
  return read16le(p) | (uint32_t)read16le(p + 2) << 16;
}
static inline void write32le(unsigned char *p, uint32_t x) {
    write16le(p, x);  write16le(p + 2, x >> 16);
}
static inline void add32le(unsigned char *p, int32_t x) {
    write32le(p, read32le(p) + x);
}
static inline uint64_t read64le(unsigned char *p) {
  return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}
static inline void write64le(unsigned char *p, uint64_t x) {
    write32le(p, x);  write32le(p + 4, x >> 32);
}
static inline void add64le(unsigned char *p, int64_t x) {
    write64le(p, read64le(p) + x);
}


#define tcc_realloc realloc
#define tcc_malloc malloc
#define tcc_free free

#if TCC_IMPORT_BY_INCLUDE_ALL
#include "tccutils.c"
#endif

#endif

