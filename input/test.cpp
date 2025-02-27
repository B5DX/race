#include <stdio.h>
#include "tmp.h"

#pragma race begin
#pragma race end
int foo(int c) {
    #pragma race begin
    if (c > 10) {
        return c + 100;
    } else {
        return 94;
    }
    #pragma race end
}
void bar(int x) {
    int a;
    if (x > 10) {
        a = 87;
    } else {
        a = x - 100;
    }
}


void test(int x, int y) {
    #pragma race begin
    int i = 0;
    int ims = 0, jms = 1;
    int a, b, c;
    int **C;
    for (int j = jms; j <= 20; j++) {
        for (i = -ims+1; i < 10; i++) {
            // Y[i] = Z[i] * W[i];
            X[i][j] = Z[i] * C[j][i+1] + Y[i] * W[i];
            a = b * c + a * b;
        }
        // for (i = -ims+1; i < 10; i++) {
        //     Y[i] = Z[i] * W[i];
        //     a = b * c + a * b;
        // }
        // Y[i] = C[j][i+1] + Z[i] * W[i];
        // a = b * c + a * b;
    }
    
    #pragma race end
    #pragma omp parallel for
    for (int i = 0; i < 10; i++) {
        ;
        // printf("%d %d\n", x, y);
    }
}

int main() {
    #pragma race begin
    // test(1, 2);
    func();
    #pragma race end
    return 0;
}