#include <stdio.h>
#include "tmp.h"

#pragma avn begin
#pragma avn end
int foo(int c) {
    #pragma avn begin
    if (c > 10) {
        return c + 100;
    } else {
        return 94;
    }
    #pragma avn end
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
    #pragma avn begin
    int i = 0;
    int ims = 0, jms = 1;
    int a, b, c;
    int **C;
    for (int j = jms; j <= 20; j++) {
        for (i = -ims+1; i < 10; i++) {
            // Y[i] = Z[i] * W[i];
            X[i][j] = C[j][i+1] + Y[i];
            a = b * c + a * b;
        }
    }
    
    #pragma avn end
    #pragma omp parallel for
    for (int i = 0; i < 10; i++) {
        ;
        // printf("%d %d\n", x, y);
    }
}

int main() {
    #pragma avn begin
    // test(1, 2);
    func();
    #pragma avn end
    return 0;
}