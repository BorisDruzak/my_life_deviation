# My Life Deviation

`my_life_deviation` contains a C++20 laboratory core for NPC behaviour and the
BEHAVIOR-0.3 documentation that defines its current research boundary. The
active implementation is `mld`; WORLD-0.5.1-t1 is preserved separately as a
non-built historical snapshot.

Start with the [repository navigation](navigation/INDEX.md) and the
[C++ 0.6 architecture entry point](docs/architecture/cpp_0.6/README.md).

## Quick start

With CMake and a C++20 compiler installed:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
.\build\Release\mld_sim.exe --npcs 128 --seed 42 --days 7
```

For a single-configuration generator, run `build/mld_sim` (or
`build/mld_sim.exe`). The optional `tools/run_experiments.py` runner reproduces
the archived laboratory scenarios after a successful build.

The executable is a limited laboratory profile, not a claim that every
BEHAVIOR-0.3 proposal, settlement-scale simulation, or Unreal Engine bridge is
implemented. Archived reports document earlier runs; they do not replace fresh
local verification.

Navigation validation needs only Python's standard library:

```powershell
python tools/verify_navigation.py
python -m unittest tools.tests.test_verify_navigation -v
```
