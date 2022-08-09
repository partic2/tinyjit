
#if !TCC_IMPORT_BY_INCLUDE_ALL
#include "tccutils.h"
#endif

#include "tccdef.h"
#include <stdio.h>
#include <stdlib.h>

char TCC_ERROR_UNIMPLEMENTED[]="unimplemented";

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *(void ***)ptab;
    /* every power of two we double array size */
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = tcc_realloc(pp, nb_alloc * sizeof(void *));
        *(void***)ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n)
{
    void **p;
    for (p = *(void***)pp; *n; ++p, --*n)
        if (*p)
            tcc_free(*p);
    tcc_free(*(void**)pp);
    *(void**)pp = NULL;
}

static char *error_message;
ST_FUNC void tcc_error(char *message){
    error_message=message;
}

ST_FUNC char *tcc_last_error(){
    return error_message;
}


#if TCC_IMPORT_BY_INCLUDE_ALL
#include "tcc-platform.c"
#include "xxx-gen.c"
#include "tccelf.c"
#endif

