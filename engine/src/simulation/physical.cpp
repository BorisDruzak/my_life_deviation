#include "npc/world.hpp"
#include "npc/dynamics.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        double value(const std::map < std::string, double > & m, const std::string & k, double fallback = 0) {
            auto it = m.find(k);
            return it == m.end() ? fallback : it->second;
        }
        Relation relation(const Actor & a, const Id & b) {
            auto it = a.relations.find(b);
            return it == a.relations.end() ? Relation {} : it->second;
        }
    }
    void World::continuous() {
        // All physiological and interpersonal rates read one common boundary snapshot.
        std::map < Id, Actor > old;
        for (const auto &[id, a] : state_.actors) {
            // Copy only dynamic inputs, not the ever-growing evidence/history buffers.
            Actor b;
            b.id = id;
            b.alive = a.alive;
            b.capable = a.capable;
            b.needs = a.needs;
            b.traits = a.traits;
            b.interests = a.interests;
            b.repetition = a.repetition;
            b.skills = a.skills;
            b.anger = a.anger;
            b.relations = a.relations;
            b.drives = a.drives;
            b.health = a.health;
            b.fatigue = a.fatigue;
            b.pain = a.pain;
            b.impairment = a.impairment;
            b.stress = a.stress;
            b.fear = a.fear;
            b.selfesteem = a.selfesteem;
            b.selfesteem_base = a.selfesteem_base;
            old.emplace(id, std::move(b));
        }
        std::map < Id, MinuteEffects > effects;
        for (const auto &[id, a] : old) effects[id].mode = ! a.capable ? "forced_rest" : "ordinary";
        for (auto &[id, a] : state_.actions) {
            auto & e = effects.at(a.actor);
            const auto & person = old.at(a.actor);
            if (a.type == "A02") {
                e.mode = "walk";
                auto & p = state_.actors.at(a.actor).position;
                p.progress = p.base_progress +(1 - p.base_progress) * static_cast < double >(state_.time + 1 - p.started) / static_cast < double >(p.due - p.started);
            } else if (a.type == "A07") {
                auto & i = state_.items.at(str(a.args, "item"));
                if (i.remaining_units <= 0) throw InvariantError("empty reserved portion");
                const auto & t = config_.item_type(i.type);
                e.needs["N01"] += get < double >(t, "satiety_total") / static_cast < double >(i.total_units);
                auto category = str(t, "category");
                e.exposure[category] = 1;
                e.pleasure_rate[category] = get_or < double >(t, "pleasure_total", 0) / static_cast < double >(i.total_units);
                -- i.remaining_units;
                ++ i.version;
                if (i.remaining_units == 0) i.placement = {
                    "tombstone", a.id
                };
                emit(a.id, "food_consumed", {
                    a.actor
                }, {
                    i.id
                }, Object {
                    {
                        "item", i.id
                    }, {
                        "units", 1
                    }, {
                        "remaining_units", i.remaining_units
                    }
                }, false);
            } else if (a.type == "A09") e.mode = "sleep";
            else if (a.type == "A23") e.mode = "rest";
            else if (a.type == "A10") {
                e.mode = "work";
                auto & ct = state_.contracts.at(str(a.args, "contract"));
                if (ct.accrued_numerator > max_money * 60 - ct.rate_per_hour) throw InvariantError("wage accrual limit reached");
                ct.accrued_numerator += ct.rate_per_hour;
            } else if (a.type == "A08" || a.type == "A21") {
                auto & j = state_.jobs.at(a.job);
                if (j.worked >= j.required) throw InvariantError("job progressed twice");
                ++ j.worked;
                e.mode = a.type == "A21" ? "work" : "ordinary";
                e.practiced_skill = j.recipe == "R01" ? "cooking" : "repair";
            } else if (a.type == "A16") {
                const auto & t = config_.item_type(state_.items.at(str(a.args, "item")).type);
                auto category = str(t, "category");
                e.exposure[category] = 1;
                e.pleasure_rate[category] = get < double >(t, "pleasure_per_30_min") / 30;
                if (category == "sport") e.mode = "sport";
            } else if (a.type == "A30") {
                e.practiced_skill = str(a.args, "skill");
            } else if (a.type == "A31") e.needs["N05"] += config_.number({
                "intimacy", "private_total_gain"
            }) / static_cast < double >(a.due - a.started);
            else if (a.type == "A15" || a.type == "A32") {
                const auto & terms = a.captured;
                const auto & group = a.participants;
                for (auto & who : group) {
                    const auto & x = old.at(who);
                    auto & fx = effects.at(who);
                    auto category = text(terms, "topic", "conversation");
                    double participation = has(terms, "attention") ? get_or < double >(at(terms, "attention"), who, 1) : 1;
                    fx.exposure[category] = participation;
                    fx.pleasure_rate[category] = .2;
                    const auto weight = participation / static_cast < double >(group.size() - 1);
                    double aff = 0, tension = 0;
                    for (auto & other : group) if (other != who) {
                        auto r = relation(x, other);
                        aff += r.affection / static_cast < double >(group.size() - 1);
                        tension += r.tension / static_cast < double >(group.size() - 1);
                        // Compatibility is estimated from disclosed interests, never another NPC's hidden trait vector.
                        double compatibility = .5;
                        for (const auto & b : state_.actors.at(who).beliefs) if (b.statement.subject == other && b.statement.predicate == "interest" && b.confidence >= .75 && text(b.statement.arguments,
                        "topic") == category) {
                            double known = get_or < double >(b.statement.arguments, "value", .5);
                            compatibility = 1 - std::abs(value(x.interests, category, .5) - known);
                        }
                        auto & nr = state_.actors.at(who).relations[other];
                        if (! x.relations.contains(other)) nr = r;
                        const double rep = value(x.repetition, category);
                        nr.affection = math::clip(1 -(1 - r.affection) * std::exp(- config_.number({
                            "relationships", "affection_growth_per_hour"
                        }) * weight * compatibility /(60 *(1 + rep))));
                        nr.tension = math::clip(r.tension - config_.number({
                            "relationships", "tension_relief_per_hour"
                        }) * weight / 60);
                    }
                    fx.needs["N03"] +=(config_.number({
                        "relationships", "social_base_per_hour"
                    }) + config_.number({
                        "relationships", "social_affection_per_hour"
                    }) * aff) *(1 - config_.number({
                        "relationships", "social_tension_factor"
                    }) * tension) * participation /(60 *(1 + x.traits.at("novelty") * value(x.repetition, category)));
                    if (a.type == "A32") fx.needs["N05"] += config_.number({
                        "intimacy", "mutual_total_gain"
                    }) / static_cast < double >(a.due - a.started);
                }
            }
            ++ a.elapsed;
            static_cast < void >(person);
        }
        for (auto &[id, x] : state_.actors) apply_minute(config_, state_.profile, x, old.at(id), effects.at(id));
    }
    void World::physical_limits() {
        std::vector < std::pair < Id, std::string >> stop;
        for (auto &[id, a] : state_.actors) {
            if (a.alive && a.health <= config_.number({
                "physical", "terminal_at"
            })) {
                a.alive = false;
                a.capable = false;
                emit("physical/" + id, "death", {
                    id
                }, {}, Object {
                    {
                        "actor", id
                    }
                });
            }
            if (! a.alive) a.capable = false;
            else if (a.capable && a.health <= config_.number({
                "physical", "incapacitated_at"
            })) {
                a.capable = false;
                emit("physical/" + id, "incapacitated", {
                    id
                });
            } else if (! a.capable && a.health >= config_.number({
                "physical", "capable_again_at"
            })) {
                a.capable = true;
                emit("physical/" + id, "capacity_recovered", {
                    id
                });
            }
            if (a.heavy_allowed && a.fatigue >= config_.number({
                "thresholds", "fatigue_heavy_block", "on"
            })) a.heavy_allowed = false;
            else if (! a.heavy_allowed && a.fatigue <= config_.number({
                "thresholds", "fatigue_heavy_block", "off"
            })) a.heavy_allowed = true;
            if (! a.active_action.empty()) {
                const auto & act = state_.actions.at(a.active_action);
                bool heavy = act.type == "A10" || act.type == "A21" ||(act.type == "A16" && text(config_.item_type(state_.items.at(str(act.args,
                "item")).type), "category") == "sport");
                if (! a.capable ||(! a.heavy_allowed && heavy)) stop.emplace_back(act.id, ! a.alive ? "terminal_state" : ! a.capable ? "incapacitated" : "physical_limit");
            }
        }
        for (auto &[id, reason] : stop) if (state_.actions.contains(id)) interrupt(id, reason, true);
    }
    void World::thresholds() {
        for (auto &[id, a] : state_.actors) {
            for (auto &[name, n] : a.needs) if (n.enabled) {
                bool old = n.active;
                if (n.value <= n.activation) n.active = true;
                else if (n.value >= n.target) n.active = false;
                if (old != n.active) emit("threshold/" + id + "/" + name, "need_threshold", {
                    id
                }, {}, Object {
                    {
                        "need", name
                    }, {
                        "active", n.active
                    }
                }, true);
                bool was = n.critical_active;
                if (n.value <= n.critical) n.critical_active = true;
                else if (n.value >= n.activation) n.critical_active = false;
                if (was != n.critical_active) emit("critical/" + id + "/" + name, "need_critical", {
                    id
                }, {}, Object {
                    {
                        "need", name
                    }, {
                        "active", n.critical_active
                    }
                }, true);
            }
            auto latch =[&](std::string key, double v, std::string param) {
                bool before = a.latches[key];
                if (v >= config_.number({
                    "thresholds", param, "on"
                })) a.latches[key] = true;
                else if (v <= config_.number({
                    "thresholds", param, "off"
                })) a.latches[key] = false;
                if (before != a.latches[key]) emit("signal/" + id + "/" + key, "state_threshold", {
                    id
                }, {}, Object {
                    {
                        "state", key
                    }, {
                        "active", a.latches[key]
                    }
                }, true);
            };
            latch("fear", a.fear, "fear");
            latch("fatigue", a.fatigue, "fatigue_signal");
            for (auto &[other, r] : a.relations) {
                if (r.affection >= config_.number({
                    "thresholds", "affection_label", "on"
                })) r.affection_high = true;
                else if (r.affection <= config_.number({
                    "thresholds", "affection_label", "off"
                })) r.affection_high = false;
                if (r.tension >= config_.number({
                    "thresholds", "tension", "on"
                })) r.tension_high = true;
                else if (r.tension <= config_.number({
                    "thresholds", "tension", "off"
                })) r.tension_high = false;
            }
        }
    }
}
