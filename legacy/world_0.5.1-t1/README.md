# WORLD-0.5.1-t1 legacy snapshot

This directory preserves the previous WORLD-0.5.1-t1 implementation for
historical comparison.  Its public simulation API lives under `engine/include/npc/`
and its former executable was `apps/world_sim/`.

It is deliberately excluded from the active repository CMake build and CI.  Do not
change this snapshot while implementing or maintaining the C++ 0.6 `mld` core;
place current work in the active root layout instead.
