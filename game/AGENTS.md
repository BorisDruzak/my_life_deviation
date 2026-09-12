# Game / Unreal Integration Instructions

These instructions apply to everything under `game/`.

They supplement the repository-level `AGENTS.md`.

The purpose of this layer is to integrate the simulation with the playable game.

---

## 1. Responsibility of game/

`game/` may contain game-engine and presentation integration such as:

* Unreal Engine integration;
* actors/components;
* visual representation;
* animation integration;
* player interaction;
* input;
* cameras;
* UI/debug UI;
* visualization of simulation state;
* adapters between UE and simulation;
* game-specific presentation logic.

This layer must not become the authoritative implementation of simulation-domain mechanics.

---

## 2. Simulation authority

The core simulation under `engine/` is authoritative for simulation state and rules.

Do not duplicate simulation logic inside the game layer.

Examples of logic that should normally remain in the simulation engine:

* needs;
* memory;
* beliefs;
* relationship calculations;
* learning;
* NPC action selection;
* social evaluation;
* reputation;
* objective world rules;
* economic exchange rules;
* action consequences.

The game layer may display or adapt these values but should not independently calculate competing versions.

---

## 3. No convenience migration

Do not move simulation logic from `engine/` into `game/` merely because Unreal makes a particular implementation convenient.

If Unreal needs data or commands that the engine does not expose:

1. define the required integration interface;
2. keep domain logic in the simulation layer;
3. create an adapter/interface in the game layer.

---

## 4. Dependency direction

Preferred dependency direction:

Game / UE
→ simulation-facing adapter
→ engine.

Avoid dependencies where core engine code requires Unreal types or headers.

The simulation engine should not depend on:

* `UObject`;
* `AActor`;
* `UActorComponent`;
* Blueprint types;
* Unreal containers solely for convenience;
* rendering components;
* game widgets.

Translate data at the integration boundary when necessary.

---

## 5. NPC representation

An Unreal NPC actor is a representation of a simulated NPC.

Do not assume the actor itself should own all simulation state.

Prefer a stable simulation identity/handle linking presentation objects to simulation entities.

The simulation should be able to continue conceptually without requiring every simulated NPC to have a fully active rendered actor.

This is important for scaling to large populations.

---

## 6. Presentation versus simulation frequency

Rendering frequency and simulation frequency are different concepts.

Do not require all NPC simulation logic to run every rendered frame.

Avoid tying simulation semantics directly to:

* frame rate;
* animation tick;
* camera visibility;
* actor rendering frequency.

Use explicit simulation timing.

Game presentation may interpolate or visualize lower-frequency simulation updates.

---

## 7. Unreal Tick

Avoid placing complex simulation-domain logic into Unreal `Tick()` functions.

Before adding per-frame work, determine whether it belongs to:

* rendering/presentation;
* interpolation;
* player input;
* visualization;

or to the simulation engine.

Per-frame presentation code should not silently become an alternative decision system.

---

## 8. Player commands

Player actions that affect the simulated world should normally be translated into explicit simulation commands/actions.

Preferred pattern:

player input
→ game interaction
→ simulation command/action
→ simulation result/event
→ presentation update.

Avoid directly mutating deep engine state from arbitrary UI/actor code.

---

## 9. Events and state synchronization

Prefer explicit synchronization between simulation and presentation.

Examples:

* simulation events;
* state snapshots;
* dirty-state notifications;
* explicit query interfaces.

Avoid uncontrolled polling of every simulation value every frame when more efficient synchronization is possible.

---

## 10. Visual state

Presentation-only state belongs in `game/`.

Examples:

* current animation montage;
* LOD;
* facial animation;
* selected mesh;
* visual effects;
* widget state;
* camera-facing labels.

These should not become simulation facts unless explicitly modeled by the simulation.

---

## 11. Animation

Animation should represent simulation state; it should not normally determine simulation-domain truth.

For example:

simulation:
NPC performs "Eat"

game:
selects eating animation and visual sequence.

Do not derive important biological results solely from animation completion unless the architecture explicitly defines such synchronization.

---

## 12. Unreal-specific optimizations

The game layer may use UE-specific optimizations such as:

* actor pooling;
* Mass/entity representation;
* LOD;
* animation budget systems;
* significance management;
* distance-based visualization;
* reduced representation of remote NPCs.

Such optimizations must not silently remove simulation state.

A visually inactive NPC may still exist in the simulation.

---

## 13. Large-population design

Assume that many simulated NPCs may exist while only a subset are rendered in detail.

Do not architect the project under the assumption:

> one full Unreal Actor with full per-frame logic equals one simulated NPC.

Keep representation separable from simulation identity.

---

## 14. Debug visualization

The game layer may expose tools for understanding simulation behavior.

Useful debug information may include:

* current needs;
* active goal;
* selected action;
* known facts;
* beliefs;
* recent memory;
* relationship state;
* current location;
* reason for decision;
* relevant utility scores.

Debug UI should read or query simulation state.

It must not modify simulation semantics merely for display convenience.

---

## 15. Testing

Where possible, keep game-layer tests focused on integration:

* mapping between simulation entity and game representation;
* command forwarding;
* event handling;
* save/load bridge;
* visualization-state updates;
* lifecycle of actors/components;
* failure handling when an entity is not rendered.

Core simulation mechanics should be tested primarily under `engine/`.

---

## 16. Interface changes

If the game layer requires a change to an engine public interface:

1. inspect the engine architecture and specification;
2. determine whether the requirement is presentation-only or genuinely simulation-domain;
3. keep the engine interface generic where practical;
4. avoid Unreal-specific types in engine APIs;
5. coordinate shared-interface changes through the root agent.

Do not let separate subagents independently redesign engine/game boundaries.

---

## 17. Save/load

Presentation-only state and simulation-domain state should remain distinguishable.

The authoritative persistent state of an NPC should not depend on the existence of its currently spawned Unreal Actor.

Where possible:

simulation state
→ persistent domain state

presentation actor
→ reconstructable representation.

---

## 18. Completion for game-layer changes

Before declaring integration work complete:

1. verify the game builds;
2. verify affected integration tests;
3. verify engine tests still pass when engine interfaces changed;
4. verify no unintended simulation logic was duplicated into `game/`;
5. inspect per-frame work for scalability;
6. inspect the final diff for UE dependencies leaking into `engine/`.
