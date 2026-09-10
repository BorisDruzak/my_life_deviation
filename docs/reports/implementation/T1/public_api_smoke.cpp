// Reproducible API-only smoke test. No internal policy headers or debug_state() access.
#include "npc/world.hpp"
#include "npc/policy/knowledge_query.hpp"
#include "npc/math.hpp"
#include <iostream>

int main() {
    try {
        auto config = npc::Config::load("data");
        auto scenario = npc::read_json("examples/food.json");
        auto world = npc::World::from_scenario(config, npc::at(scenario, "world"));
        const auto policy_hash = npc::math::sha256(npc::canonical(npc::Object{
            {"version", "POLICY-0.1"}, {"purpose", "T1-public-api-smoke"}}));
        npc::policy::ControllerMemory memory("a", config.hash, policy_hash);
        auto local = npc::policy::apply_delta(memory, world.view_delta("a", memory.cursor(), 8));
        // Exhausting the backlog here is deliberate test setup, not a per-decision production loop.
        while (!local.history_complete())
            local = npc::policy::apply_delta(memory, world.view_delta("a", memory.cursor(), 8));
        const auto belief = npc::policy::query_belief(local,
            {"shop_home", "edge", npc::Object{{"from", "shop"}, {"to", "home"}, {"minutes", 5}},
             local.own().tick, true});
        if (belief.truth != npc::policy::Truth::KnownTrue) throw npc::InputError("known edge lost");
        const auto saved = memory.snapshot();
        auto restored = npc::policy::ControllerMemory::restore(saved, config.hash, policy_hash);
        auto resumed_world = npc::World::restore(config, world.snapshot());
        if (npc::canonical(saved) != npc::canonical(restored.snapshot()))
            throw npc::InputError("policy roundtrip differs");
        world.run(2);
        resumed_world.run(2);
        const auto next = npc::policy::apply_delta(memory, world.view_delta("a", memory.cursor(), 8));
        const auto resumed = npc::policy::apply_delta(restored,
            resumed_world.view_delta("a", restored.cursor(), 8));
        if (npc::canonical(next.export_json()) != npc::canonical(resumed.export_json()) ||
            npc::canonical(memory.snapshot()) != npc::canonical(restored.snapshot()) ||
            npc::canonical(world.snapshot()) != npc::canonical(resumed_world.snapshot()))
            throw npc::InputError("continued local/world state differs");
        std::cout << npc::canonical(npc::Object{{"status", "passed"},
            {"public_headers_only", true}, {"page_budget", 8},
            {"initial_known_edge", npc::policy::describe(belief)},
            {"continued_at", next.own().tick}, {"world_hash", npc::at(world.snapshot(), "state_hash")},
            {"policy_hash", npc::at(memory.snapshot(), "state_hash")},
            {"world_and_policy_roundtrip_equal", true}}) << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
