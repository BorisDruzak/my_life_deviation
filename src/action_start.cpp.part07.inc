            need(state_.messages.at(q).author == str(c.args, "to"), "wrong question author");
            auto mode = text(c.args, "mode", "truth");
            need(mode == "truth" || mode == "partial" || mode == "refuse" || mode == "lie", "unknown answer mode");
            if (mode != "refuse") claim();
        } else if (c.type == "A19") {
            contact(str(c.args, "to"));
            claim();
        } else if (c.type == "A24") {
            if (has(c.args, "message")) {
                auto id = str(c.args, "message");
                need(state_.messages.contains(id), "message unavailable");
                const auto & m = state_.messages.at(id);
                need(m.delivered && std::find(m.recipients.begin(), m.recipients.end(), c.actor) != m.recipients.end(), "not delivered to this reader");
                if (m.channel == "digital") need(phone(), "digital interface unavailable");
                a.captured = m.content;
            } else {
                auto id = str(c.args, "record");
                need(state_.records.contains(id), "record unavailable");
                const auto & r = state_.records.at(id);
                const auto & v = r.versions.back();
                need(record_access(c.actor, v), "record access denied");
                need(get_or < std::int64_t >(c.args, "version", v.version) == v.version, "record version changed");
                need(phone(), "digital interface unavailable");
                a.captured = encode(v);
            }
        } else if (c.type == "A25") {
            auto id = str(c.args, "resource"), who = str(c.args, "grantee");
            need(state_.actors.contains(who), "unknown grantee");
            need(state_.items.contains(id) || state_.places.contains(id), "unsupported ACL target");
            auto authority = state_.permissions.contains(id) ? state_.permissions.at(id).authority : state_.items.contains(id) ? state_.items.at(id).owner : state_.places.at(id).owner;
            auto v = state_.permissions.contains(id) ? state_.permissions.at(id).version : 1;
            need(authority == c.actor && get < std::int64_t >(c.args, "version") == v, "ACL authority/version mismatch");
            static_cast < void >(get < bool >(c.args, "grant"));
            lock("acl/" + id);
        } else if (c.type == "A26") {
            auto id = str(c.args, "item");
            allowed(id);
            const auto & i = item(id);
            need(has(config_.item_type(i.type), "capacity_kg"), "not a container");
            if (i.locked) need(! i.key.empty() && holder(i.key) == c.actor, "key required");
            static_cast < void >(get < bool >(c.args, "open"));
            lock("item/" + id);
        } else if (c.type == "A27") {
            auto to = str(c.args, "to"), oid = str(c.args, "obligation");
            contact(to);
            need(state_.obligations.contains(oid), "unknown obligation");
            const auto & o = state_.obligations.at(oid);
            need(o.creditor == c.actor && o.debtor == to, "not the creditor");
            need(! text(c.args, "episode").empty(), "conversation episode required");
        } else if (c.type == "A28") {
            d = 2;
            need(phone(), "publication interface unavailable");
            claim();
            for (auto & who : get_or < std::set < Id >>(c.args, "acl", {})) need(state_.actors.contains(who), "unknown reader");
        } else if (c.type == "A29") {
            auto id = str(c.args, "record");
            need(state_.records.contains(id), "record unavailable");
            const auto & r = state_.records.at(id);
            need(r.author == c.actor && r.versions.back().version == get < std::int64_t >(c.args, "version"), "not author or stale version");
            need(phone(), "publication interface unavailable");
            for (auto & who : get_or < std::set < Id >>(c.args, "acl", {})) need(state_.actors.contains(who), "unknown reader");
