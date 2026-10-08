#include "common.hpp"
using namespace std;
#define epsilon (1.0E-20f)
#define RACE


static void poisson_step
(const double* l_input, int height, int width_y, int width_x, double* l_output)
{
  auto input =
    (const double (*)[width_y][width_x])l_input;
  auto output = (double (*)[width_y][width_x])l_output;
  int i, j, k;
  #pragma race var

  #pragma race begin
  for (i = 1; i < height-1; i++)
    for (j = 1; j < width_y-1; j++)
      for (k = 1; k < width_x-1; k++) {
          output[i][j][k] = 2.666f * input[i][j][k] - 
		0.166f * (input[i-1][j][k] + input[i+1][j][k] + input[i][j-1][k] + input[i][j+1][k] + input[i][j][k+1] + input[i][j][k-1]) - 
		0.0833f * (input[i-1][j-1][k] + input[i+1][j-1][k] + input[i-1][j+1][k] + input[i+1][j+1][k] + 
			   input[i-1][j][k-1] + input[i+1][j][k-1] + input[i][j-1][k-1] + input[i][j+1][k-1] + 
			   input[i-1][j][k+1] + input[i+1][j][k+1] + input[i][j-1][k+1] + input[i][j+1][k+1]);
      }
  #pragma race end
}

int main(int argc, char* argv[]) {
    // randomly generate input data
    int width_x = stoi(argv[1]);
    int width_y = stoi(argv[2]);
    int height = stoi(argv[3]);
    int T = stoi(argv[4]);
    double* l_input = new double[height*width_y*width_x];
    double* l_output = new double[height*width_y*width_x];
    srand(time(0));
    randomlyInit(l_input, height*width_y*width_x);
    LIKWID_MARKER_INIT;
    LIKWID_MARKER_THREADINIT;
    double time_taken = 0.0;
    auto start = chrono::high_resolution_clock::now();
    LIKWID_MARKER_START("poisson");
    for (int t = 0; t < T; t++) {
      poisson_step(l_input, height, width_y, width_x, l_output);
    }
    LIKWID_MARKER_STOP("poisson");
    auto end = chrono::high_resolution_clock::now();
    time_taken += chrono::duration_cast<chrono::nanoseconds>(end - start).count();
    time_taken *= 1e-9;
    cout << "Elapsed time: " << time_taken << setprecision(9) << endl;  
    LIKWID_MARKER_CLOSE;
    return 0;
}
  