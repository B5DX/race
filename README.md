# RACE: Redundant Array Computation Elimination

RACE is a Clang LibTooling source transformer for loop-carried redundant
expressions. It extracts shared array computations into auxiliary arrays and
can contract their dimensions. It writes `race_<filename>` beside each input;
the original source is preserved.

## Paper

This repository contains the implementation accompanying **Redundant Array
Computation Elimination** by Zixuan Wang, Liang Yuan, Xianmeng Jiang, Kun Li,
Junmin Xiao, and Yunquan Zhang, published in *Proceedings of the ACM on
Programming Languages*, volume 10, PLDI, pages 2012–2035 (2026).

RACE uses a two-level identification scheme for array-reference reuse and
expression redundancy, detects shared computations hierarchically, and
optimizes the auxiliary arrays used to store their results. Expression
reassociation exposes additional opportunities for redundancy elimination.

- [Published paper (DOI: 10.1145/3808327)](https://doi.org/10.1145/3808327)
- [PLDI 2026 paper page](https://pldi26.sigplan.org/details/pldi-2026-papers/84/Redundant-Array-Computation-Elimination)
- [Open-access full text on arXiv](https://arxiv.org/abs/2506.21960)

## Build

Requirements: CMake 3.16+, a C++17 compiler, matching LLVM/Clang development
packages, and Boost headers. Tested with LLVM/Clang 19 on Linux.
Python 3 and `clang++` beside the discovered `clang` are required for tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(llvm-config --prefix)" \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Alternatively, set `LLVM_DIR` and `Clang_DIR` to the LLVM/Clang CMake package
directories. Add `-DRACE_DEBUG=ON` to enable internal diagnostic output;
`CMAKE_BUILD_TYPE=Debug` alone does not enable it.

## Use

```sh
./build/race -h
./build/race example/poisson_gold.cpp -- -std=gnu++17
./build/race --tsr --cmi example/derivative_gold.cpp -- -std=gnu++17
./build/race -Iinclude -Iother/include source.cpp
./build/race -p /path/to/project/build source.cpp
```

Pass compiler flags after `--`, or use `--extra-arg`. Use `-p <dir>` to load
`compile_commands.json` from the input project's build directory, including its
include paths, macros, and language settings. To generate a compilation database:

```sh
cmake -S /path/to/project -B /path/to/project/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
./build/race -p /path/to/project/build /path/to/project/source.cpp
```

Without `-p` or `--`, RACE uses a fixed compilation command with no additional
flags. To override Clang's builtin header directory, pass
`--extra-arg=-resource-dir=/path/to/clang/resources`.

| Option | Effect |
| --- | --- |
| `-I<dir>` or `-I <dir>` | Add an include directory; repeatable, in the specified order. |
| `--tsr` | Substitute temporary scalar expressions within an iteration while retaining assignments. |
| `--cmi` | Allow contraction of the innermost dimension. By default it is retained to facilitate vectorization. |
| `--no-contraction` | Retain auxiliary array dimensions. This may consume substantial stack space. |
| `--no-commute` | Disable reassociation and operand reordering. |
| `--mlf=N` | Limit leaf array references in an auxiliary expression; `N >= 2`. |
| `-p <dir>` | Use a compilation database in that directory. |

RACE prints the output path after each successful transformation and reports
Clang diagnostics for invalid input. A failed transformation leaves any existing
output file intact.

The examples in `example/` are standalone. For LIKWID instrumentation, define
`LIKWID` and provide its include and library flags.

## Annotate a region

```cpp
void stencil(const double *a, double *b, int n) {
    int i;
    #pragma race var
    #pragma race begin
    for (i = 1; i < n - 1; ++i) {
        b[i] = (a[i - 1] + a[i]) + (a[i] + a[i + 1]);
    }
    #pragma race end
}
```

Use `#pragma race begin` and `#pragma race end` in pairs; nested regions are not
supported. Put `#pragma race var` before the region in the loop's enclosing
scope, after declarations of any values used for auxiliary array dimensions.
It marks the location for auxiliary declarations. Multiple regions may share
a declaration marker. Pragmas in included headers are ignored; only the main
file is rewritten. Macro-expanded loop ranges that cannot be mapped to source
characters are rejected.

## Supported input and assumptions

RACE supports double-precision stencil-style kernels with:

- A perfect rectangular nest of nonempty `for` loops, with distinct signed
  integer induction variables, `<` or `<=` bounds, and `++` on the same variable.
  Bounds must be independent of induction variables and free of side effects.
- Innermost bodies containing plain assignments to `double` scalars or arrays.
  Arithmetic expressions use `+`, `-`, `*`, `/`, unary minus, and builtin
  double-precision `sin`/`cos` calls. Intermediate arithmetic must use double
  precision; arbitrary narrowing conversions and explicit casts in the region
  are unsupported.
- Array accesses with affine unit-coefficient stencil offsets and sufficient
  valid halo elements. Unsupported subscripts are rejected.

Volatile accesses, reference aliases, and unsupported statements or conversions
are rejected.

The user must ensure that input arrays do not alias written arrays or scalars,
input storage and shape parameters remain stable, and all original and expanded
precomputation accesses are valid. Alias analysis and full dependence analysis
are not implemented. Keep iteration domains nonempty and extents positive.
Generated auxiliary arrays use automatic storage and may use GNU variable
length array extensions; compile such output with Clang/GCC in GNU mode and
provide sufficient stack space. `--no-contraction` can substantially increase
memory requirements.

Default reassociation and commutation can change floating-point rounding.
`--no-commute` preserves expression ordering but is not a promise of bitwise
identity on every compiler/platform. These optimizations assume ordinary pure
math evaluation without observation of floating-point exceptions or `errno`.
Validate numerical tolerances for the intended workload before using output.

## Tests

Run the regression suite with:

```sh
ctest --test-dir build --output-on-failure
```

It compares original and transformed kernels from `example/` across common
options and checks source transformation and error handling. Numerical checks
use relative and absolute tolerances of `2e-11`.

For additional input sizes and distributions, run:

```sh
python3 tests/regression.py build/race "$(command -v clang)" --extended
```

Add `--debug-build` when testing a binary built with `RACE_DEBUG=ON`.

## Citation

If you use RACE in your research, please cite the published paper:

```bibtex
@article{wang2026race,
  author    = {Wang, Zixuan and Yuan, Liang and Jiang, Xianmeng and
               Li, Kun and Xiao, Junmin and Zhang, Yunquan},
  title     = {Redundant Array Computation Elimination},
  journal   = {Proceedings of the ACM on Programming Languages},
  volume    = {10},
  number    = {PLDI},
  year      = {2026},
  month     = jun,
  pages     = {2012--2035},
  publisher = {Association for Computing Machinery},
  doi       = {10.1145/3808327},
  url       = {https://doi.org/10.1145/3808327}
}
```
