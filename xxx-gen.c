#define VSTACK_SIZE 255

#include "tccdef.h"
#include "tccutils.h"
#include "xxx-gen.h"


SValue *__vstack, *vtop;
uint32_t ind,loc;

CType int_type;
CType int64_type;
CType ptr_type;


ST_FUNC uint32_t get_VT_INT_TYPE_of_size(unsigned int size){
    int type;
    switch(size){
        case 4:
        type=VT_INT32;
        break;
        case 8:
        type=VT_INT64;
        break;
        case 2:
        type=VT_INT16;
        break;
        case 1:
        type=VT_INT8;
        break;
    }
    return type;
}
ST_FUNC unsigned int size_align_of_type(uint32_t type,uint32_t *align){
    unsigned int s;
    if(type<=(VT_INTLAST|VT_UNSIGNED)){
        type &= ~VT_UNSIGNED;
        s=1<<(type-1);
    }else{
        switch(type){
            case VT_FLOAT32:
            s=4;
            break;
            case VT_FLOAT64:
            s=8;
            break;
        }
    }
    if(align!=NULL){
        *align=s;
    }
    
    return s;
}

/* push constant of type "type" with useless value */
ST_FUNC void vpush(CType *type)
{
    vset(type, VT_CONST, 0);
}

/* push integer constant */
ST_FUNC void vpushi(int v)
{
    CValue cval;
    cval.i = v;
    vsetc(&int_type, VT_CONST, &cval);
}

/* push a pointer sized constant */
static void vpushs(addr_t v)
{
  CValue cval;
  cval.i = v;
  vsetc(&ptr_type, VT_CONST, &cval);
}

/* push arbitrary long constant */
ST_FUNC void vpushl(int ty, unsigned long long v)
{
    CValue cval;
    CType ctype;
    ctype.t = ty;
    cval.i = v;
    vsetc(&ctype, VT_CONST, &cval);
}


ST_FUNC void vset(CType *type, int r, int v)
{
    CValue cval;

    cval.i = v;
    vsetc(type, r, &cval);
}

#ifndef USE_ARCH_DEFINED_VSETC
ST_FUNC void vsetc(CType *type, int r, CValue *vc)
{
    int v;
    
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        tcc_error("memory full (vstack)");
    /* cannot let cpu flags if other instruction are generated. Also
       avoid leaving VT_JMP anywhere except on the top of the stack
       because it would complicate the code generator.

       Don't do this when nocode_wanted.  vtop might come from
       !nocode_wanted regions (see 88_codeopt.c) and transforming
       it to a register without actually generating code is wrong
       as their value might still be used for real.  All values
       we push under nocode_wanted will eventually be popped
       again, so that the VT_CMP/VT_JMP value will be in vtop
       when code is unsuppressed again.

       Same logic below in vswap(); */
    if (vtop >= vstack) {
        v = vtop->r & VT_VALMASK;
        if (v == VT_CMP || (v & ~1) == VT_JMP)
            gen_ldr();
    }

    vtop++;
    vtop->type = *type;
    vtop->r = r;
    vtop->r2 = VT_CONST;
    vtop->c = *vc;
    vtop->sym=NULL;
}
#endif


static void vseti(int r, int v)
{
    CType type;
    type.t = VT_INT32;
    vset(&type, r, v);
}

ST_FUNC void vpushv(SValue *v)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        tcc_error("memory full (vstack)");
    vtop++;
    *vtop = *v;
}


/* push a symbol value of TYPE */
ST_FUNC void vpushsym(CType *type, Sym *sym)
{
    CValue cval;
    cval.i = 0;
    vsetc(type, VT_CONST, &cval);
    vtop->sym = sym;
}

/* rotate n first stack elements to the bottom
   I1 ... In -> I2 ... In I1 [top is right]
*/
ST_FUNC void vrotb(int n)
{
    int i;
    SValue tmp;

    tmp = vtop[-n + 1];
    for(i=-n+1;i!=0;i++)
        vtop[i] = vtop[i+1];
    vtop[0] = tmp;
}

#ifndef USE_ARCH_DEFINED_VPOP
ST_FUNC void vpop(int n){
    SValue *psv;
    SValue sv;
    for(psv=vtop;vtop-psv<n;psv--){
        if((psv->r&VT_VALMASK)<VT_CONST){
            if(get_reg_attr(psv->r)->c&RC_STACK_MODEL){
                sv.type.t=VT_VOID;
                store(psv->r&VT_VALMASK,&sv);
            }
        }
    }
    vtop=psv;
}
#endif

ST_FUNC void vswap(void)
{
    SValue tmp;
    /* cannot vswap cpu flags. See comment at vsetc() above */
    if (vtop >= vstack) {
        int v = vtop->r & VT_VALMASK;
        if (v == VT_CMP || (v & ~1) == VT_JMP)
            gen_ldr();
    }
    tmp = vtop[0];
    vtop[0] = vtop[-1];
    vtop[-1] = tmp;
}

ST_FUNC int alloc_local(int size,int align){
    loc = (loc - size) & -align;
    return loc;
}

ST_FUNC void save_rc_upstack(int rc,int n){
    int i=0;
    for(i=0;i<NB_REGS;i++){
        if((get_reg_attr(i)->c&rc)==rc && !get_reg_attr(i)->s&RS_LOCKED){
            save_reg_upstack(i,n);
        }
    }
}


ST_FUNC void gen_lexpand(){
    int r,r2;
    r=vtop->r;
    vtop->type.t--;
    if(r&VT_LVAL){
        vdup();
        gen_lval_offset(REGISTER_SIZE);
    }else{
        r=r&VT_VALMASK;
        if(r<VT_CONST){
            r2=vtop->r2;
            vtop->r2=VT_CONST;
            vdup();
            vtop->r=r2;
        }else if(r==VT_CONST && !vtop->sym){
            vdup();
            vtop->c.i>>=32;
        }
    }
    vtop[0].sym=NULL;
    vtop[-1].sym=NULL;
}

ST_FUNC void gen_load_reg(int r){
    if(((vtop->r&VT_VALMASK)!=r)||(vtop->r&VT_LVAL)){
        save_reg_upstack(r,1);
        load(r,vtop);
        vtop->r=r;
        vtop->r2=VT_CONST;
    }
    vtop->sym=NULL;
}

ST_FUNC void save_reg_upstack(int r,int n){
    int l, rloc, saved, size, r2;
    uint32_t align;
    SValue *p, *p1, sv;
    CType *type;

    if ((r &= VT_VALMASK) >= VT_CONST)
        return;

    /* modify all stack values */
    saved = 0;
    l = 0;
    rloc = 0;
    /* found if register used in tow words value. */
    for(p = vstack, p1 = vtop - n; p <= p1; p++) {
        if((p->r&VT_VALMASK)==VT_CONST)continue;
        if ((((p->r & VT_VALMASK) == r) && (p->r2 & VT_VALMASK) < VT_CONST)||
            ((p->r & VT_VALMASK) < VT_CONST && (p->r2 & VT_VALMASK) == r)) {
            /* must save value on stack if not already done */
            size=size_align_of_type(p->type.t,&align);
            //printf("p:%x\n",(((p->r & VT_VALMASK) == r) && (p->r & VT_VALMASK) < VT_CONST));
            l=get_temp_local_var(size,align);
            sv.type.t = p->type.t;
            sv.r = VT_LOCAL | VT_LVAL;
            sv.r2 = VT_CONST;
            sv.c.i = l;
            store(p->r & VT_VALMASK, &sv);
            rloc=sv.c.i;
            if(p->r2<VT_CONST){
                sv.c.i+=get_reg_attr(p->r)->size;
                store(p->r2,&sv);
                if(p->r2==r){
                    rloc=sv.c.i;
                }
            }
            saved=1;
            break;
        }
    }
    for(p = vstack, p1 = vtop - n; p <= p1; p++) {
        if (((p->r & VT_VALMASK) == r) || ((p->r2 & VT_VALMASK) == r)) {
            if (!saved) {
                /* must save value on stack if not already done */
                r2=p->r & VT_VALMASK;
                /* store register in the stack */
                type = &p->type;
                size=size_align_of_type(type->t,&align);
				l=get_temp_local_var(size,align);
                rloc=l;
                sv.type.t = type->t;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.r2 = VT_CONST;
                sv.c.i = l;
                
                store(r2, &sv);

                saved = 1;
            }
            /* mark that stack entry as being saved on the stack */
            if (p->r & VT_LVAL) {
                p->r = (p->r & ~VT_VALMASK) | VT_LLOCAL;
            } else {
                p->r = (p->r & ~VT_VALMASK) | VT_LOCAL | VT_LVAL;
            }
            if((p->r2&VT_VALMASK) < VT_CONST){
                p->c.i=l;
            }else{
                p->c.i = rloc;
            }
        }
    }
}

ST_FUNC int is_reg_free(int r,int upstack){
    SValue *p;
	if(get_reg_attr(r)->s&RS_LOCKED){
		return 0;
	}
    for(p=vstack;p<=vtop-upstack;p++){
        if(((p->r&VT_VALMASK) ==r) || 
            (((p->r&VT_VALMASK)<VT_CONST) && (p->r2==r))){
            return 0;
        }
    }
    return 1;
}

/* get free register if r.c&rc==rc */
ST_FUNC int get_reg_of_cls(int rc){
    SValue *p;
    int r,i;
    for(i=0;i<NB_REGS;i++){
        if((get_reg_attr(i)->c&rc)==rc){
            if(is_reg_free(i,0)){
                return i;
            }
        }
    }
    /* no register left : free the first one on the stack (VERY
       IMPORTANT to start from the bottom to ensure that we don't
       spill registers used in gen_opi()) */
    for(p=vstack;p<=vtop;p++) {
        /* look at second register (if long long) */
        r = p->r2 & VT_VALMASK;
        if (r < VT_CONST && ((get_reg_attr(r)->c & rc)==rc) && (get_reg_attr(r)->s&RS_LOCKED))
            goto save_found;
        r = p->r & VT_VALMASK;
        if (r < VT_CONST && ((get_reg_attr(r)->c & rc)==rc) && (get_reg_attr(r)->s&RS_LOCKED)) {
        save_found:
            save_reg_upstack(r,0);
            return r;
        }
    }
    return -1;
}

ST_FUNC int gen_ldr(){
	int size,type,r,r2;
    uint32_t align;
    type=vtop->type.t;
    if((!(vtop->r&VT_LVAL))&&(vtop->r<VT_CONST)){
        if(is_reg_free(vtop->r&VT_VALMASK,1)){
            return vtop->r;
        }
    }
    if(is_float(type)){
        r=get_reg_of_cls(RC_FLOAT);
        load(r,vtop);
        vtop->r=r;
        vtop->r2=VT_CONST;
    }else{
        size=size_align_of_type(vtop->type.t,&align);
        if(size<=REGISTER_SIZE){
            r=get_reg_of_cls(RC_INT);
            load(r,vtop);
            vtop->r=r;
            vtop->r2=VT_CONST;
        }else if(size==2*REGISTER_SIZE){
            gen_lexpand();
            gen_ldr();
            vswap();
            gen_ldr();
            vswap();
            r2=vtop->r;
            vpop(1);
            vtop->r2=r2;
        }
    }
    vtop->sym=NULL;
    return vtop->r;
}


#define MAX_TEMP_LOCAL_VARIABLE_NUMBER 0x10
/*list of temporary local variables on the stack in current function. */
struct temp_local_variable {
	int location; //offset on stack. Svalue.c.i
	short size;
	short align;
} arr_temp_local_vars[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
short nb_temp_local_vars;



ST_FUNC int get_temp_local_var(int size,int align){
	int i;
	struct temp_local_variable *temp_var;
	int found_var;
	SValue *p;
	int r;
	char free;
	char found;
	found=0;
	for(i=0;i<nb_temp_local_vars;i++){
		temp_var=&arr_temp_local_vars[i];
		if(temp_var->size<size||align!=temp_var->align){
			continue;
		}
		/*check if temp_var is free*/
		free=1;
		for(p=vstack;p<=vtop;p++) {
			r=p->r&VT_VALMASK;
			if(r==VT_LOCAL||r==VT_LLOCAL){
				if((p->c.i >= temp_var->location) && (p->c.i <= temp_var->location+temp_var->size)){
					free=0;
					break;
				}
			}
		}
		if(free){
			found_var=temp_var->location;
			found=1;
			break;
		}
	}
	if(!found){
		found_var=alloc_local(size,align);
		if(nb_temp_local_vars<MAX_TEMP_LOCAL_VARIABLE_NUMBER){
			temp_var=&arr_temp_local_vars[i];
			temp_var->location=found_var;
			temp_var->size=size;
			temp_var->align=align;
			nb_temp_local_vars++;
		}
	}
	return found_var;
}

ST_FUNC void clear_temp_local_var_list(){
	nb_temp_local_vars=0;
}

ST_FUNC void gen_addr_of(){
    vtop->r &= ~VT_LVAL;
    /* tricky: if saved lvalue, then we can go back to lval.. . ue */
    if ((vtop->r & VT_VALMASK) == VT_LLOCAL)
        vtop->r = (vtop->r & ~(VT_VALMASK)) | VT_LOCAL | VT_LVAL;
}

ST_FUNC void gen_lval_of(){
    vtop->r |= VT_LVAL;
}

ST_FUNC int is_lval(){
    return vtop->r & VT_LVAL;
}
ST_FUNC void gen_lval_offset(int offset){
    int r;
    r=vtop->r&VT_VALMASK;
    if(r<VT_CONST){
        gen_addr_of();
        vpushi(offset);
        gen_opi(TOK_ADD);
        gen_lval_of();
    }else if((r==VT_CONST)||(r==VT_LOCAL)){
        vtop->c.i+=offset;
    }
    
}


ST_FUNC void xxx_gen_init(){
    int_type.t=VT_INT32;
    int64_type.t=VT_INT64;
    ptr_type.t=get_VT_INT_TYPE_of_size(PTR_SIZE);
    ind=0;
    __vstack=tcc_malloc(sizeof(SValue)*VSTACK_SIZE+1);
    vtop=__vstack;
    arch_gen_init();
}

ST_FUNC void xxx_gen_deinit(){
    arch_gen_deinit();
    tcc_free(__vstack);
    __vstack=NULL;
    cur_text_section()->data_offset = ind;
}


#ifdef TCC_TARGET_I386
#include "i386-gen.c"
#include "i386-link.c"
#endif

#ifdef TCC_TARGET_ARM
#include "arm-gen.c"
#include "arm-link.c"
#endif

#ifdef TCC_TARGET_X86_64
#include "x86_64-gen.c"
#include "x86_64-link.c"
#endif

#ifdef TCC_TARGET_ARM64
#include "arm64-gen.c"
#include "arm64-link.c"
#endif