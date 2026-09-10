        if (c.type == "A01" || c.type == "A23") {
            d = duration(c.args, 1, 120);
            need(location_access(c.actor, actor.position.place), "place unavailable");
        } else if (c.type == "A02") {
            auto eid = str(c.args, "edge");
            need(state_.edges.contains(eid), "unknown edge");
            const auto & e = state_.edges.at(eid);
            need(e.open, "edge closed");
            double progress = 0;
            if (actor.position.kind == "stranded") {
                need(actor.position.edge == eid, "must resume the same edge");
                progress = actor.position.progress;
            } else need(actor.position.place == e.from, "not at edge origin");
            need(location_access(c.actor, e.to), "destination access denied");
            if (! e.key.empty()) need(holder(e.key) == c.actor, "key required");
            need(carried_mass(c.actor) <= actor.capacity_kg + 1e-9, "carry limit");
            double speed = 1;
            if (config_.enabled(state_.profile, "variable_movement_speed")) speed = std::max(config_.number({
                "physical", "movement_min_speed"
            }), actor.base_speed *(1 - config_.number({
                "physical", "movement_fatigue_factor"
            }) * actor.fatigue) *(config_.number({
                "physical", "movement_health_base"
            }) + config_.number({
                "physical", "movement_health_factor"
            }) * actor.health));
            d = std::max < Minute >(1, static_cast < Minute >(std::ceil(e.minutes *(1 - progress) / speed)));
            a.captured = Object {
                {
                    "base_progress", progress
                }, {
                    "edge", eid
                }
            };
        } else if (c.type == "A03") {
            need(text(c.args, "scope", "all") == "all" || text(c.args, "scope") == "stock", "unknown observation scope");
            d = 2;
            if (has(c.args, "item")) {
                reachable(str(c.args, "item"));
            } else need(at_place(c.actor, str(c.args, "place")), "place not visible");
        } else if (c.type == "A04" || c.type == "A20") {
            auto id = str(c.args, "item");
            reachable(id);
            need(holder(id) != c.actor, "item already held; no transfer occurs");
            need(portable(config_.item_type(item(id).type)), "object fixed in place");
            if (c.type == "A04") need(permitted(c.actor, id), "permission required");
            else need(! permitted(c.actor, id), "use the permitted transfer action");
            takespace(c.actor, id);
            lock("item/" + id);
            a.captured = Object {
                {
                    "owner", item(id).owner
                }, {
                    "placement", encode(item(id).placement)
                }
            };
        } else if (c.type == "A05") {
            auto id = str(c.args, "item");
            reachable(id);
            need(holder(id) == c.actor, "not carrying the item");
            lock("item/" + id);
            lock("inventory/" + c.actor);
            if (has(c.args, "container")) {
                auto dst = str(c.args, "container");
                allowed(dst);
                const auto & box = item(dst);
                const auto & t = config_.item_type(box.type);
                need(box.open && has(t, "capacity_kg"), "container closed or unsuitable");
                auto parent = dst;
                std::set < Id > seen;
                while (state_.items.contains(parent) && seen.insert(parent).second) {
                    need(parent != id, "containment cycle");
                    const auto & p = state_.items.at(parent).placement;
                    if (p.kind == "in" || p.kind == "installed") parent = p.ref;
                    else break;
                }
                double contained = item_mass(dst) - get_or < double >(t, "mass_kg", 0);
