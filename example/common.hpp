#include <iostream>
#include <cmath>
#include <random>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <string>

void randomlyInit(double* arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 100;
    }
}

#ifdef LIKWID
#include <likwid-marker.h>
#else
#define LIKWID_MARKER_INIT
#define LIKWID_MARKER_THREADINIT
#define LIKWID_MARKER_START(x)
#define LIKWID_MARKER_STOP(x)
#define LIKWID_MARKER_CLOSE
#endif