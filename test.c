
#include "tccdef.h"
#include "xxx-gen.h"
#include "tccelf.h"
#include "tccutils.h"

#include <stdio.h>



int main(int argc,char *argv[]){
    FILE *f;
    Sym s;

    tccelf_new();
    xxx_gen_init();
    
    s.a.aligned=0;
    s.a.scope=SYM_SCOPE_GLOBAL;
    s.name="myfunc";
    s.type.t=VT_FUNC;
    s.c=0;
    put_extern_sym(&s,cur_text_section(),ind,0);

    gfunc_prolog();
    

    vpushl(VT_INT32,16);
    vtop->r=VT_LVAL|VT_LOCAL;
    vpushl(VT_INT32,20);
    vtop->r=VT_LVAL|VT_LOCAL;
    gen_opi(TOK_UMULL);


    gfunc_epilog();
    xxx_gen_deinit();
    f=fopen("myobj.o","wb+");
    tcc_output_object_file(f);
    fclose(f);
    tccelf_delete();
    printf("finish\n");
    return 0;
}