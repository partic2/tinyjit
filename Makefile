test:test_exe
	./test_exe 
#	objdump -d myobj2.o
	

test_exe:test.c xxx-gen.c tccutils.c tccelf.c tcc-platform.c
	$(CC) -o test_exe $^ -g -ggdb3 

clean:
	- rm test_exe
	- rm test_exe.exe