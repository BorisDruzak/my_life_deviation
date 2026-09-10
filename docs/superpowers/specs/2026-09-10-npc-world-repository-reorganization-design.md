# Repository reorganization for NPC World 0.5.1-t1

## Purpose

Restore the complete `WORLD-0.5.1-t1` source package to the repository and
replace the current incomplete root layout with a navigable game/simulation
structure. The result must make it clear which material is executable,
normative, game-facing, historical, or planned.

## Source selection

The canonical implementation source is
`my_life_deviation_git_ready_0.5.1-t1.zip`. Its 719 `SHA256SUMS` entries were
verified against the archive contents. It includes the complete C++20 core,
tests, T1 local-policy implementation, specifications, examples, and reports.

`my_life_deviation_REPO_READY.zip` is an earlier pre-T1 source snapshot. Its
710 archive checksum entries also verify, but it lacks the T1 policy modules
and tests. Its C++ files must not replace the canonical T1 implementation.

Only its unique historical and design material is retained:

- `docs/PROJECT_CONTEXT.md` and `docs/NEXT_STEPS.md`, reclassified as
  historical product context and roadmap input;
- original Draw.io sources, their review, the explanatory PNG, and related
  design notes;
- the mathematical specification v0.3 and its verification artifacts.

The v0.4 reference documents are already present in the T1 archive, so the
earlier duplicate copies are not imported. Corrupted repository payloads,
one-time upload workflows, and split `.part*.inc` fragments remain available
only in Git history; they are not part of the new working tree.

## Target layout

```text
engine/
  include/npc/                 # stable C++ public headers
  src/core/                    # State, World, config, JSON, snapshots, invariants
  src/simulation/              # minute dynamics, physical state, space, economy
  src/actions/                 # action start, reservations, commits
  src/social/                  # proposals, messages, relations
  src/knowledge/               # observations, evidence, local knowledge boundary
  src/autonomy/                # controller, LocalView, policy memory and indexes
  tests/                       # native and CLI regression tests

apps/world_sim/                # CLI entry point and public API example

game/
  rules/                       # runtime parameters and content catalog
  scenarios/                   # executable examples and acceptance scenarios
  content/                     # reserved home for game-authored content

docs/
  rules/                       # current WORLD-0.5 laws, actions, parameters, API
  game/                        # personality, concept, current mechanics, diagrams
  architecture/                # implementation boundaries and T0/T1 contracts
  roadmap/                     # autonomy T2-T9 design and acceptance planning
  reports/                     # verification and implementation reports
  history/                     # v0.3/v0.4 source material, explicitly non-normative

navigation/
  INDEX.md                     # human entry point
  code-map.yaml                # module ownership, entry points, dependencies
  doc-map.yaml                 # document status, authority, related code
  GAME_FLOW.md                 # World -> view -> policy -> command -> result path
  MAINTENANCE_RULES.md         # required updates and naming rules

tools/
  verify_navigation.py         # validates map entries and referenced paths
```

`third_party/` stays at repository root because it is a separately versioned
vendored Boost dependency. Build files stay at root, but target the new
locations. Public C++ includes retain the `npc/...` spelling.

## Boundaries and dependencies

The world executor owns truth and physical transitions. Social, knowledge, and
autonomy modules may request work through the public `World` API but must not
read global state as controller input. The game layer supplies parameters,
catalogues, and scenarios; it does not implement physics. Documentation labels
the current WORLD-0.5 rules as normative, T1 as implemented, and later
autonomy stages as planned.

The navigation maps make this relationship explicit. Each code module records
its allowed dependencies and relevant tests. Each document records its status:
`normative`, `implemented`, `planned`, or `historical`.

## Navigation maintenance rules

1. A new engine module requires a `code-map.yaml` entry, public entry point,
   dependencies, and test location.
2. A new or moved document requires a `doc-map.yaml` entry with status and
   related code or rule area.
3. `GAME_FLOW.md` changes whenever a new runtime boundary is inserted.
4. `verify_navigation.py` fails on missing mapped files, invalid statuses,
   duplicate module identifiers, or a top-level functional directory not
   represented in the maps.
5. Historical documents are preserved for traceability but cannot override a
   current normative rule without an explicit versioned migration.

## Migration and verification

The migration uses the verified T1 package as the content baseline, applies
the directory moves in coherent groups, updates CMake and test paths, imports
only the selected unique historical documents, and retains the newer
personality material from the current Git `main`.

Verification includes archive checksum validation, CMake source-path checks,
the navigation validator, `git diff --check`, and the package CMake/CTest
suite when a C++20 toolchain is available. The present workstation lacks
`cmake` and a C++ compiler, so native build evidence requires a configured
toolchain before merging to `main`.
