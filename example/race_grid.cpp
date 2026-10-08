#include <iostream>
#include <math.h>
#include <ctime>
#include <random>
using namespace std;

const int nblocks_clinic = 100, ny_block = 100, nx_block = 1000;
double ULAT[nblocks_clinic][ny_block][nx_block], ULON[nblocks_clinic][ny_block][nx_block];
double xsw, ysw, xs, ys, xw, yw, xc, yc, tx, ty, tz, zsw, zw, zs, zc;
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
    int n, j, i;
    init_variables();
double iv_0_0[2][nx_block - 1 + 1];
double iv_0_3[2][nx_block - 1 + 1];
double iv_0_4[2][nx_block - 1 + 1];
double iv_0_5[2][nx_block - 1 + 1];
double iv_0_6[nx_block - 1 + 1];
double iv_0_7[nx_block - 1 + 1];
double iv_0_8[nx_block - 1 + 1];
    // record time
    clock_t start, end;
    start = clock();
    #pragma race begin
    {
for (n = 1; n <= nblocks_clinic - 1; n++) {
    for (j = 2 - 1; j <= 2 - 1; j++) {
        for (i = 2 - 1; i <= nx_block - 1; i++) {
            iv_0_0[j % 2][i] = cos(ULAT[n][j][i]);
            iv_0_3[j % 2][i] = sin(ULAT[n][j][i]);
            iv_0_4[j % 2][i] = sin(ULON[n][j][i]) * iv_0_0[j % 2][i];
            iv_0_5[j % 2][i] = cos(ULON[n][j][i]) * iv_0_0[j % 2][i];
        }
    }
    for (j = 2; j <= ny_block - 1; j++) {
        for (i = 2 - 1; i <= nx_block - 1; i++) {
            iv_0_0[j % 2][i] = cos(ULAT[n][j][i]);
            iv_0_3[j % 2][i] = sin(ULAT[n][j][i]);
            iv_0_4[j % 2][i] = sin(ULON[n][j][i]) * iv_0_0[j % 2][i];
            iv_0_5[j % 2][i] = cos(ULON[n][j][i]) * iv_0_0[j % 2][i];
        }
        for (i = 2 - 1; i <= nx_block - 1; i++) {
            iv_0_6[i] = iv_0_3[(j - 1) % 2][i] + iv_0_3[j % 2][i];
            iv_0_7[i] = iv_0_5[(j - 1) % 2][i] + iv_0_5[j % 2][i];
            iv_0_8[i] = iv_0_4[(j - 1) % 2][i] + iv_0_4[j % 2][i];
        }
        for (i = 2; i <= nx_block - 1; i++) {
            zsw = iv_0_0[(j - 1) % 2][i - 1];
            xsw = iv_0_5[(j - 1) % 2][i - 1];
            ysw = iv_0_4[(j - 1) % 2][i - 1];
            zsw = iv_0_3[(j - 1) % 2][i - 1];
            zs = iv_0_0[(j - 1) % 2][i];
            xs = iv_0_5[(j - 1) % 2][i];
            ys = iv_0_4[(j - 1) % 2][i];
            zs = iv_0_3[(j - 1) % 2][i];
            zw = iv_0_0[j % 2][i - 1];
            xw = iv_0_5[j % 2][i - 1];
            yw = iv_0_4[j % 2][i - 1];
            zw = iv_0_3[j % 2][i - 1];
            zc = iv_0_0[j % 2][i];
            xc = iv_0_5[j % 2][i];
            yc = iv_0_4[j % 2][i];
            zc = iv_0_3[j % 2][i];
            tx = p25 * (iv_0_7[i] + iv_0_7[i - 1]);
            ty = p25 * (iv_0_8[i] + iv_0_8[i - 1]);
            tz = p25 * (iv_0_6[i] + iv_0_6[i - 1]);
        }
    }
}
}
    #pragma race end
    end = clock();
    cout << "Time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    return 0;
}