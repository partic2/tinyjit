

/* config flag , see src/config.h for detail help */
#define TCC_IMPORT_BY_INCLUDE_ALL 1
#define TCC_TARGET_X86_64
#define TCC_WINDOWS_ABI
#define TCC_IS_NATIVE 1

#include "src/tccutils.h"
#include <stdio.h>


static int testGenObjectFile(){
    FILE *f;
    Sym s;
    int loc,r;

    printf("testGenObjectFile\n");

/*
    Define a function work like below c code
    int myfunc(int a,int b){
        int r=a+b;
        return r;
    }
    
*/

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
    
    vpushv(vtop-2);
    vpushv(vtop-2);
    gen_opi(TOK_SUB);

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
    
/*
    Define a function work like below c code
    int myfunc2(){
        int minval=myfunc(0x12345678,0x1234);
        if(!(r<=0x12345678)){
            minval=0x12345678;
        }
        return minval;
    }
    
*/

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
    vtop->sym=&myfunc;
    vpushi(0x12345678);
    vpushi(0x1234);
    gfunc_call(2,&ct);
    int r=gen_ldr();
    int minval=alloc_local(8,8);
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->c.i=minval;
    store(r,vtop);
    vtop->r=r;
    vtop->r2=VT_CONST;
    vpushi(0x12345678);
    gen_opi(TOK_LT);
    int label=gtst(0,0);
    vpushi(0x12345678);
    r=gen_ldr();
    vtop->r=VT_LOCAL|VT_LVAL;
    vtop->c.i=minval;
    store(r,vtop);
    vpop(1);
    gsym_addr(label,ind);

    vpushi(minval);
    vtop->r=VT_LOCAL|VT_LVAL;

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
    int (*myfunc)(int a,int b);
    uint32_t *ptr2;
    printf("testRunInMemory\n");
    ptr=tcc_alloc_executable_memory(64*1024);
    #if PTR_SIZE==4
    printf("alloc executable memory:%d\n",(uint32_t)ptr);
    #else
    printf("alloc executable memory:%lld\n",(uint64_t)ptr);
    #endif
    if(ptr==NULL){
        printf("error:%s\n",tcc_last_error());
        return 0;
    }
    fflush(stdout);
    tccelf_new();
    f=fopen("myobj2.o","rb+");
    tcc_load_object_file(f,0);
    if(tcc_relocate(ptr,0)<0){
        printf("error on relocate");
        return 0;
    }
    myfunc=tcc_get_symbol("myfunc");
    myfunc2=tcc_get_symbol("myfunc2");
    #if PTR_SIZE==4
    printf("get myfunc2:%d\n",&myfunc2);
    #else
    printf("get myfunc2:%lld\n",&myfunc2);
    #endif
    ptr2=(void *)myfunc2;
    printf("myfunc2 header:%d\n",*ptr2);
    printf("myfunc return %d\n",myfunc(0x12345678,0x1234));
    printf("myfunc2 return %d\n",myfunc2());
    tccelf_delete();
    tcc_free_executable_memory(ptr,0x800);
    printf("done\n");
}
#endif

int main(int argc,char *argv[]){
    testGenObjectFile();
    testLoadAndMergeObjectFile();
    #if TCC_IS_NATIVE
    testRunInMemory();
    #endif
    return 0;
}