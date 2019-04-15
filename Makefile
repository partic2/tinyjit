test:test_exe
	./test_exe

test_exe:test.c arm-gen.c xxx-gen.c arm-link.c tccutils.c tccelf.c
	gcc -o test_exe $^