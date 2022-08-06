
#if defined _WIN32
#include <windows.h>
extern void *tcc_alloc_executable_memory(unsigned int size){
    return VirtualAlloc(NULL,size,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
}
extern void tcc_free_executable_memory(void *ptr,unsigned int size){
    VirtualFree(ptr,size,MEM_RELEASE);
}
#else
#include "tccutils.h"
#include <unistd.h>
#include <sys/mman.h>
extern void *tcc_alloc_executable_memory(unsigned int size){
	int fd;
    char tmpfile[255];
    char *tmpdir=getenv("TMPDIR");
    void *retval=NULL;
    if(tmpdir==NULL){
        tmpdir="/tmp";
    }
    if(strlen(tmpdir)>200){
        tcc_error("tmpdir path too long.");
        return NULL;
    }
    strcpy(tmpfile,tmpdir);
    strcat(tmpfile,"/XXXXXX");
    fd=mkstemp(tmpfile);
    
	if (fd == -1){
        tcc_error("temp file create failed");
    }

    unlink(tmpfile);

	if (ftruncate(fd, (off_t)size)) {
		close(fd);
		return NULL;
	}

	retval = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);

	if (retval == MAP_FAILED) {
		close(fd);
        tcc_error("mmap return failed");
		return NULL;
	}

	return retval;
}
extern void tcc_free_executable_memory(void *ptr,unsigned int size){
    munmap(ptr,size);
}
#endif