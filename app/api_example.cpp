#include "npc/world.hpp"
#include <iostream>
int main() {
    try {
        auto config = npc::Config::load("data");
        auto scenario = npc::read_json("examples/food.json");
        auto world = npc::World::from_scenario(config, npc::at(scenario, "world"));
        world.run(10);
        world.check_invariants();
        npc::write_json("out/api/state.json", world.snapshot());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
