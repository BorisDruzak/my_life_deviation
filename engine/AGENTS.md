# Simulation Engine Instructions

These instructions apply to everything under `engine/`.

They supplement the repository-level `AGENTS.md`.

If these instructions are more specific than the root instructions for simulation-engine code, follow these instructions.

---

## 1. Purpose of engine/

`engine/` contains the autonomous simulation core.

The simulation engine must remain usable independently from rendering and presentation systems.

The engine is responsible for simulation-domain behavior such as:

* NPC internal state;
* body and biological state;
* cognition;
* memory;
* knowledge and beliefs;
* needs and motivations;
* decision making;
* actions;
* social relationships;
* information exchange;
* world-state interaction;
* simulation events;
* simulation rules.

Do not move presentation or Unreal-specific responsibilities into this layer.

---

## 2. Headless simulation requirement

The simulation core must remain executable and testable without Unreal Engine.

Do not introduce direct dependencies from core simulation logic on:

* Unreal Engine;
* rendering;
* UI;
* animation;
* Blueprints;
* game widgets;
* player camera;
* visual effects.

UE-specific integration belongs outside the core simulation engine.

If an Unreal-facing adapter is required, keep the simulation-facing interface engine-agnostic.

---

## 3. Conceptual architecture

The simulation is conceptually separated into three primary domains:

1. **Body**
2. **Cognition**
3. **World**

These are architectural ownership boundaries, not merely folder names.

### Body

Owns objective physical/biological state and processes.

Examples:

* energy;
* hunger;
* thirst;
* fatigue;
* physical condition;
* physiological drives;
* sensory/body-state signals.

Body should expose information to cognition through explicit state, observations, or signals.

Cognition should not directly rewrite unrelated internal biological implementation details.

### Cognition

Owns subjective internal interpretation and decision-related state.

Examples:

* memory;
* observations;
* knowledge;
* beliefs;
* expectations;
* interests;
* preferences;
* emotions where modeled cognitively;
* plans;
* goals;
* decision evaluation;
* associative learning;
* causal reasoning.

Cognition interprets information.

It must not be treated as an omniscient copy of objective world state.

### World

Owns objective external simulation state.

Examples:

* locations;
* objects;
* resources;
* ownership;
* environmental state;
* externally observable events;
* transactions/actions affecting the world.

NPC cognition should normally learn about the world through observations, events, communication, or explicitly permitted information channels.

Do not grant agents hidden global knowledge for convenience.

---

## 4. Information boundary

Keep a clear distinction between:

* objective fact;
* observable event;
* observation;
* memory;
* belief;
* expectation;
* rumor/reported information;
* inferred conclusion.

Do not collapse these into one universal truth structure unless explicitly specified.

An NPC may hold a false belief.

A remembered event may be incomplete.

Information received from another NPC may differ from the originating fact.

This distinction is essential to the intended simulation.

---

## 5. Ownership and mutation

Every piece of mutable state should have an identifiable owner.

Avoid arbitrary cross-subsystem mutation.

Prefer:

* commands;
* events;
* explicit interfaces;
* controlled state transitions.

Avoid patterns where one system reaches into another unrelated system and modifies internal fields directly.

Example of undesirable coupling:

`DecisionSystem` directly editing internal `MemorySystem` containers.

Prefer:

`DecisionSystem -> action/result -> MemorySystem::Record(...)`

or another explicitly defined interface.

---

## 6. Body/Cognition/World interaction

The preferred conceptual flow is:

Body state
→ perception/internal signals
→ Cognition
→ goals/decision
→ intended action
→ World
→ result/event
→ Body/Cognition observations
→ memory/learning.

Not every implementation must literally follow one function call chain, but ownership should remain consistent with this model.

---

## 7. No omniscient NPC logic

NPC behavior must depend on information available to that NPC.

Do not use global simulation data directly in decision logic unless the mechanic explicitly models that knowledge.

Before using world information in cognition, ask:

> How does this NPC know this?

Valid answers may include:

* direct perception;
* previous observation;
* memory;
* conversation;
* rumor;
* public information;
* explicit institutional access;
* inference.

If there is no information path, the cognition system should not use the data.

---

## 8. Determinism

Simulation behavior that depends on randomness must use explicit controlled random sources.

Avoid hidden global randomness.

Tests must be able to reproduce stochastic scenarios through fixed seeds or equivalent deterministic control.

Given:

* identical initial simulation state;
* identical configuration;
* identical random seed;
* identical input/event sequence;

the simulation should produce reproducible results unless nondeterminism is explicitly required and documented.

---

## 9. Time

Avoid dependence on wall-clock time inside simulation-domain logic.

Simulation behavior should use explicit simulation time, tick duration, timestamps, or other simulation-owned time abstractions.

This is required for:

* testing;
* deterministic replay;
* accelerated simulation;
* save/load;
* offline analysis.

---

## 10. Numeric invariants

For state represented by bounded values, enforce bounds at clear ownership points.

Do not scatter arbitrary clamps across unrelated call sites.

Where relevant, define and test:

* valid range;
* units;
* neutral value;
* saturation behavior;
* decay behavior;
* update frequency.

Do not hide broken formulas by repeatedly clamping invalid outputs.

If a formula frequently produces impossible values, investigate the formula.

---

## 11. Learning systems

Keep associative and causal learning conceptually distinct where required by specification.

Association answers questions such as:

> What tends to happen together or after this?

Causal reasoning answers questions such as:

> What probably caused this result?

Low causal reasoning ability must not automatically imply inability to learn simple associations unless explicitly specified.

Do not merge these mechanisms merely because they can share implementation infrastructure.

---

## 12. Subjective versus objective state

Maintain the distinction between:

* objective physical/world state;
* subjective perception;
* subjective value;
* belief;
* preference;
* expectation.

Example:

An activity can be objectively available while the NPC believes it is unavailable.

An item can objectively have properties the NPC does not know.

An NPC can expect an activity to be pleasant and later discover otherwise.

This mismatch is a desired feature of the simulation.

---

## 13. Decision making

Decision systems should consume explicit inputs rather than directly inspecting arbitrary internal/global state.

Where practical, separate:

* candidate generation;
* feasibility;
* expected consequences;
* subjective utility;
* constraints;
* selection;
* execution;
* result/feedback.

Do not silently hardcode special-case behavior for one NPC unless the specification explicitly requires it.

Diversity should preferably emerge from differences in:

* properties;
* thresholds;
* weights;
* knowledge;
* memories;
* relationships;
* resources;
* circumstances.

---

## 14. Performance constraints

Assume the simulation may ultimately run hundreds or thousands of NPCs.

Treat hot paths accordingly.

Avoid, unless justified:

* full-world scans per NPC per tick;
* unrestricted all-pairs NPC interaction;
* repeated parsing/allocation in hot loops;
* rebuilding unchanged derived data every tick;
* unnecessarily frequent decision recalculation;
* expensive work for distant/inactive NPCs when an equivalent lower-frequency update is valid.

Prefer when semantically safe:

* indexes;
* spatial partitioning;
* social-neighborhood filtering;
* cached derived values with invalidation;
* dirty flags;
* event-driven updates;
* batching;
* fixed-size or reusable buffers;
* tiered update frequency;
* reduced update frequency for systems whose semantics allow it.

An optimization is acceptable only if it preserves the intended simulation meaning.

---

## 15. Complexity review

Before adding logic with approximate complexity of `O(N²)` or worse across NPCs, explicitly evaluate whether that cost is justified.

Typical examples requiring review:

* every NPC evaluating every other NPC;
* global rumor propagation;
* global relationship scans;
* unrestricted item/resource searches.

Prefer bounded candidate sets based on:

* proximity;
* social graph;
* known entities;
* current location;
* relevant memory;
* cached indexes.

---

## 16. Allocation policy

Avoid unnecessary heap allocation in high-frequency simulation paths.

Prefer stable/reusable structures where reasonable.

Do not sacrifice architecture for micro-optimization, but do not introduce obvious per-tick allocation storms.

Measure before performing invasive low-level optimization.

---

## 17. Testing engine behavior

Behavior-changing engine code should normally have deterministic tests.

Tests should cover relevant categories such as:

* normal path;
* boundary values;
* impossible/invalid input;
* repeated updates;
* long-term decay/growth;
* deterministic random behavior;
* interaction between affected systems;
* regression scenarios.

For probabilistic mechanics, fixed-seed tests may verify individual behavior, while larger simulation tests may verify distribution.

---

## 18. Statistical validation

Do not assert that a probabilistic mechanic is correct based on one random run.

Where the specification implies distributions or emergent behavior, use sufficiently large samples and inspect aggregate results.

Useful checks may include:

* mean;
* median;
* variance;
* percentile ranges;
* event frequency;
* action share;
* population share reaching a state;
* time to state transition.

Statistical tests must not use absurdly narrow tolerances for inherently stochastic behavior.

---

## 19. Long-running simulation

For changes affecting feedback loops, learning, relationships, needs, economy, information spread, or repeated decision making, consider longer simulation runs.

Look for:

* runaway variables;
* permanent saturation;
* inability to recover;
* oscillations;
* deadlocks;
* starvation;
* population homogenization;
* unrealistically repetitive actions;
* excessive state growth.

---

## 20. Refactoring policy

Do not perform a large architectural rewrite merely because a cleaner structure is imaginable.

When moving toward the Body/Cognition/World architecture:

1. characterize current behavior;
2. define the target ownership boundary;
3. stabilize interfaces;
4. migrate one responsibility at a time;
5. keep tests passing;
6. remove obsolete paths only after replacement is verified.

Avoid mixing a broad refactor with unrelated new game mechanics.

---

## 21. Public interfaces

Changes to interfaces used by multiple engine subsystems require integration review.

Before modifying such an interface:

* find all call sites;
* identify dependent systems;
* evaluate serialization/save implications where relevant;
* evaluate tests;
* evaluate performance implications.

Parallel subagents should not independently redefine shared public interfaces.

---

## 22. Serialization and save-state compatibility

When simulation state is persisted, schema/state changes must consider compatibility.

Do not silently delete or reinterpret persisted meaning.

If compatibility is intentionally broken, document it clearly.

Derived/cache data should not automatically be persisted unless there is a reason.

---

## 23. Debuggability

Prefer architecture where errors can be attributed to a subsystem.

Important state transitions should be inspectable in tests or diagnostics.

For complex emergent behavior, it should be possible to determine:

* what the NPC knew;
* what it believed;
* what need/goal was active;
* what candidates were considered;
* why an action was selected;
* what result occurred;
* what was learned.

Do not solve debugging needs by giving runtime systems hidden global coupling.

---

## 24. Completion for engine changes

Before declaring an engine task complete:

1. compile/build affected targets;
2. run relevant unit tests;
3. run integration tests where applicable;
4. run deterministic simulation cases when applicable;
5. inspect simulation output for behavioral changes;
6. inspect performance-sensitive paths;
7. compare behavior to the specification;
8. review the final diff for accidental cross-domain coupling.

A passing build alone is insufficient.
