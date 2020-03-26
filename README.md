# ffiasm

This package is a script that generates a Finite field Library in Intel64 Assembly

## Ussage

install g++ nasm ang gmp library if you don't have it.

`
npm install -g ffiasm
mkdir myProject
cd myProject
buildzqfield -q 21888242871839275222246405745257275088548364400416034343698204186575808495617 -n Fr
`

You now will have two files fr.c and fr.asm

If you are in linux:
`
nasm -felf64 fr.asm
`

If you are in a mac:
`
nasm -fmacho64 --prefix _ fr.asm
`

Create a file named main.c to use the library

`
#include <stdio.h>
#include <stdlib.h>
#include "fr.h"

int main(int argc, char **argv) {

    int N = atoi(argv[1]);

    Fr_init();

    FrElement a;
    a.type = Fr_SHORT;
    a.shortVal = 2;

    FrElement b;
    a.type = Fr_SHORT;
    a.shortVal = 6;

    FrElement c;

    Fr_mul(&c, &a, &b);

    char *c1 = Fr_element2str(&c);
    printf("Result: %s\n", c);
    free(c1);
}
`

Compile it

`
g++ main.c fr.o fr.c -o example -lgmp
`

Run it
`
./example
`

## License

# Finite field library in Wasm and Javascript

## License

ffiasm is part of the iden3 project copyright 2020 0KIMS association and published with GPL-3 license. Please check the COPYING file for more details.

