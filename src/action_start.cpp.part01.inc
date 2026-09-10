                    "record", "version", "delete", "statement", "acl", "public"
                }
            }, {
                "A30", {
                    "skill", "tool", "minutes"
                }
            }, {
                "A31", {}
            }, {
                "A32", {
                    "proposal", "version"
                }
            }
        };
    }
    bool World::try_start(const Command & c, const std::vector < Command > & batch) {
        auto & actor = state_.actors.at(c.actor);
        need(actor.alive && actor.capable, "actor incapacitated");
        need(actor.active_action.empty(), "actor busy");
        need(actor.position.kind == "at" ||(c.type == "A02" && actor.position.kind == "stranded"), "actor in transit");
        for (const auto & kv : c.args.as_object()) need(fields.at(c.type).contains(std::string(kv.key())), "unknown action parameter");
        for (const auto &[id, version] : c.expected_versions) {
            if (state_.items.contains(id)) need(state_.items.at(id).version == version, "object version changed");
            else if (state_.permissions.contains(id)) need(state_.permissions.at(id).version == version, "ACL version changed");
            else throw InputError("unknown versioned object");
        }
        Action a;
        a.id = command_key(c);
        a.type = c.type;
        a.actor = c.actor;
        a.participants = {
            c.actor
        };
        a.commands = {
            a.id
        };
        a.args = c.args;
        a.started = state_.time;
        a.intent = c.intent;
        Minute d = 1;
        std::vector < Reservation > locks;
        std::optional < Job > new_job;
        auto lock =[&](std::string key, Money capacity = 1, Money amount = 1) {
            if (amount == 0) return;
            Reservation r;
            r.resource = std::move(key);
            r.action = a.id;
            r.capacity = capacity;
            r.amount = amount;
            locks.push_back(r);
        };
        auto item =[&](const Id & id)->const Item & {
            auto it = state_.items.find(id);
            need(it != state_.items.end(), "unknown/unavailable item");
            return it->second;
        };
        auto reachable =[&](const Id & id) {
            need(accessible(c.actor, id), "item not physically accessible");
        };
        auto allowed =[&](const Id & id) {
            reachable(id);
            need(permitted(c.actor, id), "no permission");
        };
        auto takespace =[&](const Id & who, const Id & id) {
            auto add = holder(id) == who ? 0 : item_mass(id);
            need(carried_mass(who) + add <= state_.actors.at(who).capacity_kg + 1e-9, "carry limit");
            lock("inventory/" + who);
        };
        auto phone =[&]() {
            for (const auto &[id, x] : state_.items) if (x.type == "I10" && holder(id) == c.actor && accessible(c.actor, id)) return true;
            return false;
        };
        auto contact =[&](const Id & to) {
            need(channel_available(c.actor, to, text(c.args, "channel", "speech")), "contact unavailable");
        };
        auto known_evidence =[&]() {
            auto refs = get_or < std::vector < Id >>(c.args, "evidence", {});
            for (const auto & id : refs) need(std::any_of(actor.beliefs.begin(), actor.beliefs.end(),[&](const auto & e) {
                return e.id == id;
            }), "unknown evidence reference");
            return refs;
        };
        auto claim =[&]() {
            need(has(c.args, "statement"), "missing statement");
            auto s = decode < Statement >(at(c.args, "statement"));
            need(! s.subject.empty() && ! s.predicate.empty() && s.valid_from >= 0, "invalid statement");
            auto refs = known_evidence();
            need(! refs.empty() || get_or < bool >(c.args, "fabricated", false), "unfounded statement must be explicitly marked for private audit");
        };
