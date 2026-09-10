            auto pid = str(c.args, "proposal");
            need(state_.proposals.contains(pid), "unknown agreement");
            const auto & p = state_.proposals.at(pid);
            need(p.state == "accepted" && ! p.executed && p.signatures == p.participants && p.version == get < std::int64_t >(c.args,
            "version"), "no signed current agreement");
            need(p.participants.contains(c.actor), "not a participant");
            auto kind = str(p.terms, "kind");
            need((c.type == "A32" && kind == "private") ||(c.type == "A15" && kind == "joint"), "wrong joint kind");
            d = get < Minute >(p.terms, "minutes");
            auto place = str(p.terms, "place");
            need(state_.time >= get < Minute >(p.terms, "start") && state_.time + d <= get < Minute >(p.terms, "end"), "outside signed window");
            a.participants.assign(p.participants.begin(), p.participants.end());
            a.commands.clear();
            for (auto & who : a.participants) {
                need(at_place(who, place) && awake(who) && state_.actors.at(who).active_action.empty() && location_access(who, place),
                "participant unavailable");
                auto ready = std::find_if(batch.begin(), batch.end(),[&](const Command & q) {
                    return q.actor == who && q.type == c.type && text(q.args, "proposal") == pid && get_or < std::int64_t >(q.args,
                    "version", 0) == p.version && state_.receipts.at(command_key(q)).status == "queued";
                });
                need(ready != batch.end(), "missing independently submitted current readiness");
                a.commands.push_back(command_key(* ready));
                if (c.type == "A32") need(config_.enabled(state_.profile, "intimacy") && state_.actors.at(who).needs.contains("N05") && state_.actors.at(who).needs.at("N05").enabled && state_.time / 1440 - state_.actors.at(who).birth_day >= 365 * 18,
                "adult module not available");
            }
            if (c.type == "A32") {
                need(a.participants.size() == 2 && state_.places.at(place).private_space, "private room required");
                for (auto &[who, other] : state_.actors) if (! p.participants.contains(who)) need(! at_place(who, place) || ! other.alive,
                "privacy interrupted");
            }
            a.proposal = pid;
            a.proposal_version = p.version;
            a.captured = p.terms;
            lock("place/" + place, state_.places.at(place).capacity, static_cast < Money >(a.participants.size()));
            lock("proposal/" + pid);
        } else if (c.type == "A16") {
            d = duration(c.args, 30, 120);
            auto id = str(c.args, "item");
            allowed(id);
            const auto & t = config_.item_type(item(id).type);
            need(has(t, "pleasure_per_30_min"), "no leisure affordance");
            auto topic = str(t, "category");
            need(actor.known_topics.contains(topic), "activity not known");
            if (topic == "sport") need(actor.heavy_allowed, "sport blocked");
            lock("item/" + id);
        } else if (c.type == "A17") {
            contact(str(c.args, "to"));
            auto op = str(c.args, "operator");
            need(std::set < std::string > {
                "WHO", "WHAT", "WHERE", "WHEN", "WHY", "CONFIRM", "COMPARE"
            }
            .contains(op), "invalid question operator");
            auto refs = known_evidence();
            need(! refs.empty() &&(op != "COMPARE" || refs.size() == 2), "question needs evidence");
        } else if (c.type == "A18") {
            contact(str(c.args, "to"));
            auto q = str(c.args, "question");
            need(state_.messages.contains(q) && state_.messages.at(q).readers.contains(c.actor) && text(state_.messages.at(q).content,
            "type") == "question", "question not received/read");
