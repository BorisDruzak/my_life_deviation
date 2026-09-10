            if (has(a.args, "item")) {
                auto item = str(a.args, "item");
                if (! accessible(a.actor, item) || holder(item) != a.actor || ! channel_available(a.actor, to, "speech") || carried_mass(to) + item_mass(item) > state_.actors.at(to).capacity_kg + 1e-9) return false;
                if ((mode == "gift" || mode == "loan_item") && state_.items.at(item).owner != a.actor) return false;
            }
            if (a.price > 0 &&(available(actor.account, a.id) < a.price || state_.accounts.at(state_.actors.at(to).account) > max_money - a.price)) return false;
            if (! a.proposal.empty()) {
                const auto & p = state_.proposals.at(a.proposal);
                if (p.state != "accepted" || p.version != a.proposal_version || p.executed) return false;
            }
            if (has(a.args, "obligation")) {
                const auto & o = state_.obligations.at(str(a.args, "obligation"));
                if (o.status != "pending" && o.status != "breached") return false;
                if (mode == "repay" && a.price > o.remaining) return false;
            }
        }
        if (id == "A15" || id == "A32") {
            const auto & p = state_.proposals.at(a.proposal);
            if (p.state != "accepted" || p.version != a.proposal_version || p.signatures != p.participants) return false;
            auto place = str(p.terms, "place");
            for (auto & who : a.participants) if (! at_place(who, place) || ! location_access(who, place)) return false;
            if (id == "A32") for (const auto &[who, x] : state_.actors) if (! p.participants.contains(who) && x.alive && at_place(who,
            place)) return false;
        }
        if (id == "A16") {
            auto item = str(a.args, "item");
            if (! usable(item)) return false;
            if (text(config_.item_type(state_.items.at(item).type), "category") == "sport" && ! actor.heavy_allowed) return false;
        }
        if (id == "A24" && has(a.args, "record")) {
            auto & r = state_.records.at(str(a.args, "record"));
            const auto & v = r.versions.back();
            if (! v.deleted && ! record_access(a.actor, v)) return false;
            return record_access(a.actor, decode < RecordVersion >(a.captured));
        }
        if (id == "A25") {
            auto r = str(a.args, "resource");
            auto it = state_.permissions.find(r);
            auto version = it == state_.permissions.end() ? 1 : it->second.version;
            auto authority = it == state_.permissions.end() ?(state_.items.contains(r) ? state_.items.at(r).owner : state_.places.at(r).owner) : it->second.authority;
            return authority == a.actor && version == get < std::int64_t >(a.args, "version");
        }
        if (id == "A26") return usable(str(a.args, "item"));
        if (id == "A29") {
            const auto & r = state_.records.at(str(a.args, "record"));
            return r.author == a.actor && r.versions.back().version == get < std::int64_t >(a.args, "version");
        }
        if (id == "A30") return usable(str(a.args, "tool"));
        if (id == "A31") for (const auto &[who, x] : state_.actors) if (who != a.actor && x.alive && at_place(who, actor.position.place)) return false;
        return true;
    }
    void World::move_item(const Id & id, const Placement & where, const Id & owner, const Action & a, const std::string & type) {
        auto & i = state_.items.at(id);
        auto before = encode(i.placement);
        i.placement = where;
        if (! owner.empty()) i.owner = owner;
        ++ i.version;
        for (auto &[sid, sh] : state_.shops) if (sh.stock.contains(id) && !(where.kind == "at" && where.ref == sh.place &&(i.owner == sid || i.owner == sh.account))) sh.stock.erase(id);
        auto actors = a.participants;
        if (where.kind == "inventory" && std::find(actors.begin(), actors.end(), where.ref) == actors.end()) actors.push_back(where.ref);
