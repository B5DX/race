#include <stdio.h>
#include "tmp.h"

#pragma past begin
#pragma past end
int foo(int c) {
    #pragma past begin
    if (c > 10) {
        return c + 100;
    } else {
        return 94;
    }
    #pragma past end
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
    #pragma past begin
    for (int i = 0; i < 10; i++) {
        ;
        // printf("%d %d\n", x, y);
    }
    #pragma past end
    #pragma omp parallel for
    for (int i = 0; i < 10; i++) {
        ;
        // printf("%d %d\n", x, y);
    }
}

int main() {
    #pragma past begin
    // test(1, 2);
    func();
    #pragma past end
    return 0;
}