#include "common.hpp"

using namespace std;

#define tf (3d0/4)
#define i6 (1d0/6)
#define i144 (1d0/144)
#define i12 (1d0/12)
#define d4a (2d0/3)
#define d4b (-(1d0/12))

int height = 304;
int weight_y = 304;
int weight_x = 304;
int T = 1;

extern "C" void derivative_gold (double *h_r1, double *h_u1, double *h_u2, double *h_u3, double *h_mu, double *h_la, double *h_met1, double *h_met2, double *h_met3, double *h_met4, double *strx, double *stry, double c1, double c2, int N) {
	auto r1 = (double (*)[weight_y][weight_x])h_r1;
	auto u1 = (double (*)[weight_y][weight_x])h_u1;
	auto u2 = (double (*)[weight_y][weight_x])h_u2;
	auto u3 = (double (*)[weight_y][weight_x])h_u3;
	auto mu = (double (*)[weight_y][weight_x])h_mu;
	auto la = (double (*)[weight_y][weight_x])h_la;
	auto met1 = (double (*)[weight_y][weight_x])h_met1;
	auto met2 = (double (*)[weight_y][weight_x])h_met2;
	auto met3 = (double (*)[weight_y][weight_x])h_met3;
	auto met4 = (double (*)[weight_y][weight_x])h_met4;
	int i, j, k;
	double t1, t2, t3, t4, t5;
	#pragma race var
	
	#pragma race begin
	for (i = 2; i < N-2; i++) {
		for (j = 2; j < N-2; j++) {
			for (k = 2; k < N-2; k++) {
				t1 =
					c2*(  mu[i][j+2][k]*met1[i][j+2][k]*met1[i][j+2][k]*(
								c2*(u2[i+2][j+2][k]-u2[i-2][j+2][k]) +
								c1*(u2[i+1][j+2][k]-u2[i-1][j+2][k])    )
							+  mu[i][j-2][k]*met1[i][j-2][k]*met1[i][j-2][k]*(
								c2*(u2[i+2][j-2][k]-u2[i-2][j-2][k])+
								c1*(u2[i+1][j-2][k]-u2[i-1][j-2][k])     )
					   ) +
					c1*(  mu[i][j+1][k]*met1[i][j+1][k]*met1[i][j+1][k]*(
								c2*(u2[i+2][j+1][k]-u2[i-2][j+1][k]) +
								c1*(u2[i+1][j+1][k]-u2[i-1][j+1][k])  )
							+ mu[i][j-1][k]*met1[i][j-1][k]*met1[i][j-1][k]*(
								c2*(u2[i+2][j-1][k]-u2[i-2][j-1][k]) +
								c1*(u2[i+1][j-1][k]-u2[i-1][j-1][k])))
					+
					c2*(  la[i+2][j][k]*met1[i+2][j][k]*met1[i+2][j][k]*(
								c2*(u2[i+2][j+2][k]-u2[i+2][j-2][k]) +
								c1*(u2[i+2][j+1][k]-u2[i+2][j-1][k])    )
							+ la[i-2][j][k]*met1[i-2][j][k]*met1[i-2][j][k]*(
								c2*(u2[i-2][j+2][k]-u2[i-2][j-2][k])+
								c1*(u2[i-2][j+1][k]-u2[i-2][j-1][k])     )
					   ) +
					c1*(  la[i+1][j][k]*met1[i+1][j][k]*met1[i+1][j][k]*(
								c2*(u2[i+1][j+2][k]-u2[i+1][j-2][k]) +
								c1*(u2[i+1][j+1][k]-u2[i+1][j-1][k])  )
							+ la[i-1][j][k]*met1[i-1][j][k]*met1[i-1][j][k]*(
								c2*(u2[i-1][j+2][k]-u2[i-1][j-2][k]) +
								c1*(u2[i-1][j+1][k]-u2[i-1][j-1][k])));

				t2 = c2*(
						(2*mu[i][j][k+2]+la[i][j][k+2])*met2[i][j][k+2]*met1[i][j][k+2]*(
							c2*(u1[i+2][j][k+2]-u1[i-2][j][k+2]) +
							c1*(u1[i+1][j][k+2]-u1[i-1][j][k+2])   )*strx[i]*stry[j]
						+ mu[i][j][k+2]*met3[i][j][k+2]*met1[i][j][k+2]*(
							c2*(u2[i+2][j][k+2]-u2[i-2][j][k+2]) +
							c1*(u2[i+1][j][k+2]-u2[i-1][j][k+2])  )
						+ mu[i][j][k+2]*met4[i][j][k+2]*met1[i][j][k+2]*(
							c2*(u3[i+2][j][k+2]-u3[i-2][j][k+2]) +
							c1*(u3[i+1][j][k+2]-u3[i-1][j][k+2])  )*stry[j]
						+ ((2*mu[i][j][k-2]+la[i][j][k-2])*met2[i][j][k-2]*met1[i][j][k-2]*(
								c2*(u1[i+2][j][k-2]-u1[i-2][j][k-2]) +
								c1*(u1[i+1][j][k-2]-u1[i-1][j][k-2])  )*strx[i]*stry[j]
							+ mu[i][j][k-2]*met3[i][j][k-2]*met1[i][j][k-2]*(
								c2*(u2[i+2][j][k-2]-u2[i-2][j][k-2]) +
								c1*(u2[i+1][j][k-2]-u2[i-1][j][k-2])   )
							+ mu[i][j][k-2]*met4[i][j][k-2]*met1[i][j][k-2]*(
								c2*(u3[i+2][j][k-2]-u3[i-2][j][k-2]) +
								c1*(u3[i+1][j][k-2]-u3[i-1][j][k-2])   )*stry[j] )
						) + c1*(
							(2*mu[i][j][k+1]+la[i][j][k+1])*met2[i][j][k+1]*met1[i][j][k+1]*(
								c2*(u1[i+2][j][k+1]-u1[i-2][j][k+1]) +
								c1*(u1[i+1][j][k+1]-u1[i-1][j][k+1]) )*strx[i+2]*stry[j]
							+ mu[i][j][k+1]*met3[i][j][k+1]*met1[i][j][k+1]*(
								c2*(u2[i+2][j][k+1]-u2[i-2][j][k+1]) +
								c1*(u2[i+1][j][k+1]-u2[i-1][j][k+1]) )
							+ mu[i][j][k+1]*met4[i][j][k+1]*met1[i][j][k+1]*(
								c2*(u3[i+2][j][k+1]-u3[i-2][j][k+1]) +
								c1*(u3[i+1][j][k+1]-u3[i-1][j][k+1])  )*stry[j]
							+ ((2*mu[i][j][k-1]+la[i][j][k-1])*met2[i][j][k-1]*met1[i][j][k-1]*(
									c2*(u1[i+2][j][k-1]-u1[i-2][j][k-1]) +
									c1*(u1[i+1][j][k-1]-u1[i-1][j][k-1]) )*strx[i-2]*stry[j]
								+ mu[i][j][k-1]*met3[i][j][k-1]*met1[i][j][k-1]*(
									c2*(u2[i+2][j][k-1]-u2[i-2][j][k-1]) +
									c1*(u2[i+1][j][k-1]-u2[i-1][j][k-1]) )
								+ mu[i][j][k-1]*met4[i][j][k-1]*met1[i][j][k-1]*(
									c2*(u3[i+2][j][k-1]-u3[i-2][j][k-1]) +
									c1*(u3[i+1][j][k-1]-u3[i-1][j][k-1])   )*stry[j]  ) );

				t3 = ( c2*(
							(2*mu[i+2][j][k]+la[i+2][j][k])*met2[i+2][j][k]*met1[i+2][j][k]*(
								c2*(u1[i+2][j][k+2]-u1[i+2][j][k-2]) +
								c1*(u1[i+2][j][k+1]-u1[i+2][j][k-1])   )*strx[i]
							+ la[i+2][j][k]*met3[i+2][j][k]*met1[i+2][j][k]*(
								c2*(u2[i+2][j][k+2]-u2[i+2][j][k-2]) +
								c1*(u2[i+2][j][k+1]-u2[i+2][j][k-1])  )*stry[j]
							+ la[i+2][j][k]*met4[i+2][j][k]*met1[i+2][j][k]*(
								c2*(u3[i+2][j][k+2]-u3[i+2][j][k-2]) +
								c1*(u3[i+2][j][k+1]-u3[i+2][j][k-1])  )
							+ ((2*mu[i-2][j][k]+la[i-2][j][k])*met2[i-2][j][k]*met1[i-2][j][k]*(
									c2*(u1[i-2][j][k+2]-u1[i-2][j][k-2]) +
									c1*(u1[i-2][j][k+1]-u1[i-2][j][k-1])  )*strx[i]
								+ la[i-2][j][k]*met3[i-2][j][k]*met1[i-2][j][k]*(
									c2*(u2[i-2][j][k+2]-u2[i-2][j][k-2]) +
									c1*(u2[i-2][j][k+1]-u2[i-2][j][k-1])   )*stry[j]
								+ la[i-2][j][k]*met4[i-2][j][k]*met1[i-2][j][k]*(
									c2*(u3[i-2][j][k+2]-u3[i-2][j][k-2]) +
									c1*(u3[i-2][j][k+1]-u3[i-2][j][k-1])   ) )
						    ) + c1*(
							    (2*mu[i+1][j][k]+la[i+1][j][k])*met2[i+1][j][k]*met1[i+1][j][k]*(
								    c2*(u1[i+1][j][k+2]-u1[i+1][j][k-2]) +
								    c1*(u1[i+1][j][k+1]-u1[i+1][j][k-1]) )*strx[i]
							    + la[i+1][j][k]*met3[i+1][j][k]*met1[i+1][j][k]*(
								    c2*(u2[i+1][j][k+2]-u2[i+1][j][k-2]) +
								    c1*(u2[i+1][j][k+1]-u2[i+1][j][k-1]) )*stry[j]
							    + la[i+1][j][k]*met4[i+1][j][k]*met1[i+1][j][k]*(
								    c2*(u3[i+1][j][k+2]-u3[i+1][j][k-2]) +
								    c1*(u3[i+1][j][k+1]-u3[i+1][j][k-1])  )
							    + ((2*mu[i-1][j][k]+la[i-1][j][k])*met2[i-1][j][k]*met1[i-1][j][k]*(
									    c2*(u1[i-1][j][k+2]-u1[i-1][j][k-2]) +
									    c1*(u1[i-1][j][k+1]-u1[i-1][j][k-1]) )*strx[i]
								    + la[i-1][j][k]*met3[i-1][j][k]*met1[i-1][j][k]*(
									    c2*(u2[i-1][j][k+2]-u2[i-1][j][k-2]) +
									    c1*(u2[i-1][j][k+1]-u2[i-1][j][k-1]) )*stry[j]
								    + la[i-1][j][k]*met4[i-1][j][k]*met1[i-1][j][k]*(
									    c2*(u3[i-1][j][k+2]-u3[i-1][j][k-2]) +
									    c1*(u3[i-1][j][k+1]-u3[i-1][j][k-1])   )  ) ) )*stry[j];

				t4 = c2*(
						mu[i][j][k+2]*met3[i][j][k+2]*met1[i][j][k+2]*(
							c2*(u1[i][j+2][k+2]-u1[i][j-2][k+2]) +
							c1*(u1[i][j+1][k+2]-u1[i][j-1][k+2])   )*stry[j+2]*strx[i]
						+ la[i][j][k+2]*met2[i][j][k+2]*met1[i][j][k+2]*(
							c2*(u2[i][j+2][k+2]-u2[i][j-2][k+2]) +
							c1*(u2[i][j+1][k+2]-u2[i][j-1][k+2])  )
						+ ( mu[i][j][k-2]*met3[i][j][k-2]*met1[i][j][k-2]*(
								c2*(u1[i][j+2][k-2]-u1[i][j-2][k-2]) +
								c1*(u1[i][j+1][k-2]-u1[i][j-1][k-2])  )*stry[j]*strx[i]
							+ la[i][j][k-2]*met2[i][j][k-2]*met1[i][j][k-2]*(
								c2*(u2[i][j+2][k-2]-u2[i][j-2][k-2]) +
								c1*(u2[i][j+1][k-2]-u2[i][j-1][k-2])   ) )
						) + c1*(
							mu[i][j][k+1]*met3[i][j][k+1]*met1[i][j][k+1]*(
								c2*(u1[i][j+2][k+1]-u1[i][j-2][k+1]) +
								c1*(u1[i][j+1][k+1]-u1[i][j-1][k+1]) )*stry[j-2]*strx[i]
							+ la[i][j][k+1]*met2[i][j][k+1]*met1[i][j][k+1]*(
								c2*(u2[i][j+2][k+1]-u2[i][j-2][k+1]) +
								c1*(u2[i][j+1][k+1]-u2[i][j-1][k+1]) )
							+ ( mu[i][j][k-1]*met3[i][j][k-1]*met1[i][j][k-1]*(
									c2*(u1[i][j+2][k-1]-u1[i][j-2][k-1]) +
									c1*(u1[i][j+1][k-1]-u1[i][j-1][k-1]) )*stry[j]*strx[i]
								+ la[i][j][k-1]*met2[i][j][k-1]*met1[i][j][k-1]*(
									c2*(u2[i][j+2][k-1]-u2[i][j-2][k-1]) +
									c1*(u2[i][j+1][k-1]-u2[i][j-1][k-1]) ) ) );

				t5 = c2*(
						mu[i][j+2][k]*met3[i][j+2][k]*met1[i][j+2][k]*(
							c2*(u1[i][j+2][k+2]-u1[i][j+2][k-2]) +
							c1*(u1[i][j+2][k+1]-u1[i][j+2][k-1])   )*stry[j+1]*strx[i]
						+ mu[i][j+2][k]*met2[i][j+2][k]*met1[i][j+2][k]*(
							c2*(u2[i][j+2][k+2]-u2[i][j+2][k-2]) +
							c1*(u2[i][j+2][k+1]-u2[i][j+2][k-1])  )
						+ ( mu[i][j-2][k]*met3[i][j-2][k]*met1[i][j-2][k]*(
								c2*(u1[i][j-2][k+2]-u1[i][j-2][k-2]) +
								c1*(u1[i][j-2][k+1]-u1[i][j-2][k-1])  )*stry[j]*strx[i]
							+ mu[i][j-2][k]*met2[i][j-2][k]*met1[i][j-2][k]*(
								c2*(u2[i][j-2][k+2]-u2[i][j-2][k-2]) +
								c1*(u2[i][j-2][k+1]-u2[i][j-2][k-1])   ) )
						) + c1*(
							mu[i][j+1][k]*met3[i][j+1][k]*met1[i][j+1][k]*(
								c2*(u1[i][j+1][k+2]-u1[i][j+1][k-2]) +
								c1*(u1[i][j+1][k+1]-u1[i][j+1][k-1]) )*stry[j-1]*strx[i]
							+ mu[i][j+1][k]*met2[i][j+1][k]*met1[i][j+1][k]*(
								c2*(u2[i][j+1][k+2]-u2[i][j+1][k-2]) +
								c1*(u2[i][j+1][k+1]-u2[i][j+1][k-1]) )
							+ ( mu[i][j-1][k]*met3[i][j-1][k]*met1[i][j-1][k]*(
									c2*(u1[i][j-1][k+2]-u1[i][j-1][k-2]) +
									c1*(u1[i][j-1][k+1]-u1[i][j-1][k-1]) )*stry[j]*strx[i]
								+ mu[i][j-1][k]*met2[i][j-1][k]*met1[i][j-1][k]*(
									c2*(u2[i][j-1][k+2]-u2[i][j-1][k-2]) +
									c1*(u2[i][j-1][k+1]-u2[i][j-1][k-1]) ) ) );
				r1[i][j][k] = r1[i][j][k] + t1 + t2 + t3 + t4 + t5;
			}
		}
	}
	#pragma race end
}

int main(int argc, char **argv) {
	weight_x = stoi(argv[1]);
	weight_y = stoi(argv[2]);
	height = stoi(argv[3]);
	T = stoi(argv[4]);
	if (weight_x != weight_y || weight_x != height || weight_y != height) {
		cout << "Error: weight_x, weight_y, and height must be equal" << endl;
		return 1;
	}
	double *h_r1 = new double[weight_x*weight_y*height];
	double *h_u1 = new double[weight_x*weight_y*height];
	double *h_u2 = new double[weight_x*weight_y*height];
	double *h_u3 = new double[weight_x*weight_y*height];
	double *h_mu = new double[weight_x*weight_y*height];
	double *h_la = new double[weight_x*weight_y*height];
	double *h_met1 = new double[weight_x*weight_y*height];
	double *h_met2 = new double[weight_x*weight_y*height];
	double *h_met3 = new double[weight_x*weight_y*height];
	double *h_met4 = new double[weight_x*weight_y*height];
	double *strx = new double[weight_x*weight_y*height];
	double *stry = new double[weight_x*weight_y*height];
	double c1 = 0.1;
	double c2 = 0.2;
	int N = height;
	srand(time(0));
	randomlyInit(h_r1, weight_x*weight_y*height);
	randomlyInit(h_u1, weight_x*weight_y*height);
	randomlyInit(h_u2, weight_x*weight_y*height);
	randomlyInit(h_u3, weight_x*weight_y*height);
	randomlyInit(h_mu, weight_x*weight_y*height);
	randomlyInit(h_la, weight_x*weight_y*height);
	randomlyInit(h_met1, weight_x*weight_y*height);
	randomlyInit(h_met2, weight_x*weight_y*height);
	randomlyInit(h_met3, weight_x*weight_y*height);
	randomlyInit(h_met4, weight_x*weight_y*height);
	randomlyInit(strx, weight_x*weight_y*height);
	randomlyInit(stry, weight_x*weight_y*height);
	LIKWID_MARKER_INIT;
	LIKWID_MARKER_THREADINIT;
	double time_taken = 0.0;
    auto start = chrono::high_resolution_clock::now();
	LIKWID_MARKER_START("derivative");
    for (int t = 0; t < T; t++) {
		derivative_gold(h_r1, h_u1, h_u2, h_u3, h_mu, h_la, h_met1, h_met2, h_met3, h_met4, strx, stry, c1, c2, N);
    }
	LIKWID_MARKER_STOP("derivative");
    auto end = chrono::high_resolution_clock::now();
    time_taken = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
    time_taken *= 1e-9;
    cout << "Elapsed time: " << time_taken << setprecision(9) << endl;  
	LIKWID_MARKER_CLOSE;
	return 0;
}