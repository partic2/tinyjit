test:test_exe
	./test_exe

test_exe:test.c xxx-gen.c tccutils.c tccelf.c
	gcc -o test_exe $^