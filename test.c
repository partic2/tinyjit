
#include "tccdef.h"
#include "xxx-gen.h"
#include "tccelf.h"
#include "tccutils.h"

#include <stdio.h>

#include "tcc-platform.h"

static int testGenObjectFile(){
    FILE *f;
    Sym s;
    int loc,r;

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
    

    vpushl(VT_INT16,16);
    vtop->r=VT_LVAL|VT_LOCAL;
    vpushl(VT_INT32,20);
    vtop->r=VT_LVAL|VT_LOCAL;
    gen_opi(TOK_UMULL);

    vpop(1);

    loc=alloc_local(8,8);
    vpushl(VT_INT64,loc);
    vtop->r=VT_LOCAL;
    r=get_reg_of_cls(RC_INT);
    load(r,vtop);
    vtop->r=r;
    vtop->c.r2=VT_CONST;
    vtop->r|=VT_LVAL;

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
    vpushl(VT_INT32,20);
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
#ifdef TCC_IS_NATIVE
static int testRunInMemory(){
    void *ptr;
    FILE *f;
    int i;
    long long (*myfunc2)();
    printf("testRunInMemory\n");
    ptr=tcc_alloc_executable_memory(0x800);
    tccelf_new();
    f=fopen("myobj2.o","rb+");
    tcc_load_object_file(f,0);
    tcc_relocate(ptr,0);
    myfunc2=tcc_get_symbol("myfunc2");
    printf("myfunc2 return %lld\n",myfunc2());
    tccelf_delete();
    tcc_free_executable_memory(ptr,0x800);
    printf("done\n");
}
#endif

int main(int argc,char *argv[]){
    testGenObjectFile();
    testLoadAndMergeObjectFile();
    #ifdef TCC_IS_NATIVE
    testRunInMemory();
    #endif
    return 0;
}