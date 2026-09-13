# SELF-0.1: implementation and causal regression plan

**Goal:** implement the supplied SELF-MODEL-0.1 as an executable, auditable loop, and repair repeated decisions / unmet needs / employment follow-through.
**Approved basis:** user-provided SELF-MODEL-0.1 and explicit request to complete it. Source base is the actual supplied C++ 0.11 archive, not the nonexistent SELF12 package.
**Architecture:** executor emits only perceived result envelopes; two normal cognitive operations interpret/attribute them. Mind owns 8x5 beliefs; planner receives bounded predictions. Recovery is a separately perceived delayed result processed at the same mutation point.
**Tech stack:** C++20, existing CMake/CTest, Python standard-library scenario analysis. No new external runtime dependencies.

## Constraints
- Executor and forecasts never write SelfModel. Hidden results never become personal evidence.
- Context/operations use the existing budgets; no second free thinking loop.
- Replays, including replays after the hot provenance cache rotates, cannot reinforce a source.
- Coping requires observed recovery and functioning; elapsed time alone is insufficient.
- All adaptation choices where the source says “for example” or “function(...)” are documented, not passed off as specified constants.
- No courage/decisiveness/confidence actor trait; observed descriptors never feed decisions.
- All deliverables must exist and build; attach raw commands, exit codes, hashes, and coverage.

## Tasks (each includes failing regression, implementation, complete verification)
- [x] S1 `include/life/self_model.hpp`, `src/life/self_model.cpp`, `tests/test_self_model.cpp`: 8 domains, 5 axes, attribution, source identity, neutral prior, weak generalization, finite validation. Run `life_tests self_model`.
- [x] S2 `include/life/experience.hpp`, `src/life/experience.cpp`, executor/social publishers: bounded inbox, masked perception, saved predecision uncertainty. Test executor with autonomy disabled, hidden-world invariance, overflow and replay.
- [x] S3 `src/life/cognitive_self_world.cpp`, cognition headers/dispatcher: InterpretOutcome then AttributeOutcome charged as existing operations. Test gate, preemption, save/restore at both stages, sources and trace.
- [x] S4 same runtime: bounded recovery traces; at delayed observed recovery create an envelope, integrate only through AttributeOutcome. Test delayed coping, different distress/function, no fake uncertainty exposure, replay.
- [x] S5 `mind.cpp`, `PersonalView`, appraisal: forecast components and private predictions only; link appraisal to Affect and Body. Test same opportunities after self/luck-attributed history, per-domain specificity, pure forecasts and trace-off equality.
- [x] S6 generation, persistence, validation: history replay through same interpretation/integration functions; explicit new save version; workers 1/2/4, active recovery restoration and all invalid inputs.
- [x] D1 `mind.cpp`, `cognitive_world.cpp`, project scheduling: forbid addressless contact actions; release fruitless focus without deleting goals; compare actual continuation before interrupting work; log exact retry/deferral reason.
- [x] D2 executor and planner: known Buy->Eat continuation valued without awarding food at purchase; feasible rest/water; per-need critical durations and longest episode; low-money scenario with sufficient vacancies.
- [x] D3 `career.hpp/cpp`, career runtime: source-backed leads, information inquiry and actual interview/hiring, preserve partial knowledge, no automatic retries with unchanged evidence, join only after acceptance; report attempts/accepted/completed and causal fact roots.
- [x] V: full CTest, sanitizers, weekly matched-seed matrix (42,7,101); final archive re-extracted and rebuilt. Report measurements separately from designed tests.

## Verification evidence
All 387 named C++ cases passed; Release CTest 24/24; ASan/UBSan 21/21 (world, community_long, life11_long explicitly excluded); six final week worlds returned 0. Re-extracted source configured, built and passed 24/24 CTest. Compile-input SHA-256 matches the final tested source. Raw command records are included in the delivery evidence.
