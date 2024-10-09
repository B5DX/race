#include <iostream>
#include <time.h>
#include <math.h>
using namespace std;

// #define j_start 5
// #define jtf 196
// #define k_start 5
// #define ktf 48
// #define i_start 5
// #define itf 214
#define j_start 5
#define jtf 205
#define k_start 5
#define ktf 55
#define i_start 5
#define itf 205
#define T 100
double mkrdxm;
double mkrdxp;
double mrdx;
double mkrdym;
double mkrdyp;
double mrdy;
double rdx = 0.5;
double rdy = 0.5;
double xkmhd[jtf + 10][ktf + 10][itf + 10];
double MUT[jtf + 10][itf + 10];
double msfux[jtf + 10][itf + 10];
double msfuy[jtf + 10][itf + 10];
double msftx[jtf + 10][itf + 10];
double msfty[jtf + 10][itf + 10];
double msfvy[jtf + 10][itf + 10];
double msfvx_inv[jtf + 10][itf + 10];
double c1[ktf + 10];
double c2[ktf + 10];
double field[jtf + 10][ktf + 10][itf + 10];
double tendency[jtf + 10][ktf + 10][itf + 10];
double tmp[jtf + 10][ktf + 10][itf + 10];

int ***A, B[10][10], C[10], D[10];

int main() {
    int i, j, k, x, y, z;
    int test1, test2;
    // Wrong
    // #pragma avn begin
    // for (x = j_start; x <= jtf; x++) {
    //     for (y = k_start; y <= ktf; y++) {
    //         for (z = i_start; z <= itf; z++) {
    //             A[x][y][z] = 0.54 * (A[x][y][z]) + \
	// 									0.03 * (A[x][y][z-1] + A[x][y-1][z] + \
	// 											A[x][y+1][z] + A[x][y][z+1] + \
	// 											A[x-1][y][z] + A[x+1][y][z])+ \
	// 									0.01 * (A[x-1][y][z-1] + A[x-1][y-1][z] + \
	// 											A[x-1][y+1][z] + A[x-1][y][z+1] + \
	// 											A[x][y-1][z-1] + A[x][y+1][z-1] + \
	// 											A[x][y-1][z+1] + A[x][y+1][z+1] + \
	// 											A[x+1][y][z-1] + A[x+1][y-1][z] + \
	// 											A[x+1][y+1][z] + A[x+1][y][z+1]) + \
	// 									0.02 * (A[x-1][y-1][z-1] + A[x-1][y+1][z-1] + \
	// 											A[x-1][y-1][z+1] + A[x-1][y+1][z+1] + \
	// 											A[x+1][y-1][z-1] + A[x+1][y+1][z-1] + \
	// 											A[x+1][y-1][z+1] + A[x+1][y+1][z+1]);
    //         }
    //     }
    // }
    // #pragma avn end

    // Correct
    // #pragma avn begin
    // for (k = k_start; k <= ktf; k++) {
    //     for (j = j_start; j <= jtf; j++) {
    //         for (i = i_start; i <= itf; i++) {
    //             A[k][j][i] = 0.54 * (A[k][j][i]) + \
	// 									0.03 * (A[k][j][i-1] + A[k][j-1][i] + \
	// 											A[k][j+1][i] + A[k][j][i+1] + \
	// 											A[k-1][j][i] + A[k+1][j][i])+ \
	// 									0.01 * (A[k-1][j][i-1] + A[k-1][j-1][i] + \
	// 											A[k-1][j+1][i] + A[k-1][j][i+1] + \
	// 											A[k][j-1][i-1] + A[k][j+1][i-1] + \
	// 											A[k][j-1][i+1] + A[k][j+1][i+1] + \
	// 											A[k+1][j][i-1] + A[k+1][j-1][i] + \
	// 											A[k+1][j+1][i] + A[k+1][j][i+1]) + \
	// 									0.02 * (A[k-1][j-1][i-1] + A[k-1][j+1][i-1] + \
	// 											A[k-1][j-1][i+1] + A[k-1][j+1][i+1] + \
	// 											A[k+1][j-1][i-1] + A[k+1][j+1][i-1] + \
	// 											A[k+1][j-1][i+1] + A[k+1][j+1][i+1]);
    //         }
    //     }
    // }
    // #pragma avn end

    // for (int t = 0; t < T; t++) {
    //     #pragma avn begin
    //     for (j = j_start; j <= jtf; j++) {
    //         for (k = k_start; k <= ktf; k++) {
    //             for (i = i_start; i <= itf; i++) {
    //                 // mkrdxm = (test2*test1 + test2*test1);
    //                 // mkrdxm = (C[i]*D[i] * C[i+1]*D[i+1]);
    //                 // mkrdxm = (test2*test1*C[i-1] + B[i][j-1] + test2*test1*C[i+1] + B[i+2][j-1]);
    //                 // mkrdxm = (test2*test1*C[i] + B[i+2][j] + test2*test1*C[i+3] + B[i+4][j]);
    //                 // mkrdxm = msfux[j][i] * (xkmhd[j][k][i] + xkmhd[j][k][i + 2] + xkmhd[j][k][i + 2] + xkmhd[j][k][i + 4]) * msfuy[j][1+i];
    //                 // A[j][k][i] = 0.54 * (A[j][k][i]) + \
    //                 //     0.03 * (A[j][k][i-1] + A[j][k-1][i] + \
    //                 //             A[j][k+1][i] + A[j][k][i+1] + \
    //                 //             A[j-1][k][i] + A[j+1][k][i]);

    //                 mkrdxm=((((((msfux[j][i]/msfuy[j][i])*0.500000)*(xkmhd[j][k][i]+xkmhd[j][k][i-1]))*0.500000)*(((c1[k]*MUT[j][i])+c2[k])+((c1[k]*MUT[j][i-1])+c2[k])))*rdx);
    //                 mkrdxp=((((((msfux[j][i+1]/msfuy[j][i+1])*0.500000)*(xkmhd[j][k][i+1]+xkmhd[j][k][i]))*0.500000)*(((c1[k]*MUT[j][i+1])+c2[k])+((c1[k]*MUT[j][i])+c2[k])))*rdx);
    //                 mrdx=((msftx[j][i]*msfty[j][i])*rdx);
    //                 mkrdym=((((((msfvy[j][i]*msfvx_inv[j][i])*0.500000)*(xkmhd[j][k][i]+xkmhd[j-1][k][i]))*0.500000)*(((c1[k]*MUT[j][i])+c2[k])+((c1[k]*MUT[j-1][i])+c2[k])))*rdy);
    //                 mkrdyp=((((((msfvy[j+1][i]*msfvx_inv[j+1][i])*0.500000)*(xkmhd[j+1][k][i]+xkmhd[j][k][i]))*0.500000)*(((c1[k]*MUT[j+1][i])+c2[k])+((c1[k]*MUT[j][i])+c2[k])))*rdy);
    //                 mrdy=((msftx[j][i]*msfty[j][i])*rdy);
    //                 tendency[j][k][i]=(tendency[j][k][i]+((mrdx*((mkrdxp*(field[j][k][i+1]-field[j][k][i]))-(mkrdxm*(field[j][k][i]-field[j][k][i-1]))))+(mrdy*((mkrdyp*(field[j+1][k][i]-field[j][k][i]))-(mkrdym*(field[j][k][i]-field[j-1][k][i]))))));
    //                 // tendency[j][k][i] = tendency[j][k][i] + (((msftx[j][i]*msfty[j][i])*rdx) * ((msfux[j][i + 1] / msfuy[j][i + 1] * 0.500000 * (xkmhd[j][k][i + 1] + xkmhd[j][k][i]) * 0.500000 * (c1[k] * MUT[j][i + 1] + c2[k] + (c1[k] * MUT[j][i] + c2[k])) * rdx) * (field[j][k][i + 1] - field[j][k][i]) - (msfux[j][i] / msfuy[j][i] * 0.500000 * (xkmhd[j][k][i] + xkmhd[j][k][i - 1]) * 0.500000 * (c1[k] * MUT[j][i] + c2[k] + (c1[k] * MUT[j][i - 1] + c2[k])) * rdx) * (field[j][k][i] - field[j][k][i - 1])) + ((msftx[j][i]*msfty[j][i])*rdy) * ((msfvy[j + 1][i] * msfvx_inv[j + 1][i] * 0.500000 * (xkmhd[j + 1][k][i] + xkmhd[j][k][i]) * 0.500000 * (c1[k] * MUT[j + 1][i] + c2[k] + (c1[k] * MUT[j][i] + c2[k])) * rdy) * (field[j + 1][k][i] - field[j][k][i]) - (msfvy[j][i] * msfvx_inv[j][i] * 0.500000 * (xkmhd[j][k][i] + xkmhd[j - 1][k][i]) * 0.500000 * (c1[k] * MUT[j][i] + c2[k] + (c1[k] * MUT[j - 1][i] + c2[k])) * rdy) * (field[j][k][i] - field[j - 1][k][i])));

    //             }
    //         }
    //     }
    //     #pragma avn end
    // }

    const int nblocks_clinic = 10;
    const int ny_block = 10;
    const int nx_block = 10;
    int ULAT[nblocks_clinic][ny_block][nx_block];
    int ULON[nblocks_clinic][ny_block][nx_block];
    int zsw, xsw, ysw, zs, xs, ys, zw, xw, yw, zc, xc, yc, tx, ty, tz;
    int p25 = 25;
    int iv_0_1[ny_block + 1][nx_block + 1], iv_0_0[ny_block + 1][nx_block + 1], iv_0_3[ny_block + 1][nx_block + 1], iv_0_2[ny_block + 1][nx_block + 1];

    // #pragma avn begin
    // for (int i = 0; i < 10; i++) {
    //     p25 = msftx[i][j] + msfty[i][j];
    //     p25 = msftx[i][j-1] + msfty[i][j-1];
    // }
    // #pragma avn end

    #pragma avn begin
    for (int n = 1; n <= nblocks_clinic; n++) {
        for (int j = 2; j <= ny_block; j++) {
            for (int i = 2; i <= nx_block; i++) {
                zsw=cos(ULAT[n][j-1][i-1]);
                xsw=cos(ULON[n][j-1][i-1])*zsw;
                ysw=sin(ULON[n][j-1][i-1])*zsw;
                zsw=sin(ULAT[n][j-1][i-1]);
                zs=cos(ULAT[n][j-1][i]);
                xs=cos(ULON[n][j-1][i])*zs;
                ys=sin(ULON[n][j-1][i])*zs;
                zs=sin(ULAT[n][j-1][i]);
                zw=cos(ULAT[n][j][i-1]);
                xw=cos(ULON[n][j][i-1])*zw;
                yw=sin(ULON[n][j][i-1])*zw;
                zw=sin(ULAT[n][j][i-1]);
                zc=cos(ULAT[n][j][i]);
                xc=cos(ULON[n][j][i])*zc;
                yc=sin(ULON[n][j][i])*zc;
                zc=sin(ULAT[n][j][i]);
                tx = p25*(xc+xs+xw+xsw);
                ty = p25*(yc+ys+yw+ysw);
                tz = p25*(zc+zs+zw+zsw);
            }
        }
    }
    #pragma avn end

    // #pragma avn begin
    // for (int n = 1; n <= nblocks_clinic; n++) {
    //     for (int j = 2; j <= ny_block; j++) {
    //         for (int i = 2; i <= nx_block; i++) {
    //             // zsw=cos(ULAT[n][j-1][i-1]);
    //             // xsw=cos(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]);
    //             // ysw=sin(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]);
    //             // zsw1=sin(ULAT[n][j-1][i-1]);
    //             // zs=cos(ULAT[n][j-1][i]);
    //             // xs=cos(ULON[n][j-1][i])*cos(ULAT[n][j-1][i]);
    //             // ys=sin(ULON[n][j-1][i])*cos(ULAT[n][j-1][i]);
    //             // zs1=sin(ULAT[n][j-1][i]);
    //             // zw=cos(ULAT[n][j][i-1]);
    //             // xw=cos(ULON[n][j][i-1])*cos(ULAT[n][j][i-1]);
    //             // yw=sin(ULON[n][j][i-1])*cos(ULAT[n][j][i-1]);
    //             // zw1=sin(ULAT[n][j][i-1]);
    //             // zc=cos(ULAT[n][j][i]);
    //             // xc=cos(ULON[n][j][i])*cos(ULAT[n][j][i]);
    //             // yc=sin(ULON[n][j][i])*cos(ULAT[n][j][i]);
    //             // zc1=sin(ULAT[n][j][i]);
    //             tx = p25*(cos(ULON[n][j][i])*cos(ULAT[n][j][i])+cos(ULON[n][j-1][i])*cos(ULAT[n][j-1][i])+cos(ULON[n][j][i-1])*cos(ULAT[n][j][i-1])+cos(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]));
    //             ty = p25*(sin(ULON[n][j][i])*cos(ULAT[n][j][i])+sin(ULON[n][j-1][i])*cos(ULAT[n][j-1][i])+sin(ULON[n][j][i-1])*cos(ULAT[n][j][i-1])+sin(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]));
    //             tz = p25*(sin(ULAT[n][j][i])+sin(ULAT[n][j-1][i])+sin(ULAT[n][j][i-1])+sin(ULAT[n][j-1][i-1]));
    //             tx = p25*(cos(ULON[n][j][i])*cos(ULAT[n][j][i])+cos(ULON[n][j-1][i])*cos(ULAT[n][j-1][i])+cos(ULON[n][j][i-1])*cos(ULAT[n][j][i-1])+cos(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]));
    //             ty = p25*(sin(ULON[n][j][i])*cos(ULAT[n][j][i])+sin(ULON[n][j-1][i])*cos(ULAT[n][j-1][i])+sin(ULON[n][j][i-1])*cos(ULAT[n][j][i-1])+sin(ULON[n][j-1][i-1])*cos(ULAT[n][j-1][i-1]));
    //             tz = p25*(sin(ULAT[n][j][i])+sin(ULAT[n][j-1][i])+sin(ULAT[n][j][i-1])+sin(ULAT[n][j-1][i-1]));
    //         }
    //     }
    // }
    // #pragma avn end

    return 0;
}