#include <iostream>
#include <math.h>
#include <ctime>
#include <random>
using namespace std;

const int nblocks_clinic = 100, ny_block = 1000, nx_block = 100;
double ULAT[nblocks_clinic][ny_block][nx_block], ULON[nblocks_clinic][ny_block][nx_block];
double iv_0_1[ny_block + 1][nx_block + 1], iv_0_0[ny_block + 1][nx_block + 1], iv_0_3[ny_block + 1][nx_block + 1], iv_0_2[ny_block + 1][nx_block + 1];
double xsw, ysw, xs, ys, xw, yw, xc, yc, tx, ty, tz;
double p25 = 25;

void init_variables() {
    for (int n = 0; n < nblocks_clinic; n++) {
        for (int j = 0; j < ny_block; j++) {
            for (int i = 0; i < nx_block; i++) {
                ULAT[n][j][i] = rand() % 100;
                ULON[n][j][i] = rand() % 100;
            }
        }
    }
    for (int j = 0; j < ny_block; j++) {
        for (int i = 0; i < nx_block; i++) {            
            iv_0_1[j][i] = rand() % 100;
            iv_0_0[j][i] = rand() % 100;
            iv_0_3[j][i] = rand() % 100;
            iv_0_2[j][i] = rand() % 100;
        }
    }
    xsw = rand() % 100;
    ysw = rand() % 100;
    xs = rand() % 100;
    ys = rand() % 100;
    xw = rand() % 100;
    yw = rand() % 100;
    xc = rand() % 100;
    yc = rand() % 100;
    tx = rand() % 100;
    ty = rand() % 100;
    tz = rand() % 100;
}

int main() {
    init_variables();
    // record time
    clock_t start, end;
    start = clock();
    for (int n = 1; n < nblocks_clinic; n++) {
        for (int j = 2 - 1; j < ny_block; j++) {
            for (int i = 2 - 1; i < nx_block; i++) {
                iv_0_0[j][i] = cos(ULON[n][j][i]);
            }
        }
        for (int j = 2 - 1; j < ny_block; j++) {
            for (int i = 2 - 1; i < nx_block; i++) {
                iv_0_1[j][i] = cos(ULAT[n][j][i]);
            }
        }
        for (int j = 2 - 1; j < ny_block; j++) {
            for (int i = 2 - 1; i < nx_block; i++) {
                iv_0_2[j][i] = sin(ULAT[n][j][i]);
            }
        }
        for (int j = 2 - 1; j < ny_block; j++) {
            for (int i = 2 - 1; i < nx_block; i++) {
                iv_0_3[j][i] = sin(ULON[n][j][i]);
            }
        }
        for (int j = 2; j < ny_block; j++) {
            for (int i = 2; i < nx_block; i++) {
                xsw = iv_0_1[j - 1][i - 1] * iv_0_0[j - 1][i - 1];
                ysw = iv_0_1[j - 1][i - 1] * iv_0_3[j - 1][i - 1];
                xs = iv_0_1[j - 1][i] * iv_0_0[j - 1][i];
                ys = iv_0_1[j - 1][i] * iv_0_3[j - 1][i];
                xw = iv_0_1[j][i - 1] * iv_0_0[j][i - 1];
                yw = iv_0_1[j][i - 1] * iv_0_3[j][i - 1];
                xc = iv_0_1[j][i] * iv_0_0[j][i];
                yc = iv_0_1[j][i] * iv_0_3[j][i];
                tx = p25 * (xc + xs + xw + xsw);
                ty = p25 * (yc + ys + yw + ysw);
                tz = p25 * (iv_0_2[j][i] + iv_0_2[j - 1][i] + iv_0_2[j][i - 1] + iv_0_2[j - 1][i - 1]);
            }
        }
    }
    end = clock();
    cout << "Time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    return 0;
}