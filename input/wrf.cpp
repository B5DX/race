#include <iostream>
#include <time.h>
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

int A[10], B[10], C[10], D[10];

int main() {
    int i, j, k;
    int tst;
    for (int t = 0; t < T; t++) {
        #pragma avn begin
        for (j = j_start; j <= jtf; j++) {
            for (k = k_start; k <= ktf; k++) {
                for (i = i_start; i <= itf; i++) {
                    mkrdxm = (tst*0.5*A[i] + B[i+1] + tst*0.5*A[i+2] + B[i+3]+A[i] + B[i+1] + A[i+2] + B[i+3]+A[i] + B[i+1] + A[i+2] + B[i+3]);
                    // mkrdxm = msfux[j][i] * msfuy[j][1+i] * (xkmhd[j][k][i] + xkmhd[j][k][i - 1] + xkmhd[j][k][i - 1]);

                    // mkrdxm=((((((msfux[j][i]/msfuy[j][i])*0.500000)*(xkmhd[j][k][i]+xkmhd[j][k][i-1]))*0.500000)*(((c1[k]*MUT[j][i])+c2[k])+((c1[k]*MUT[j][i-1])+c2[k])))*rdx);
                    // mkrdxp=((((((msfux[j][i+1]/msfuy[j][i+1])*0.500000)*(xkmhd[j][k][i+1]+xkmhd[j][k][i]))*0.500000)*(((c1[k]*MUT[j][i+1])+c2[k])+((c1[k]*MUT[j][i])+c2[k])))*rdx);
                    // // // mrdx=((msftx[j][i]*msfty[j][i])*rdx);
                    // mkrdym=((((((msfvy[j][i]*msfvx_inv[j][i])*0.500000)*(xkmhd[j][k][i]+xkmhd[j-1][k][i]))*0.500000)*(((c1[k]*MUT[j][i])+c2[k])+((c1[k]*MUT[j-1][i])+c2[k])))*rdy);
                    // mkrdyp=((((((msfvy[j+1][i]*msfvx_inv[j+1][i])*0.500000)*(xkmhd[j+1][k][i]+xkmhd[j][k][i]))*0.500000)*(((c1[k]*MUT[j+1][i])+c2[k])+((c1[k]*MUT[j][i])+c2[k])))*rdy);
                    // // // mrdy=((msftx[j][i]*msfty[j][i])*rdy);
                    // tendency[j][k][i]=(tendency[j][k][i]+((mrdx*((mkrdxp*(field[j][k][i+1]-field[j][k][i]))-(mkrdxm*(field[j][k][i]-field[j][k][i-1]))))+(mrdy*((mkrdyp*(field[j+1][k][i]-field[j][k][i]))-(mkrdym*(field[j][k][i]-field[j-1][k][i]))))));
                }
            }
        }
        #pragma avn end
    }
    return 0;
}