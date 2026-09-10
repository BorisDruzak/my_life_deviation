            n.awake_drain = get < double >(v, "awake_drain_per_hour");
            n.sleep_drain = get < double >(v, "sleep_drain_per_hour");
            n.sleep_gain = get < double >(v, "sleep_net_gain_per_hour");
            n.critical = get < double >(v, "critical");
            n.activation = get < double >(v, "activation");
            n.target = get < double >(v, "target");
            a.needs[idn] = n;
        }
        a.known_methods = {
            "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A09", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18",
            "A19", "A20", "A22", "A23", "A24", "A25", "A26", "A27", "A28", "A29", "R01"
        };
        a.known_topics = {
            "meal", "canned", "snack", "conversation", "reading", "sport", "rest"
        };
        a.skills["repair"] = .5;
        a.skills["cooking"] = .2;
        a.drives["appropriation"] = {
            0, 0
        };
        return a;
    }
    Item make_item(const Id & id, const Id & type, const Id & owner, const Placement & placement, const Config & c) {
        const auto & d = c.item_type(type);
        Item x;
        x.id = id;
        x.type = type;
        x.owner = owner;
        x.placement = placement;
        x.total_units = get_or < std::int64_t >(d, "total_units", 0);
        x.remaining_units = x.total_units;
        x.open = get_or < bool >(d, "is_open", true);
        x.condition = get_or < double >(d, "condition", 1);
        return x;
    }
    State scenario_state(const Config & c, const Json & raw) {
        if (! raw.is_object()) throw InputError("scenario.world must be an object");
        Json j = raw;
        auto profile = text(j, "profile", "baseline");
        auto actors = has(j, "actors") ? at(j, "actors").as_object() : Object {};
        for (auto & kv : actors) {
            Json patch = kv.value();
            auto id = std::string(kv.key());
            auto base = encode(make_actor(id, "", c));
            if (has(patch, "needs")) {
                Json needs = at(base, "needs");
                for (const auto & nk : at(patch, "needs").as_object()) {
                    const auto key = std::string(nk.key());
                    if (key == "N05" && ! has(needs, key)) {
                        const auto & v = at(at(c.parameters, "needs"), key);
                        Need n;
                        n.awake_drain = get < double >(v, "awake_drain_per_hour");
                        n.sleep_drain = get < double >(v, "sleep_drain_per_hour");
                        n.critical = get < double >(v, "critical");
                        n.activation = get < double >(v, "activation");
                        n.target = get < double >(v, "target");
                        needs.as_object()[key] = encode(n);
                    }
                    if (! has(needs, key)) throw InputError("unknown need " + key);
                    if (nk.value().is_number()) needs.as_object()[key].as_object()["value"] = nk.value();
                    else needs.as_object()[key] = merged(at(needs, key), nk.value());
                }
                patch.as_object()["needs"] = needs;
            }
            for (auto key : {
                "traits", "norm_prices"
            }) if (has(patch, key)) patch.as_object()[key] = merged(at(base, key), at(patch, key));
            kv.value() = merged(base, patch);
        }
        j.as_object()["actors"] = actors;
        if (has(j, "items")) for (auto & kv : j.as_object()["items"].as_object()) {
            auto x = kv.value();
            auto base = encode(make_item(std::string(kv.key()), str(x, "type"), text(x, "owner"), decode < Placement >(at(x,
            "placement")), c));
            kv.value() = merged(base, x);
        }
        State s = decode < State >(j);
        s.profile = profile;
        if (! s.config_hash.empty() && s.config_hash != c.hash) throw InputError("scenario config hash mismatch");
        s.config_hash = c.hash;
