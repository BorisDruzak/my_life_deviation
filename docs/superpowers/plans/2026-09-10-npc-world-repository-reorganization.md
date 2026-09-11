# NPC World repository reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the verified WORLD-0.5.1-t1 package into a coherent game/simulation repository and add maintained navigation for code and documents.

**Architecture:** Use `my_life_deviation_git_ready_0.5.1-t1.zip` as the sole C++ implementation baseline. Move the restored files into `engine`, `apps`, `game`, and categorized `docs` trees while preserving public `npc/...` header includes. Add JSON navigation maps and a standard-library validator; import only the explicitly selected historical design material from `my_life_deviation_REPO_READY.zip`.

**Tech Stack:** CMake 3.20+, C++20, Python 3 standard library, JSON, vendored Boost.JSON 1.83.

**Spec:** `docs/superpowers/specs/2026-09-10-npc-world-repository-reorganization-design.md`

## Global Constraints

- `my_life_deviation_git_ready_0.5.1-t1.zip` is canonical; do not replace its T1 code with code from `my_life_deviation_REPO_READY.zip`.
- Preserve the public header spelling `#include "npc/..."`.
- Keep `third_party/` at repository root and keep its contents byte-for-byte unchanged.
- Preserve the newer personality document and its draw.io source from the prior Git `main` under `docs/game/personality/`.
- Retain only unique historical material from the earlier archive; mark all historical documents in `navigation/doc-map.json`.
- Keep repository design specifications and plans under `docs/superpowers/`; they are engineering records, not normative game documentation.
- Do not add corrupted `.part*.inc`, truncated base64 import parts, or one-time recovery workflows to the new tree.
- Do not claim a native build passes unless CMake and a C++20 compiler are actually available and the commands complete successfully.

---

### Task 1: Restore the canonical package and preserve provenance

**Files:**
- Create: `provenance/README.md`
- Create: `provenance/archive-sha256.json`
- Create: `.gitattributes`
- Delete: obsolete root README, lone `src/persistence.cpp`, old recovery workflows, and the old Russian top-level documentation directory after their retained content has been copied.

**Interfaces:**
- Consumes: `C:\Users\admin-2\Downloads\my_life_deviation_git_ready_0.5.1-t1.zip`, `C:\Users\admin-2\Downloads\my_life_deviation_REPO_READY.zip`, and the current branch's personality document/draw.io.
- Produces: a UTF-8-safe restored repository root with source provenance available to `navigation/doc-map.json`.

- [ ] **Step 1: Validate the two source archives directly from ZIP streams**

Run a Python standard-library checker that reads each `SHA256SUMS` entry from the ZIP and hashes each named entry without extracting it. Require these exact results:

```text
my_life_deviation_git_ready_0.5.1-t1.zip: 719 checked, 0 mismatches
my_life_deviation_REPO_READY.zip: 710 checked, 0 mismatches
```

- [ ] **Step 2: Extract the canonical package to a temporary directory with UTF-8 ZIP handling**

Use `zipfile.ZipFile` rather than `tar` so names such as `СИмуляция(2).drawio` retain their Unicode spelling. Reject absolute paths and `..` path segments before extracting. Do not extract into the repository until the checksum result from Step 1 is green.

- [ ] **Step 3: Copy only canonical root-level build and dependency files**

Copy `.clang-format`, `.gitignore`, `CMakePresets.json`, `THIRD_PARTY.md`, and the full `third_party/` tree from the canonical archive. Replace the old `CMakeLists.txt` in Task 2, and relocate archive manifests into `provenance/` instead of keeping them as ambiguous root files.

- [ ] **Step 4: Write provenance metadata**

Create `provenance/archive-sha256.json` with this shape:

```json
{
  "canonical": {
    "path": "my_life_deviation_git_ready_0.5.1-t1.zip",
    "sha256": "703BCF45475CD631C0BA6344FFF94BE265DA921904BB11A02B7592E84F28067B",
    "role": "WORLD-0.5.1-t1 implementation baseline"
  },
  "historical": {
    "path": "my_life_deviation_REPO_READY.zip",
    "sha256": "8AFB057B2F0BA69927CD06E13A1771F2F25B554B6D369BC5DDF54316FECDBD41",
    "role": "pre-T1 historical documentation source"
  }
}
```

Document that archive checksums establish content integrity, not a fresh local C++ build result.

- [ ] **Step 5: Commit the source restoration boundary**

```bash
git add .gitattributes .clang-format .gitignore CMakePresets.json THIRD_PARTY.md third_party/ provenance/
git commit -m "build(repository): restore verified source baseline"
```

### Task 2: Split the executable C++ core by responsibility

**Files:**
- Create: `engine/include/npc/**`
- Create: `engine/src/core/{config,json,math,persistence,validation,world}.cpp`
- Create: `engine/src/simulation/{dynamics,economy,physical,spatial}.cpp`
- Create: `engine/src/actions/{action_start,action_commit}.cpp`
- Create: `engine/src/social/social.cpp`
- Create: `engine/src/knowledge/knowledge.cpp`
- Create: `engine/src/autonomy/{controller,local_view}.cpp`
- Create: `engine/src/autonomy/policy/{commitments,knowledge_query,types,validation}.cpp`
- Create: `engine/src/autonomy/policy/{persistent_map,types_internal}.hpp`
- Create: `engine/tests/**`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: canonical `include/npc/`, `src/`, and `tests/` files.
- Produces: library target `npc_world`, unchanged public `npc/...` include paths, and CTest targets `core`, `cli`, and `autonomy_t0`.

- [ ] **Step 1: Move public headers without changing their relative `npc/` paths**

Move every canonical header from `include/npc/` to `engine/include/npc/`, including `local_view.hpp` and `policy/{knowledge_query,types}.hpp`. Do not edit include directives that already use `npc/...`.

- [ ] **Step 2: Move implementation and test files into the defined module directories**

Use the mapping in this task's file list. Move the nine native test sources, two Python CLI tests, and test helpers into `engine/tests/`. No T1 implementation or test file may be omitted.

- [ ] **Step 3: Update the CMake target paths**

Replace the old source list with the new paths, retaining target names and compile flags:

```cmake
add_library(npc_world STATIC
  engine/src/core/json.cpp
  engine/src/core/math.cpp
  engine/src/core/config.cpp
  engine/src/core/world.cpp
  engine/src/core/validation.cpp
  engine/src/simulation/spatial.cpp
  engine/src/core/persistence.cpp
  engine/src/simulation/dynamics.cpp
  engine/src/simulation/physical.cpp
  engine/src/knowledge/knowledge.cpp
  engine/src/social/social.cpp
  engine/src/simulation/economy.cpp
  engine/src/actions/action_start.cpp
  engine/src/actions/action_commit.cpp
  engine/src/autonomy/controller.cpp
  engine/src/autonomy/policy/types.cpp
  engine/src/autonomy/policy/knowledge_query.cpp
  engine/src/autonomy/local_view.cpp
  engine/src/autonomy/policy/validation.cpp
  engine/src/autonomy/policy/commitments.cpp)
target_include_directories(npc_world PUBLIC engine/include)
```

Update all test and CLI paths to `engine/tests/` and `apps/world_sim/` in the same file.

- [ ] **Step 4: Run structural CMake-path verification**

Run a Python check that extracts every `src/`, `apps/`, and `engine/tests/` source named in `CMakeLists.txt`, asserts that it exists, and asserts that no old `src/`, `include/`, or `tests/` source directory remains.

- [ ] **Step 5: Run native build/test commands when the toolchain is available**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

If `cmake` or a C++ compiler is absent, record the skipped command and reason in the final verification report rather than fabricating a passing result.

- [ ] **Step 6: Commit the engine relocation**

```bash
git add CMakeLists.txt engine/ apps/world_sim/
git rm -r include src tests app
git commit -m "refactor(engine): separate world simulation modules"
```

### Task 3: Separate game data, current rules, reports, and history

**Files:**
- Create: `game/rules/{parameters,catalog,acceptance_map}.json`
- Create: `game/scenarios/*.json`
- Create: `game/content/.gitkeep`
- Create: `docs/rules/**`
- Create: `docs/game/{concept,mechanics,personality,analysis}/**`
- Create: `docs/architecture/implementation/**`
- Create: `docs/roadmap/autonomy/**`
- Create: `docs/reports/**`
- Create: `docs/history/{math_v0.3,world_core_v0.4,design-source,context}/**`
- Create: `docs/superpowers/archive/**`
- Modify: `README.md`, `CMakeLists.txt`

**Interfaces:**
- Consumes: canonical archive documents/data, selected older archive documents, and the current Git `main` personality materials.
- Produces: one current source for each rule/document and explicit historical classification for earlier material.

- [ ] **Step 1: Move runtime data into the game layer**

Move `data/parameters.json`, `data/catalog.json`, and `data/acceptance_map.json` to `game/rules/`. Move all five canonical example scenarios to `game/scenarios/`. Update `Config::load` and CLI defaults only where file paths are hard-coded; keep scenario JSON schemas unchanged.

- [ ] **Step 2: Move normative and implemented documents**

Move canonical `docs/spec/` to `docs/rules/`. Move T0/T1 implementation documents and `START_HERE_T0.md`/`START_HERE_T1.md` to `docs/architecture/implementation/`, autonomy T2-T9 design to `docs/roadmap/autonomy/`, reports to `docs/reports/`, and current mechanics/concept material to `docs/game/`. Move the archive's existing plan into `docs/superpowers/archive/` without replacing the current reorganization specification or plan.

- [ ] **Step 3: Preserve the current Git personality material**

Move `Документация/Механики/Модель_личности_и_выбора_действий_v0.1.md` and its draw.io diagram from the original Git `main` to `docs/game/personality/`. State in the destination README that it is a current design concept, not proof of implemented mechanics.

- [ ] **Step 4: Import only selected unique historical material**

From `my_life_deviation_REPO_READY.zip`, import `PROJECT_CONTEXT.md`, `NEXT_STEPS.md`, all v0.3 mathematical files, original Draw.io files, their PNG, and source-review notes. Place them under `docs/history/` and add a front matter line `Status: historical` to each new historical Markdown index file. Do not import its C++ sources, its duplicate v0.4 documents, or old generated runtime logs.

- [ ] **Step 5: Relocate delivery manifests without treating them as rules**

Move `AUTONOMY_REVIEW_MANIFEST.json`, `T0_IMPLEMENTATION_MANIFEST.json`, and `T1_IMPLEMENTATION_MANIFEST.json` to `docs/architecture/implementation/manifests/`. Move `GIT_READY_VERIFICATION.json`, `GIT_UPLOAD_MANIFEST.md`, `REPOSITORY_MANIFEST.json`, `SHA256SUMS`, and `TREE.txt` to `provenance/`. Update their references in entry documents after moving them.

- [ ] **Step 6: Rewrite repository entry points**

Replace the root README with a concise project overview and links to `navigation/INDEX.md`, `docs/rules/`, `docs/game/`, `docs/architecture/`, and `docs/roadmap/`. Update CMake data installation from `data/` to `game/rules/`.

- [ ] **Step 7: Commit game and document reorganization**

```bash
git add README.md game/ docs/
git rm -r data examples reports
git commit -m "docs(game): organize rules content and history"
```

### Task 4: Implement the navigation mechanism and its checks

**Files:**
- Create: `navigation/INDEX.md`
- Create: `navigation/code-map.json`
- Create: `navigation/doc-map.json`
- Create: `navigation/GAME_FLOW.md`
- Create: `navigation/MAINTENANCE_RULES.md`
- Create: `tools/verify_navigation.py`
- Create: `tools/tests/test_verify_navigation.py`

**Interfaces:**
- Consumes: final directory hierarchy and JSON maps.
- Produces: `python tools/verify_navigation.py --root .`, exit code 0 when every mapped file and dependency boundary is valid.

- [ ] **Step 1: Write the failing validator tests**

Create `tools/tests/test_verify_navigation.py` using `unittest` and a temporary fixture root. Cover missing mapped paths, duplicate module IDs, invalid document statuses, and an unrepresented top-level functional directory:

```python
def test_rejects_missing_mapped_path(self):
    result = validate_navigation(self.root)
    self.assertIn("missing path", result.errors[0])
```

- [ ] **Step 2: Run the tests to prove the validator is absent**

```bash
python -m unittest tools.tests.test_verify_navigation -v
```

Expected: import failure for `verify_navigation` before the implementation exists.

- [ ] **Step 3: Implement `tools/verify_navigation.py` with only the standard library**

Expose `validate_navigation(root: pathlib.Path) -> ValidationResult` and a CLI `--root` argument. Parse `code-map.json` and `doc-map.json`; require code-map fields `id`, `path`, `entry_points`, `depends_on`, `tests`, and doc-map fields `path`, `status`, `authority`, `related`. Permit only `normative`, `implemented`, `planned`, and `historical` statuses. Validate every listed path and every declared dependency ID.

- [ ] **Step 4: Run the validator tests and the real repository validator**

```bash
python -m unittest tools.tests.test_verify_navigation -v
python tools/verify_navigation.py --root .
```

Expected: all unit tests pass and the repository reports zero errors.

- [ ] **Step 5: Write the maps and human navigation pages**

Map `core`, `simulation`, `actions`, `social`, `knowledge`, and `autonomy` in `code-map.json`. `GAME_FLOW.md` must include this actual runtime path:

```text
Actor/State -> World::view_delta or World::view -> controller::decide
-> Command -> World::submit -> action_start -> continuous minute physics
-> action_commit -> Event/Evidence/Receipt -> LocalView/ControllerMemory
```

`INDEX.md` must state reading order: rules, game concept, engine code map, implemented T1 boundaries, roadmap, and historical material. `MAINTENANCE_RULES.md` contains the five mandatory update rules from the design specification.

- [ ] **Step 6: Commit the navigation mechanism**

```bash
git add navigation/ tools/verify_navigation.py tools/tests/test_verify_navigation.py
git commit -m "feat(navigation): add repository maps and validation"
```

### Task 5: Record migration evidence and prepare the main update

**Files:**
- Create: `docs/reports/repository-reorganization-verification.md`
- Modify: `README.md`, `navigation/doc-map.json`, `provenance/README.md`

**Interfaces:**
- Consumes: restored layout, CMake configuration, navigation validator, source archive hashes.
- Produces: reviewable evidence for merging `codex/reorganize-npc-world` into `main`.

- [ ] **Step 1: Run the complete structural evidence set**

```bash
git diff --check origin/main...HEAD
python tools/verify_navigation.py --root .
python -m unittest tools.tests.test_verify_navigation -v
```

Run the CMake/CTest commands from Task 2 if and only if the toolchain is present. Record each command, exit code, and whether it passed or was skipped.

- [ ] **Step 2: Review archive and Git coverage**

Confirm that every canonical non-generated C++ source, header, test, scenario, current rule, and implementation report has a destination. Confirm that every imported `REPO_READY` document is marked historical and that no pre-T1 C++ source was introduced.

- [ ] **Step 3: Write the verification report**

Include archive SHA-256 values, counts of restored sources/headers/tests/docs, navigation-validator result, CMake/CTest result or explicit toolchain limitation, and a list of excluded historical artifacts.

- [ ] **Step 4: Commit verification evidence**

```bash
git add docs/reports/repository-reorganization-verification.md README.md navigation/doc-map.json provenance/README.md
git commit -m "docs(repository): record reorganization verification"
```

- [ ] **Step 5: Merge only after final review**

Inspect `git status --short`, `git diff --check origin/main...HEAD`, and the complete branch diff. Push the branch, then fast-forward or merge it into `main` only after all available checks have green evidence. Do not force-push.
