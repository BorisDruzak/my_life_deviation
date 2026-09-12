# Project rules

## Primary repository

The primary repository for this project is [BorisDruzak/my_life_deviation](https://github.com/BorisDruzak/my_life_deviation).



# My Life Deviation — Codex Agent Instructions

## 1. Project purpose

This repository contains the simulation and game systems for **My Life Deviation**.

The project is built around an autonomous NPC/world simulation in which believable behavior should emerge from common rules, individual properties, knowledge, memory, needs, relationships, beliefs, and accumulated experience.

The implementation must follow the project's written specifications.

Do not invent new simulation or gameplay mechanics merely to make implementation easier.

---

## 2. Sources of truth

Before implementing or modifying simulation behavior, inspect the relevant documentation.

Primary sources:

* `docs/architecture/`
* `docs/superpowers/specs/`
* `docs/superpowers/plans/`
* `docs/rules/`
* `docs/verification/`

Secondary/historical material may exist elsewhere in the repository.

Historical documentation must not override a newer explicit specification.

When documents conflict:

1. identify the conflicting documents;
2. determine which one appears newer or explicitly authoritative;
3. report the conflict;
4. do not silently choose a new game-design rule.

When code and specification conflict:

1. inspect Git history when useful;
2. determine whether the code is legacy, incomplete, or the specification may be outdated;
3. report the discrepancy;
4. use the current approved specification as the default source of truth unless there is clear evidence that it has been superseded.

---

## 3. Mandatory task intake

For every non-trivial task, do not begin editing immediately.

First:

1. inspect repository status;
2. identify the current branch and relevant recent changes;
3. read this `AGENTS.md`;
4. locate any more specific nested `AGENTS.md`;
5. read the specifications referenced by the task;
6. inspect related architecture documents;
7. inspect the current implementation;
8. inspect existing tests;
9. search for related systems and call sites;
10. determine the delta between current implementation and requested behavior.

Only after this investigation should implementation planning begin.

Do not assume a requested feature is absent before searching the repository.

Do not recreate functionality that already exists.

---

## 4. Root-agent responsibility

For substantial tasks, the primary agent acts as the **orchestrator, architect, and integrator**.

The root agent remains responsible for:

* understanding the complete task;
* understanding relevant specifications;
* architectural decisions;
* decomposition into work packages;
* defining subsystem boundaries;
* defining public interfaces;
* identifying dependencies between tasks;
* assigning work to subagents;
* resolving contradictions between subagent results;
* integrating changes;
* final verification;
* specification-conformance review;
* final report.

Delegation does not transfer architectural responsibility away from the root agent.

Subagents are workers and reviewers, not independent product designers.

---

## 5. Multi-agent policy

For substantial implementation work, actively consider using subagents.

Use subagents when:

* several independent parts of the repository must be investigated;
* work can be safely divided into non-overlapping code areas;
* independent tests can be developed in parallel;
* an independent review would improve reliability;
* simulation output needs separate analysis;
* performance analysis can proceed independently;
* a large refactor contains several separable migrations.

Do not create subagents for trivial edits.

Prefer small, well-bounded work packages.

As a default, use no more than **3–4 concurrent subagents**.

Use additional waves of subagents rather than creating a large number of concurrent agents with overlapping context.

---

## 6. Recommended subagent roles

The root agent may create agents with the following logical roles.

### Scout

Purpose:

* repository reconnaissance;
* specification-to-code mapping;
* dependency discovery;
* identification of affected files;
* identification of existing tests;
* identification of architectural conflicts.

Scouts normally do not edit code.

Expected output:

* relevant files and symbols;
* current behavior;
* specification gaps;
* dependencies;
* risks;
* recommended implementation boundary.

### Implementer

Purpose:

* implement one clearly bounded part of an approved plan.

An implementer must receive:

* exact objective;
* relevant specification;
* allowed code scope;
* expected interfaces;
* forbidden changes;
* expected tests;
* completion criteria.

Implementers must not redesign unrelated systems.

### Test engineer

Purpose:

* create or improve tests from the specification;
* validate edge cases independently from the implementation;
* create regression tests for identified failures;
* verify deterministic behavior where required.

Tests should validate externally meaningful behavior and invariants, not merely duplicate implementation details.

### Simulation analyst

Purpose:

* execute simulation scenarios;
* inspect aggregate behavior;
* identify unrealistic distributions;
* identify dead states, loops, starvation, runaway values, or unstable systems;
* compare simulation behavior against documented expectations.

Simulation analysts should initially report findings rather than silently changing mechanics.

### Reviewer

Purpose:

* independently compare implementation against specification and architecture.

Review for:

* specification deviations;
* hidden coupling;
* ownership violations;
* incorrect formulas;
* invalid assumptions;
* missing edge cases;
* accidental nondeterminism;
* performance regressions;
* inadequate tests;
* unnecessary complexity.

Rank findings by severity when possible.

---

## 7. Delegation contract

Never delegate with vague instructions such as:

> Implement the social system.

Every implementation task delegated to a subagent must include:

1. objective;
2. specification references;
3. code ownership/scope;
4. files or directories that may be changed;
5. interfaces that must remain stable;
6. forbidden changes;
7. tests required;
8. completion criteria;
9. expected report.

If the task cannot be bounded sufficiently, the root agent should investigate further before delegating it.

---

## 8. Concurrent editing rules

Do not assign overlapping implementation ownership to multiple subagents at the same time.

Avoid situations where two agents concurrently modify:

* the same source file;
* the same public interface;
* the same data structure;
* tightly coupled logic;
* the same CMake/build configuration.

Prefer ownership such as:

* Agent A: subsystem A implementation;
* Agent B: subsystem B implementation;
* Agent C: tests or analysis.

Shared interfaces should normally be defined or modified by the root agent or by one explicitly designated integration agent.

When two work packages depend on the same interface:

1. stabilize the interface first;
2. then delegate dependent implementation.

---

## 9. Specification authority

Agents implement approved mechanics.

Agents do **not** invent missing game-design rules.

When implementation requires answering an unresolved design question, stop that part of the task and report:

* the unresolved question;
* why implementation depends on it;
* affected systems;
* reasonable alternatives;
* consequences of each alternative.

Examples of unresolved design questions include:

* how one psychological property should mathematically affect another;
* how attraction should depend on social status;
* how quickly a rumor should lose credibility;
* how a new biological need should decay;
* what subjective utility an NPC should assign to a new activity.

Technical implementation details may be resolved by the root agent.

New simulation rules require an explicit specification or explicit user decision.

---

## 10. Preserve behavioral meaning

Do not treat a successful compile as proof that a simulation change is correct.

When refactoring simulation code, preserve semantics unless the specification explicitly changes behavior.

For migrations:

* prefer incremental migration over uncontrolled big-bang rewrites;
* preserve tests during migration;
* add characterization/regression tests when existing behavior must be preserved;
* remove legacy code only after replacement behavior is verified.

---

## 11. Testing requirements

Behavior-changing work requires tests.

Depending on scope, verification may include:

* unit tests;
* integration tests;
* deterministic simulation tests;
* regression tests;
* invariant/property tests;
* statistical/distribution checks;
* long-running simulation scenarios;
* performance measurements.

Do not weaken or remove meaningful tests merely to make a change pass.

If an existing test conflicts with an updated specification:

1. identify the conflict;
2. explain why the test is obsolete;
3. update the test together with the implementation.

---

## 12. Simulation verification

For significant changes to NPC behavior, the root agent should consider running simulation scenarios beyond ordinary unit tests.

Inspect relevant metrics such as:

* action distribution;
* unmet needs;
* frequency of social interactions;
* work/rest balance;
* relationship formation;
* relationship decay;
* information propagation;
* belief formation;
* pathological behavioral loops;
* agents stuck without viable actions;
* runaway positive or negative feedback;
* resource starvation;
* unintended homogenization of NPC behavior.

A mechanically passing simulation may still be behaviorally wrong.

---

## 13. Performance policy

The simulation is intended to support many active NPCs.

Performance-sensitive changes must consider:

* algorithmic complexity;
* unnecessary global scans;
* avoidable `O(N²)` interaction patterns;
* per-tick allocation;
* repeated recalculation of unchanged state;
* cache locality;
* spatial/social indexing;
* event-driven or dirty-state updates;
* batching;
* simulation frequency/tiering.

Optimization must not alter simulation semantics unless explicitly approved by specification.

Do not replace a detailed simulation with a lower-quality approximation merely for performance without documenting the semantic difference.

---

## 14. Git discipline

Before editing:

* inspect the current working tree;
* preserve unrelated user changes;
* do not reset or discard unrelated work.

Do not rewrite Git history unless explicitly instructed.

Do not force-push unless explicitly instructed.

Do not push, merge, or modify `main` merely because implementation is complete unless the user explicitly requested that action.

Keep commits logically scoped when commits are part of the requested workflow.

Before reporting completion, inspect the final diff.

---

## 15. Documentation changes

When implementation changes an approved mechanic, verify whether corresponding documentation must also change.

Do not leave specification and code knowingly inconsistent.

When the specification itself must change, keep the change explicit and distinguish:

* design/specification change;
* implementation change;
* test change.

---

## 16. Completion criteria

A substantial task is not complete until the root agent has:

1. reviewed the final diff;
2. verified specification conformance;
3. run relevant tests;
4. checked integration with affected systems;
5. reviewed subagent findings;
6. resolved or documented remaining issues;
7. considered simulation-level effects;
8. considered performance where relevant.

---

## 17. Final report

For substantial work, report:

### Implemented

What was implemented and which specification it satisfies.

### Files changed

Main files/modules changed.

### Architecture

Relevant architectural decisions or preserved boundaries.

### Tests

Tests executed and their results.

### Simulation verification

Scenarios executed and notable outcomes, when applicable.

### Performance

Known or measured performance implications.

### Specification gaps

Questions or ambiguities discovered in the specification.

### Remaining risks

Known limitations, technical debt, or follow-up work.

Do not claim success without evidence from the executed verification.
