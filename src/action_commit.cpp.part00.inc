#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    bool World::precommit(const Action & a) const {
        for (auto & who : a.participants) {
            const auto & x = state_.actors.at(who);
            if (! x.alive || ! x.capable || x.active_action != a.id) return false;
        }
        const auto & actor = state_.actors.at(a.actor);
        const auto & id = a.type;
        auto usable =[&](const Id & item) {
            return accessible(a.actor, item) && permitted(a.actor, item);
        };
        if (id == "A02") return actor.position.kind == "transit";
        if (id == "A06") {
            const auto & sh = state_.shops.at(str(a.args, "shop"));
            auto it = state_.items.find(str(a.args, "item"));
            if (it == state_.items.end()) return false;
            const auto & i = it->second;
            return at_place(a.actor, sh.place) && open_interval(sh.place, state_.time, a.due) && sh.stock.contains(i.id) && i.owner == text(a.captured,
            "owner") && i.placement.kind == "at" && i.placement.ref == sh.place && available(actor.account, a.id) >= a.price && state_.accounts.at(sh.account) <= max_money - a.price && carried_mass(a.actor) + item_mass(i.id) <= actor.capacity_kg + 1e-9;
        }
        if (id == "A07") {
            const auto & i = state_.items.at(str(a.args, "item"));
            return(i.remaining_units == 0 && i.placement.kind == "tombstone") ||(usable(i.id) && holder(i.id) == a.actor);
        }
        if (id == "A04" || id == "A20") {
            auto item = str(a.args, "item");
            return accessible(a.actor, item) &&(id == "A20" ? ! permitted(a.actor, item) : permitted(a.actor, item)) && carried_mass(a.actor) +(holder(item) == a.actor ? 0 : item_mass(item)) <= actor.capacity_kg + 1e-9;
        }
        if (id == "A05") {
            auto item = str(a.args, "item");
            if (! accessible(a.actor, item) || holder(item) != a.actor) return false;
            if (has(a.args, "container")) return usable(str(a.args, "container")) && state_.items.at(str(a.args, "container")).open;
            return true;
        }
        if (id == "A09") return usable(str(a.args, "bed"));
        if (id == "A10") {
            const auto & ct = state_.contracts.at(str(a.args, "contract"));
            return actor.heavy_allowed && usable(ct.workplace);
        }
        if (id == "A08" || id == "A21") {
            const auto & j = state_.jobs.at(a.job);
            if (! at_place(a.actor, j.place) || ! location_access(a.actor, j.place)) return false;
            if (id == "A21" && ! actor.heavy_allowed) return false;
            if (! j.fixture.empty() && ! usable(j.fixture)) return false;
            if (! j.target.empty() && ! usable(j.target)) return false;
            for (auto & t : j.tools) if (! usable(t) || state_.items.at(t).condition < get_or < double >(config_.item_type(state_.items.at(t).type),
            "required_condition", 0)) return false;
            for (auto & m : j.materials) if (state_.items.at(m).placement.kind != "escrow" || state_.items.at(m).placement.ref != j.id || ! permitted(a.actor,
            m)) return false;
        }
        if (id == "A11" || id == "A17" || id == "A18" || id == "A19" || id == "A27") return channel_available(a.actor, str(a.args,
        "to"), text(a.args, "channel", "speech"));
        if (id == "A14") {
            for (auto & who : get < std::set < Id >>(a.args, "participants")) if (who != a.actor && ! channel_available(a.actor,
            who, text(a.args, "channel", "speech"))) return false;
        }
        if (id == "A12") {
            const auto & p = state_.proposals.at(a.proposal);
            return p.version == a.proposal_version &&(p.state == "offered" || p.state == "countered" || p.state == "accepted") && ! p.executed && p.expires >= state_.time + 1;
        }
        if (id == "A13") {
            auto mode = str(a.args, "mode"), to = str(a.args, "to");
