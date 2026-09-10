#include "npc/world.hpp"
#include "npc/math.hpp"
namespace npc {
    Json World::snapshot() const {
        if (phase_ != "ready") throw InvariantError("snapshot only at a completed boundary");
        auto payload = encode(state_);
        return Object {
            {
                "format", "npc-world-snapshot"
            }, {
                "version", rules_version
            }, {
                "config_hash", config_.hash
            }, {
                "state_hash", math::sha256(canonical(payload))
            }, {
                "state", std::move(payload)
            }
        };
    }
    World World::restore(Config c, const Json & snapshot) {
        if (str(snapshot, "format") != "npc-world-snapshot" || str(snapshot, "version") != rules_version || str(snapshot,
        "config_hash") != c.hash) throw InputError("incompatible snapshot version/configuration; explicit migration required");
        const auto & payload = at(snapshot, "state");
        if (math::sha256(canonical(payload)) != str(snapshot, "state_hash")) throw InputError("snapshot checksum mismatch");
        auto s = decode < State >(payload);
        // Snapshots must be complete. Defaults are allowed in scenarios, not in restores.
        if (canonical(encode(s)) != canonical(payload)) throw InputError("incomplete or non-canonical snapshot state");
        return World(std::move(c), std::move(s), true);
    }
}
