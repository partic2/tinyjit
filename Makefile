test:test_exe
	./test_exe 
#	objdump -d myobj2.o
	

test_exe:
	$(CC) -o test_exe test.c -g -ggdb3 

clean:
	- rm test_exe
	- rm test_exe.exe