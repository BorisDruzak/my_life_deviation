# My Life Deviation

`my_life_deviation` contains the C++20 COMMUNITY-0.10 research core for NPC
behaviour. The active implementation is `life_core`; it combines BEHAVIOR-0.3
with an explicitly limited COG-0.4 reference and COMMUNITY-0.10 settlement
profile. It is not a claim of scientific validity, complete coverage of every
specification, UE5 support, or a verified Windows/MSVC build.

Start with the [repository navigation](navigation/INDEX.md) and the
[COMMUNITY-0.10 boundary](docs/architecture/community_0.10/README.md).

## Quick start

With CMake and a C++20 compiler installed:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
.\build\Release\life_sim.exe --community --population 16 --seed 42 --days 1
```

For a single-configuration generator, run `build/life_sim` (or
`build/life_sim.exe`). `tools/run_community.py` writes a new output directory;
the supplied results under `docs/reports/community_0.10/` are historical
evidence, not proof of a fresh local run.

The former `mld` C++ 0.6 laboratory core is retained under
`legacy/cpp_0.6/`; WORLD-0.5.1-t1 remains a separate historical snapshot.

Navigation validation needs only Python's standard library:

```powershell
python tools/verify_navigation.py
python -m unittest tools.tests.test_verify_navigation -v
```
