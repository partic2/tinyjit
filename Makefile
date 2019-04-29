test:test_exe
	./test_exe

test_exe:test.c xxx-gen.c tccutils.c tccelf.c tcc-platform.c
	gcc -o test_exe $^ -g -ggdb3

clean:
	rm test_exe.exe