# tinyjit

Tinyjit is a tiny jit engine.

These code is partly fork from **tinycc**
So the APIs are seemed like tinycc. But there are some differents.

1. gfunc_prolog,gfunc_epilog work diferently. see xxx-gen.h

2. some struct,constant work differently. see tccdef.h

3. extract a little part of tccelf. library will only support a simple subset of elf format.

Now this project is incomplete. the arm and i386 backend can be used but have not been completely tested, and float-integer conversions are almost broken.

**the arm64 backend have not been ported.**


# TODO:

Port arm64 backend.

More test.

Float-integer conversions

Eliminate compile warning


# How to use

See test.c and the header files for help.