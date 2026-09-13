# C++ 0.8 / COG-0.4 integration plan

Base: delivered C++ 0.7.0; normative source: Документация/Механики/Когнитивный_контур_v0.4.md.
Goal: decisions must be produced by timed, budgeted, subjective operations, and their real traces must be inspectable.

## Steps and files
- [x] Rebuild supplied baseline; all 6 CTest registrations pass.
- [x] Red: `python probe_cli.py /mnt/data/build07/life_sim` rejects --thoughts.
- [x] `include/life/cognition.hpp`, `src/life/cognition.cpp`: serializable candidate queue, focus, personal readings, context, typed thoughts and operation progress. Tests: no early publication, no sleep credit, stale operands, signed forecasts, bounded context.
- [x] `src/life/mind.cpp`: split bounded method enumeration and a single method forecast; Planner remains as reference; negative expected outcomes are never clipped to zero.
- [x] `src/life/world.cpp`: integrate continuous physical intervals to each affected actor's event, a maximum 1s physical step, subsecond cognitive completions, phase ordered effects, stable batch commit. Thoughts use PersonalView only.
- [x] `src/life/executor.cpp`: message outcome notifications, explicit observations before cognitive commit, leave thought and physical-action lifecycles separate.
- [x] `app/main.cpp`: --thoughts JSONL, --workers, --work-chunk; deterministic diagnostics do not consume IDs or change state.
- [x] `src/life/persistence.cpp` / `validation.cpp`: new format, entire cognitive progress and provenance; reject invalid saves. Test save during a thought and continue identically.
- [x] `tests/test_cognition.cpp`: integrated semantic/temporal/counterfactual tests; `tools/analyze_thoughts.py`: actual trace audit.
- [x] Run reference/indexed, thread/chunk, trace/no-trace and resumed comparisons; normal/deficit simulations and detailed thought inspection.
- [x] Release+sanitizer verification, re-extract delivery ZIP, clean build and verification, standalone result report.

## Scope
One focus, serial operations per actor, explicit clocks and provenance. No LLM. No full free-form SEM language or fake narrative logs. The existing world catalogue/physiology/history is reused, not replaced by a tiny unrelated demo. Unknown identification probabilities and universal multilateral contracts remain separately marked, rather than silently invented.

Verification note: Release fully exercised; ASan/UBSan completed for8/9 CTest registrations excluding the long world group. Production profiling and complete semantic/multimonth integration are explicitly not covered.
