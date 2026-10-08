#include "common.hpp"

using namespace std;

static void j3d_step
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
                output[i][j][k] = (0.5*input[i-1][j-1][k-1] + 0.7*input[i-1][j-1][k] + 0.9*input[i-1][j-1][k+1] + 1.2*input[i-1][j][k-1] + 1.5*input[i-1][j][k] + 1.2*input[i-1][j][k+1] + 0.9*input[i-1][j+1][k-1] + 0.7*input[i-1][j+1][k] + 0.5*input[i-1][j+1][k+1] +
                                    0.51*input[i][j-1][k-1] + 0.71*input[i][j-1][k] + 0.91*input[i][j-1][k+1] + 1.21*input[i][j][k-1] + 1.51*input[i][j][k] + 1.21*input[i][j][k+1] + 0.91*input[i][j+1][k-1] + 0.71*input[i][j+1][k] + 0.51*input[i][j+1][k+1] +
                                    0.52*input[i+1][j-1][k-1] + 0.72*input[i+1][j-1][k] + 0.92*input[i+1][j-1][k+1] + 1.22*input[i+1][j][k-1] + 1.52*input[i+1][j][k] + 1.22*input[i+1][j][k+1] + 0.92*input[i+1][j+1][k-1] + 0.72*input[i+1][j+1][k] + 0.52*input[i+1][j+1][k+1]) / 159;
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
    LIKWID_MARKER_START("j3d27pt");
    for (int t = 0; t < T; t++) {
      j3d_step(l_input, height, width_y, width_x, l_output);
    }
    LIKWID_MARKER_STOP("j3d27pt");
    auto end = chrono::high_resolution_clock::now();
    time_taken += chrono::duration_cast<chrono::nanoseconds>(end - start).count();
    time_taken *= 1e-9;
    cout << "Elapsed time: " << time_taken << setprecision(9) << endl;  
    LIKWID_MARKER_CLOSE;
    return 0;
}
