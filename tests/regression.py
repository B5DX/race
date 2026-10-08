#!/usr/bin/env python3
"""Compile and compare transformed kernels; no third-party test dependencies."""
import ctypes as ct
import math
import json
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile

RACE = str(Path(sys.argv[1]).resolve())
CLANG = str(Path(sys.argv[2]).resolve())
CXX = str(Path(CLANG).with_name('clang++'))
ROOT = Path(__file__).resolve().parents[1]
PTR = ct.POINTER(ct.c_double)
MODES = [[], ['--tsr'], ['--cmi'], ['--no-contraction'], ['--no-commute'],
         ['--mlf=2'], ['--tsr', '--cmi'], ['--no-commute', '--cmi']]
# label, random seed, magnitude, distribution; shared by originals and transforms.
CASES = [('random793', 793, 1.0, 'signed'), ('random42', 42, 1.0, 'signed'),
         ('random2026', 2026, 1.0, 'signed'), ('small', 42, 1e-3, 'signed'),
         ('large', 2026, 10.0, 'signed'), ('positive', 793, 1.0, 'positive'),
         ('zero', 0, 0.0, 'signed'), ('alternating', 793, 1.0, 'alternating')]
EXTENDED = '--extended' in sys.argv[3:]
if not EXTENDED:
    CASES = CASES[:1]
STENCIL_SIZES = [(3, 3, 3), (5, 6, 7), (7, 5, 4)]
DERIVATIVE_SIZES = (5, 7)
GRID_SEEDS = (41,)
if EXTENDED:
    STENCIL_SIZES += [(9, 8, 10), (12, 10, 9)]
    DERIVATIVE_SIZES += (9, 12)
    GRID_SEEDS += (793, 2026)
STATS = {}

def run(args, success=True):
    result = subprocess.run([str(a) for a in args], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=120)
    if success and result.returncode:
        raise AssertionError(' '.join(map(str, args)) + '\n' + result.stdout + result.stderr)
    if not success:
        assert result.returncode != 0, 'Invalid input accepted: ' + str(args)
    return result


def transform(path, mode):
    result = run([RACE, *mode, path, '--', '-std=gnu++17'])
    # A release build emits exactly one output summary, no internal dumps/warnings.
    if '--debug-build' not in sys.argv:
        assert not result.stderr, result.stderr
        assert result.stdout.startswith('Saved ') and result.stdout.count('\n') == 1, result.stdout
    return path.with_name('race_' + path.name)


def library(source, name, wrapper=''):
    code = source.read_text().split('int main(', 1)[0] + wrapper
    cpp = source.parent / (name + '.cpp')
    so = source.parent / (name + '.so')
    cpp.write_text(code)
    run([CXX, '-std=gnu++17', '-O0', '-shared', '-fPIC', cpp, '-o', so])
    return ct.CDLL(str(so))


def check(actual, expected, label):
    assert len(actual) == len(expected), label
    stats = STATS.setdefault(label[0], [0, 0.0, 0.0])
    stats[0] += 1
    for i, (a, b) in enumerate(zip(actual, expected)):
        assert math.isfinite(a) and math.isfinite(b), (label, i, a, b)
        error = abs(a - b)
        stats[1] = max(stats[1], error)
        stats[2] = max(stats[2], error / max(1.0, abs(b)))
        assert math.isclose(a, b, rel_tol=2e-11, abs_tol=2e-11), (label, i, a, b)


def arrays(n, count, case=CASES[0]):
    _, seed, scale, kind = case
    rng = random.Random(seed)
    def value(i):
        if kind == 'alternating':
            return scale * (1.0 if i % 2 else -1.0)
        return scale * rng.uniform(0.1, 1.0) if kind == 'positive' else scale * rng.uniform(-0.8, 0.8)
    return [(ct.c_double * n)(*(value(i) for i in range(n))) for _ in range(count)]


def examples(folder):
    for name, kernel in [('poisson_gold.cpp', 'poisson_step'), ('j3d27pt_gold.cpp', 'j3d_step')]:
        source = folder / name
        wrapper = ('\nextern "C" void kernel(const double *a, int h, int y, int x, double *b) {'
                   + kernel + '(a, h, y, x, b);}\n')
        original = library(source, 'original_' + kernel, wrapper)
        original.kernel.argtypes = [PTR, ct.c_int, ct.c_int, ct.c_int, PTR]
        for m, mode in enumerate(MODES):
            generated = transform(source, mode)
            assert 'iv_' in generated.read_text(), 'Example was not optimized: ' + name
            optimized = library(generated, 'optimized_' + kernel + str(m), wrapper)
            optimized.kernel.argtypes = original.kernel.argtypes
            for h, y, x in STENCIL_SIZES:
                for case in CASES:
                    data, expected, actual = arrays(h*y*x, 3, case)
                    for i in range(h*y*x): actual[i] = expected[i]
                    original.kernel(data, h, y, x, expected)
                    optimized.kernel(data, h, y, x, actual)
                    check(actual, expected, (name, mode, h, y, x, case[0]))
        print(f'{name}: {len(MODES)} modes x {len(STENCIL_SIZES)} sizes x {len(CASES)} input cases passed', flush=True)

    source = folder / 'derivative_gold.cpp'
    original = library(source, 'original_derivative')
    original.derivative_gold.argtypes = [PTR]*12 + [ct.c_double]*2 + [ct.c_int]
    default_outputs = {}
    for m, mode in enumerate(MODES):
        optimized = library(transform(source, mode), 'optimized_derivative' + str(m))
        optimized.derivative_gold.argtypes = original.derivative_gold.argtypes
        for size in DERIVATIVE_SIZES:
            for lib in (original, optimized):
                for name in ('height', 'weight_x', 'weight_y'): ct.c_int.in_dll(lib, name).value = size
            for c, case in enumerate(CASES):
                c1, c2 = [(0.1, 0.2), (-0.7, 0.33), (0.0, 0.9)][c % 3]
                inputs = arrays(size**3, 12, case)
                actual = (ct.c_double * (size**3))(*inputs[0])
                optimized.derivative_gold(actual, *inputs[1:], c1, c2, size)
                original.derivative_gold(*inputs, c1, c2, size)
                check(actual, inputs[0], ('derivative', mode, size, case[0], c1, c2))
                if not mode:
                    default_outputs[size, case[0]] = list(actual)
                elif '--tsr' in mode:
                    check(actual, default_outputs[size, case[0]],
                          ('derivative TSR-vs-default', mode, size, case[0]))
    print(f'derivative_gold.cpp: {len(MODES)} modes x {len(DERIVATIVE_SIZES)} sizes x {len(CASES)} input cases passed', flush=True)

    source = folder / 'grid.cpp'
    code = source.read_text().replace('nblocks_clinic = 100, ny_block = 100, nx_block = 1000',
                                      'nblocks_clinic = 4, ny_block = 6, nx_block = 7')
    code = code.replace('int main()', 'extern "C" void kernel()').replace('return 0;', 'return;')
    code = code.replace('cout << "Time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;', '')
    source.write_text(code)
    original = library(source, 'original_grid')
    for m, mode in enumerate(MODES):
        optimized = library(transform(source, mode), 'optimized_grid' + str(m))
        # libc's random state is shared: reset to the same seed for each kernel.
        libc = ct.CDLL(None)
        names = ('tx', 'ty', 'tz', 'xsw', 'ysw', 'zsw', 'xs', 'ys', 'zs', 'xw', 'yw', 'zw', 'xc', 'yc', 'zc')
        for seed in GRID_SEEDS:
            libc.srand(seed); original.kernel()
            libc.srand(seed); optimized.kernel()
            check([ct.c_double.in_dll(optimized, n).value for n in names],
                  [ct.c_double.in_dll(original, n).value for n in names], ('grid', mode, seed))
    print(f'grid.cpp: {len(MODES)} modes x {len(GRID_SEEDS)} seeds and live-out scalars passed', flush=True)


def edge_cases(folder):
    source = folder / 'scalar.cpp'
    source.write_text('''extern "C" void kernel(double *a, double *out) {
    int i;
    double t = 0.0, u = 0.0, p = 0.5, s = 0.0;
    #pragma race var
    #pragma race begin
    for (i = 1; i < 10; ++i) {
        t = a[i] * 0.12345678901234567;
        u = t;
        p = p + 1.0;
        out[i] = u + t + p;
        t = p * 2.0;
        p = 3.0;
        out[i] = out[i] + t;
        s = s + a[i];
    }
    #pragma race end
    out[10] = t; out[11] = s; out[12] = p;
}\n''')
    original = library(source, 'original_scalar')
    original.kernel.argtypes = [PTR, PTR]
    for m, mode in enumerate(MODES):
        lib = library(transform(source, mode), 'optimized_scalar' + str(m))
        lib.kernel.argtypes = [PTR, PTR]
        for case in CASES:
            data, expected, actual = arrays(15, 3, case)
            for i in range(15): actual[i] = expected[i]
            original.kernel(data, expected); lib.kernel(data, actual)
            check(actual, expected, ('scalar', mode, case[0]))

    source = folder / 'operators.cpp'
    source.write_text('''extern "C" void kernel(double *a, double *out) {
    int i;
    #pragma race var
    #pragma race begin
    for (i = 1; i < 10; ++i) {
        out[i] = (a[i] + a[i+1]) + (a[i-1] + a[i]);
        out[i+15] = (a[i] * a[i+1]) + (a[i-1] * a[i]);
    }
    #pragma race end
}\n''')
    original = library(source, 'original_operators')
    original.kernel.argtypes = [PTR, PTR]
    for m, mode in enumerate(MODES):
        lib = library(transform(source, mode), 'optimized_operators' + str(m))
        lib.kernel.argtypes = [PTR, PTR]
        for case in CASES:
            data, expected, actual = arrays(30, 3, case)
            for i in range(30): actual[i] = expected[i]
            original.kernel(data, expected); lib.kernel(data, actual)
            check(actual, expected, ('operators', mode, case[0]))

    source = folder / 'literals.cpp'
    literals = ['0.1', '1.2300e-4', '0x1.999999999999ap-4', '0.1f',
                "1'234.500", '4.9406564584124654e-324', '-0.0', '0.14285714285714285']
    source.write_text('#define FRACTION 0.14285714285714285\n'
                      '#define JOIN_IMPL(a, b) a ## b\n#define JOIN(a, b) JOIN_IMPL(a, b)\n'
                      'extern "C" void kernel(double *a, double *out) {\n'
                      'int i; double t;\n#pragma race var\n#pragma race begin\n'
                      'for (i = 1; i < 10; ++i) {\n' +
                      ''.join(f't = a[i] * {"FRACTION" if n == 7 else literal}; '
                              f'out[i+{10*n}] = t + a[i+1] * {literal};\n'
                              for n, literal in enumerate(literals)) +
                      'out[i+80] = a[i] * JOIN(0.12, 5);\n'
                      '}\n#pragma race end\n}\n')
    original = library(source, 'original_literals')
    original.kernel.argtypes = [PTR, PTR]
    for m, mode in enumerate([['--no-commute'], ['--no-commute', '--tsr']]):
        generated = transform(source, mode)
        region = generated.read_text().split('#pragma race begin', 1)[1]
        for literal in literals + ['0.125']:
            assert literal in region, (mode, literal, region)
        lib = library(generated, 'optimized_literals' + str(m))
        lib.kernel.argtypes = [PTR, PTR]
        data = arrays(15, 1)[0]
        expected = (ct.c_double * 90)()
        actual = (ct.c_double * 90)()
        original.kernel(data, expected); lib.kernel(data, actual)
        assert list(actual) == list(expected), ('literal values changed', mode)

    source = folder / 'inline.cpp'
    source.write_text('''extern "C" void kernel(double *a, double *out) {
#pragma race var
#pragma race begin
for (int i = 1; i < 10; i++) { out[i] = a[i] + a[i+1]; } out[10] = 42.0;
#pragma race end
}\n''')
    generated = transform(source, ['--tsr'])
    assert 'out[10] = 42.0;' in generated.read_text()
    run([CXX, '-std=gnu++17', '-fsyntax-only', generated])

    for idx, body in enumerate([
        '#pragma race end\n',
        '{\n#pragma race var\n}\n#pragma race begin\nfor(int i=0;i<3;i++){a[i]=1.0;}\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor(int i=0;i<3;i++){a[2*i]=1.0;}\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor(unsigned i=0;i<3;i++){a[i]=1.0;}\n#pragma race end\n',
        '#pragma race begin\n',
        '#pragma race begin\n#pragma race begin\n#pragma race end\n',
        '#pragma race wrong\n',
        '#pragma race begin extra\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor (int i=0; i<3; i++) { if(i) a[i]=1.0; }\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor (int i=0; i<3; i++) { a[i]+=1.0; }\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor (int i=0; 2<3; i++) { a[i]=1.0; }\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor (int i=0,j=0; i<3; j++) { a[i]=1.0; }\n#pragma race end\n',
        '#pragma race var\n#pragma race begin\nfor (int i=0; i<3; i++) { a[i]=missing; }\n#pragma race end\n',
    ]):
        bad = folder / ('bad' + str(idx) + '.cpp')
        bad.write_text('void f(double *a) {\n' + body + '}\n')
        output = bad.with_name('race_' + bad.name)
        output.write_text('previous output\n')
        run([RACE, bad, '--', '-std=gnu++17'], success=False)
        assert output.read_text() == 'previous output\n', 'Output changed on failure'
    # Explicit databases and common compiler flags continue to work.
    (folder/'compile_commands.json').write_text(json.dumps([
        {'directory': str(folder), 'file': str(source),
         'arguments': [CXX, '-std=gnu++17', '-c', str(source)]}]))
    run([RACE, '-p', folder, source])
    run([RACE, '--extra-arg=-std=gnu++17', source])
    graph = folder/'graph.cpp'
    graph.write_text('#include "Solver/Graph.hpp"\n#include <cassert>\n'
                     'int main() { UDGraph<int> g; g.addNode(1); g.addNode(2); '
                     'g.addNode(3); g.addEdge(1,2); g.removeNode(3); '
                     'assert(g.getNeighbors(1) == vector<int>{2}); '
                     'g.removeNode(2); assert(g.getNeighbors(1).empty()); }\n')
    run([CXX, '-std=c++17', '-I'+str(ROOT/'src'), graph, '-o', folder/'graph'])
    run([folder/'graph'])
    multiple = run([RACE, source, folder/'scalar.cpp', '--', '-std=gnu++17'])
    if '--debug-build' not in sys.argv:
        assert not multiple.stderr, multiple.stderr
        assert multiple.stdout.count('Saved ') == 2 and multiple.stdout.count('\n') == 2, multiple.stdout
    run([RACE, '--mlf=0', source], success=False)
    first, second = folder/'first', folder/'second'
    first.mkdir(); second.mkdir()
    (first/'value.h').write_text('#define VALUE 1.25\n')
    (second/'value.h').write_text('#define VALUE 4.5\n')
    includes = folder/'includes.cpp'
    includes.write_text('#include "value.h"\nvoid f(double *a) { int i;\n'
                        '#pragma race var\n#pragma race begin\n'
                        'for(i=0;i<3;i++){a[i]=VALUE;}\n#pragma race end\n}\n')
    run([RACE, '-I'+str(first), '-I'+str(second), includes])
    assert 'a[i] = 1.25;' in includes.with_name('race_includes.cpp').read_text()
    locked = folder/'locked'
    locked.mkdir()
    shutil.copy(source, locked/source.name)
    locked.chmod(0o500)
    try:
        run([RACE, locked/source.name], success=False)
        assert not (locked/('race_'+source.name)).exists()
    finally:
        locked.chmod(0o700)
    print('scalar dependencies, precision, source ranges and invalid inputs passed', flush=True)


with tempfile.TemporaryDirectory(prefix='race-regression-') as temp:
    folder = Path(temp)
    for file in (ROOT/'example').iterdir():
        if file.is_file() and not file.name.startswith(('race_', '._')): shutil.copy(file, folder)
    help_text = run([RACE, '-h']).stdout
    assert 'Directory containing compile_commands.json' in help_text, help_text
    assert 'Build path' not in help_text, help_text
    edge_cases(folder)
    examples(folder)
for name, (count, absolute, scaled) in STATS.items():
    print(f'{name}: {count} comparisons, max abs={absolute:.3e}, max scaled={scaled:.3e}')
print(f'All {sum(v[0] for v in STATS.values())} numerical comparisons and regression checks passed.')
