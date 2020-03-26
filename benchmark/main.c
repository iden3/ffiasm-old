#include <stdio.h>
#include <stdlib.h>
#include "fr.h"

int main(int argc, char **argv) {

    int N = atoi(argv[1]);

    Fr_init();

    FrElement a;
    a.type = Fr_SHORT;
    a.shortVal = 2;

    for (int i=0; i<N; i++) {
        Fr_mul(&a, &a, &a);
    }

    char *c1 = Fr_element2str(&a);
    printf("Result: %s\n", a);
    free(c1);
}
