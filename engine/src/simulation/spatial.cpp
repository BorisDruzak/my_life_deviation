#include "npc/world.hpp"
#include <algorithm>
namespace npc {
    bool World::at_place(const Id & actor, const Id & place) const {
        auto it = state_.actors.find(actor);
        return it != state_.actors.end() && it->second.position.kind == "at" && it->second.position.place == place;
    }
    std::string World::item_place(const Id & id) const {
        std::set < Id > seen;
        Id cur = id;
        while (state_.items.contains(cur)) {
            if (! seen.insert(cur).second) throw InvariantError("containment cycle");
            const auto & p = state_.items.at(cur).placement;
            if (p.kind == "at") return p.ref;
            if (p.kind == "inventory") {
                const auto & a = state_.actors.at(p.ref);
                return a.position.kind == "at" ? a.position.place : std::string();
            }
            if (p.kind == "in" || p.kind == "installed") {
                cur = p.ref;
                continue;
            }
            if (p.kind == "escrow") return state_.jobs.at(p.ref).place;
            return {};
        }
        return {};
    }
    std::string World::holder(const Id & id) const {
        std::set < Id > seen;
        Id cur = id;
        while (state_.items.contains(cur)) {
            if (! seen.insert(cur).second) throw InvariantError("containment cycle");
            const auto & p = state_.items.at(cur).placement;
            if (p.kind == "inventory") return p.ref;
            if (p.kind == "in" || p.kind == "installed") {
                cur = p.ref;
                continue;
            }
            return {};
        }
        return {};
    }
    double World::item_mass(const Id & id, std::set < Id > * visiting) const {
        std::set < Id > local;
        if (! visiting) visiting = & local;
        if (! visiting->insert(id).second) throw InvariantError("containment cycle");
        const auto & i = state_.items.at(id);
        double m = get_or < double >(config_.item_type(i.type), "mass_kg", 0);
        if (i.placement.kind == "tombstone") m = 0;
        else if (i.total_units > 0) m *= static_cast < double >(i.remaining_units) / i.total_units;
        for (const auto &[other, x] : state_.items) if ((x.placement.kind == "in" || x.placement.kind == "installed") && x.placement.ref == id) m += item_mass(other,
        visiting);
        visiting->erase(id);
        return m;
    }
    double World::carried_mass(const Id & actor) const {
        double m = 0;
        for (const auto &[id, i] : state_.items) if (i.placement.kind == "inventory" && i.placement.ref == actor) m += item_mass(id);
        return m;
    }
    bool World::accessible(const Id & actor, const Id & id) const {
        if (! state_.items.contains(id) || ! state_.actors.contains(actor)) return false;
        if (! at_place(actor, item_place(id))) return false;
        auto cur = id;
        std::set < Id > seen;
        while (state_.items.contains(cur)) {
            if (! seen.insert(cur).second) return false;
            const auto & p = state_.items.at(cur).placement;
            if (p.kind == "in") {
                auto it = state_.items.find(p.ref);
                if (it == state_.items.end() || ! it->second.open) return false;
                cur = p.ref;
                continue;
            }
            if (p.kind == "installed") return false;
            if (p.kind == "inventory") return p.ref == actor;
            return p.kind == "at";
        }
        return false;
    }
    bool World::permitted(const Id & actor, const Id & id) const {
        auto it = state_.items.find(id);
        if (it == state_.items.end()) return false;
        if (it->second.owner == actor) return true;
        auto acl = state_.permissions.find(id);
        if (acl != state_.permissions.end() && acl->second.denied.contains(actor)) return false;
        if (it->second.owner == "public") return true;
        for (const auto &[key, o] : state_.obligations) if (o.kind == "return_item" && o.item == id && o.debtor == actor &&(o.status == "pending" || o.status == "breached")) return true;
        auto p = state_.permissions.find(id);
        return p != state_.permissions.end() && p->second.grantees.contains(actor);
    }
    bool World::location_access(const Id & actor, const Id & place) const {
        auto it = state_.places.find(place);
        if (it == state_.places.end()) return false;
        if (it->second.owner == actor) return true;
        auto acl = state_.permissions.find(place);
        if (acl != state_.permissions.end() && acl->second.denied.contains(actor)) return false;
        if (it->second.public_entry) return true;
        auto p = state_.permissions.find(place);
        return p != state_.permissions.end() && p->second.grantees.contains(actor);
    }
    bool World::awake(const Id & id) const {
        const auto & a = state_.actors.at(id);
        if (! a.alive || ! a.capable) return false;
        if (a.active_action.empty()) return true;
        auto it = state_.actions.find(a.active_action);
        return it == state_.actions.end() || it->second.type != "A09";
    }
    bool World::open_interval(const Id & place, Minute start, Minute end) const {
        if (! state_.places.contains(place) || end <= start) return false;
        const auto & windows = state_.places.at(place).open_windows;
        for (Minute t = start; t < end; ++ t) {
            bool covered = false;
            const auto tod = t % 1440;
            for (const auto & w : windows) if (w[0] <= tod && tod < w[1]) covered = true;
            if (! covered) return false;
        }
        return true;
    }
    bool World::channel_available(const Id & from, const Id & to, const std::string & channel) const {
        if (from == to || ! state_.actors.contains(from) || ! state_.actors.contains(to)) return false;
        if (channel == "speech") return awake(to) && state_.actors.at(from).position.kind == "at" && at_place(to, state_.actors.at(from).position.place);
        if (channel != "digital") return false;
        auto phone =[&](const Id & a) {
            for (const auto &[id, x] : state_.items) if (x.type == "I10" && holder(id) == a) return true;
            return false;
        };
        return phone(from) && phone(to);
    }
    bool World::record_access(const Id & actor, const RecordVersion & v) const {
        return ! v.deleted &&(v.public_read || v.acl.contains(actor));
    }
    Money World::available(const Id & account, const Id & own) const {
        auto it = state_.accounts.find(account);
        if (it == state_.accounts.end()) return 0;
        Money value = it->second;
        for (const auto &[id, r] : state_.reservations) if (r.resource == "money/" + account && r.action != own) value -= r.amount;
        return value;
    }
    bool World::lockable(const std::vector < Reservation > & locks, const Id & own) const {
        std::map < Id, Money > sum;
        std::map < Id, Money > capacities;
        for (const auto & r : locks) {
            if (r.amount <= 0 || r.capacity < r.amount) return false;
            if (capacities.contains(r.resource) && capacities.at(r.resource) != r.capacity) return false;
            capacities[r.resource] = r.capacity;
            if (sum[r.resource] > r.capacity - r.amount) return false;
            sum[r.resource] += r.amount;
        }
        for (const auto &[id, r] : state_.reservations) {
            if (r.action == own || ! sum.contains(r.resource)) continue;
            const auto capacity = capacities.at(r.resource);
            if (sum[r.resource] > capacity - r.amount) return false;
            sum[r.resource] += r.amount;
        }
        return true;
    }
}
