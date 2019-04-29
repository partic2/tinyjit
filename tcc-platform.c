

#if defined _WIN32
#include <windows.h>
extern void *tcc_alloc_executable_memory(unsigned int size){
    return VirtualAlloc(NULL,size,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
}
extern void tcc_free_executable_memory(void *ptr,unsigned int size){
    VirtualFree(ptr,size,MEM_RELEASE);
}
#else
#include <unistd.h>
#include <sys/mman.h>
extern void *tcc_alloc_executable_memory(unsigned int size){
    mmap(NULL,size,PROT_EXEC|PROT_READ|PROT_WRITE,MAP_ANONYMOUS,-1,0);
}
extern void tcc_free_executable_memory(void *ptr,unsigned int size){
    munmap(ptr,size);
}
#endif