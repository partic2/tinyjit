
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

    vpushi(0);
    vtop->type.t=VT_INT32;
    vpushi(0);
    vtop->type.t=VT_INT32;

    gfunc_prolog();
    
    vpushv(vtop-1);
    vpushv(vtop-3);
    gen_opi(TOK_ADD);

    r=gen_ldr();

    vpop(1);

    loc=alloc_local(8,8);
    vpushi(loc);
    vtop->r=VT_LVAL|VT_LOCAL;
    store(r,vtop);

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

    CType ct;
    ct.t=VT_INT32;
    
    tccelf_new();
    xxx_gen_init();
    
    s.a.aligned=0;
    s.a.scope=SYM_SCOPE_GLOBAL;
    s.name="myfunc2";
    s.type.t=VT_FUNC;
    s.c=0;
    put_extern_sym(&s,cur_text_section(),ind,0);

    Sym myfunc;
    myfunc.name="myfunc";
    myfunc.type.t=VT_FUNC;
    myfunc.c=0;
    myfunc.a.aligned=0;
    myfunc.a.scope=SYM_SCOPE_GLOBAL;


    gfunc_prolog();
    
    vpushi(0);
    printf(" (vtop->c.i-4) == (int)(vtop->c.i-4):%d\n",(vtop->c.i-4) == (int)(vtop->c.i-4));
    vtop->sym=&myfunc;
    vpushi(16);
    vpushi(20);
    gfunc_call(2,&ct);

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
    int (*myfunc2)();
    uint32_t *ptr2;
    printf("testRunInMemory\n");
    ptr=tcc_alloc_executable_memory(64*1024);
    printf("alloc executable memory:%d\n",(int32_t)ptr);
    if((int32_t)ptr==NULL){
        printf("error:%s\n",tcc_last_error());
        return 0;
    }
    fflush(stdout);
    tccelf_new();
    f=fopen("myobj2.o","rb+");
    tcc_load_object_file(f,0);
    tcc_relocate(ptr,0);
    myfunc2=tcc_get_symbol("myfunc2");
    printf("get myfunc2:%d\n",&myfunc2);
    ptr2=(void *)myfunc2;
    printf("myfunc2 header:%d\n",*ptr2);
    printf("myfunc2 return %d\n",myfunc2());
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