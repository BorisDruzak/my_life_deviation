# My Life Deviation — RECOVERY 0.13

Headless C++20 NPC simulation without LLM. The active RECOVERY 0.13 engine
extends SELF 0.12 with subjective resource facts, owned clothing and funded
exchange, protected budgets and monetary help, learned procedures, and delayed
private text communication. The complete former active release remains under
`legacy/cpp_0.12/`.

Primary references:

- [RECOVERY rules](docs/rules/recovery_0.13/SPEC_RU.md)
- [implementation plan](docs/architecture/recovery_0.13/PLAN_RU.md)
- [delivered verification report](docs/reports/recovery_0.13/REPORT_RU.md)
- [SELF-MODEL rules](docs/rules/self_model_0.1/SELF-MODEL-0.1.md)
- [repository navigation](navigation/INDEX.md)

Archive-provided test results are historical evidence, retained with exact
source and evidence hashes in `provenance/incoming/recovery_0.13/`. They do
not replace a local build.

## Build and tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --parallel 2
./build/life_tests
./build/self_behavior_scenario
```

### Windows (MSVC x64)

The repository includes the `windows-msvc-release` CMake preset. After
installing CMake 3.20+ and Visual Studio 2022 Build Tools with the C++
workload, run these commands from PowerShell; no Developer Prompt is needed:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --parallel 2
ctest --preset windows-msvc-release --parallel 2
& .\out\build\windows-msvc-release\Release\life_sim.exe --recovery --population 8 --seed 42 --days 1 --summary summary.json
```

Build products are placed below `out/`, which Git ignores. The release preset
uses standard MSVC and Windows SDK components only; the optional sanitizer
configuration remains for GCC/Clang environments.

## RECOVERY scenario

```sh
./build/life_sim --recovery --population 16 --seed 42 --days 7 \
  --summary summary.json --recovery-state recovery.json \
  --self-state self.json --career-state career.json \
  --community-state community.json --actors actors.csv --save week.save
```

`--recovery` includes the dependent community, life-project, SelfModel and
adaptive-employment mechanics. It keeps NPC knowledge distinct from objective
state: a visible garment is not proof of ownership, and another NPC's balance
is unavailable unless learned through an allowed information path.

Save format: `LIFE-SAVE-0.13.0-recovery1-r1`. Older saves are rejected
explicitly; no hidden migration reinterprets psychological history or item
ownership.
