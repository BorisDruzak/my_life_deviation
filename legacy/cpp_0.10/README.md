# C++0.10 COMMUNITY legacy snapshot

This directory preserves the C++0.10 COMMUNITY implementation that was the
active repository engine before the SELF-0.1 / 0.12 migration.

It contains the former root CMake build definition, engine, CLI, COG reference
and tooling. It is historical source material: the root `CMakeLists.txt` does
not include it, and its executable targets are not the current project entry
points. The active engine is `engine/`.

The copy exists to retain the prior implementation and its migration context;
it must not be modified to change current simulation behaviour.
