
#include "tccdef.h"
#include "xxx-gen.h"
#include "tccelf.h"
#include "tccutils.h"

#include <stdio.h>


static int testGenObjectFile(){
    FILE *f;
    Sym s;

    printf("testGenObjectFile\n");

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
    printf("done\n");
    return 0;
}


static int testLoadAndMergeObjectFile(){
    FILE *f,*f2;
    Sym s;

    
    tccelf_new();
    xxx_gen_init();
    
    s.a.aligned=0;
    s.a.scope=SYM_SCOPE_GLOBAL;
    s.name="myfunc2";
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

    printf("testLoadAndMergeObjectFile\n");
    fflush(stdout);
    f2=fopen("myobj.o","rb+");
    tcc_load_object_file(f2,0);
    fclose(f2);
    f=fopen("myobj2.o","wb+");
    tcc_output_object_file(f);
    fclose(f);
    tccelf_delete();
    printf("done\n");
    return 0;
}

int main(int argc,char *argv[]){
    testGenObjectFile();
    testLoadAndMergeObjectFile();
    return 0;
}