#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
#include <numeric>
namespace npc {
    namespace {
        Receipt public_receipt(Receipt r) {
            r.debug_reason.clear();
            r.fingerprint.clear();
            return r;
        }
        void validate_argument_types(const Json & args) {
            const std::set < std::string > strings = {
                "item", "place", "edge", "shop", "job", "recipe", "fixture", "bed", "contract", "to", "channel", "proposal", "answer",
                "mode", "obligation", "target", "operator", "question", "message", "record", "resource", "grantee", "episode", "skill",
                "tool", "container", "scope"
            };
            const std::set < std::string > integers = {
                "minutes", "expires", "version", "amount"
            };
            const std::set < std::string > booleans = {
                "open", "grant", "fabricated", "public", "delete", "forceful"
            };
            const std::set < std::string > lists = {
                "materials", "tools", "participants", "evidence", "acl"
            };
            for (const auto & kv : args.as_object()) {
                const auto k = std::string(kv.key());
                if (strings.contains(k)) static_cast < void >(decode < std::string >(kv.value()));
                if (integers.contains(k)) static_cast < void >(decode < std::int64_t >(kv.value()));
                if (booleans.contains(k)) static_cast < void >(decode < bool >(kv.value()));
                if (lists.contains(k)) static_cast < void >(decode < std::vector < std::string >>(kv.value()));
                if (k == "terms" && ! kv.value().is_object()) throw InputError("terms must be an object");
                if (k == "statement") static_cast < void >(decode < Statement >(kv.value()));
            }
        }
        void finite_json(const Json & j) {
            if (j.is_double() && ! std::isfinite(j.as_double())) throw InputError("non-finite command value");
            if (j.is_array()) for (auto & v : j.as_array()) finite_json(v);
            if (j.is_object()) for (auto & v : j.as_object()) finite_json(v.value());
        }
    }
    World::World(Config c, State s) : World(std::move(c), std::move(s), false) {} World::World(Config c, State s, bool restoring) : config_(std::move(c)),
    state_(std::move(s)) {
        if (state_.config_hash.empty()) state_.config_hash = config_.hash;
        validate_state(config_, state_, restoring);
        if (! restoring) {
            for (auto &[id, a] : state_.actors) {
                for (auto &[n, v] : a.needs) {
                    if (v.value <= v.activation) v.active = true;
                    if (v.value >= v.target) v.active = false;
                    v.critical_active = v.value <= v.critical;
                }
                if (a.health <= 0) {
                    a.alive = false;
                    a.capable = false;
                } else if (a.health <= config_.number({
                    "physical", "incapacitated_at"
                })) a.capable = false;
                if (a.fatigue >= config_.number({
                    "thresholds", "fatigue_heavy_block", "on"
                })) a.heavy_allowed = false;
            }
        }
        rebuild_local_evidence_index();
    }
    World World::from_scenario(Config c, const Json & j) {
        auto s = scenario_state(c, j);
        return World(std::move(c), std::move(s));
    }
    bool World::can_decide(const Id & id) const {
        if (phase_ != "ready" || ! state_.actors.contains(id)) throw InputError("invalid scheduler query");
        const auto & a = state_.actors.at(id);
        return a.alive && a.capable && a.active_action.empty();
    }
    Receipt World::submit(const Command & command) {
        if (phase_ != "ready") throw InputError("submit only at a ready boundary");
