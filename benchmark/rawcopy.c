#include <stdio.h>
#include <stdlib.h>
#include "fr.h"

int main(int argc, char **argv) {

    int N = atoi(argv[1]);

    Fr_init();

    FrElement a;
    a.type = Fr_LONGMONTGOMERY;
    for (int i=0; i<Fr_N64; i++) {
        a.longVal[i] = 0xAAAAAAAA;
    }

    FrElement b;
    b.type = Fr_LONGMONTGOMERY;
    for (int i=0; i<Fr_N64; i++) {
        b.longVal[i] = 0xBBBBBBBB;
    }

    for (int i=0; i<N; i++) {
        Fr_rawCopy(a.longVal, b.longVal);
    }

    /*
    char *c1 = Fr_element2str(&a);
    printf("Result: %s\n", a);
    free(c1);
    */
}
