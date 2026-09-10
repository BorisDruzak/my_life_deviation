                }
            }, d.goal);
            return d;
        }
        // A pending social exchange does not outrank immediately available food or sleep.
        // Keep the availability/passive guards above this check; do not interrupt active actions.
        const auto & food = x.actor.needs.at("N01");
        const auto & sleep = x.actor.needs.at("N02");
        std::string food_item;
        for (auto &[id, i] : x.inventory) if (get_or < bool >(config.item_type(str(i, "type")), "edible", false) && get_or < std::int64_t >(i,
        "remaining_units", 0) > 0) {
            food_item = id;
            break;
        }
        if (food.value <= food.activation && ! food_item.empty()) {
            d.goal = "eat";
            d.command = x.cmd("A07", Object {{"item", food_item}}, d.goal);
            return d;
        }
        if (sleep.value <= sleep.activation) {
            for (const auto &[id, b] : x.known_items) if (text(b.statement.arguments, "type") == "F01") {
                auto route = x.route(str(b.statement.arguments, "place"));
                if (! route.possible) continue;
                d.goal = "sleep";
                Minute duration = std::min < Minute >(720, std::max < Minute >(1, static_cast < Minute >(std::ceil((sleep.target - sleep.value) * 60 / std::max(1.0,
                sleep.sleep_gain)))));
                d.command = route.first.empty() ? x.cmd("A09", Object {{"bed", id}, {"minutes", duration}}, d.goal) : x.cmd("A02", Object {{"edge", route.first}}, d.goal);
                return d;
            }
        }
        // Reading is separate from delivery. No answer can be made using an unread body.
        for (const auto & m : at(view, "inbox").as_array()) if (! get < bool >(m, "read")) {
            d.goal = "read_message";
            d.command = x.cmd("A24", Object {{"message", str(m, "id")}}, d.goal);
            return d;
        }
        // This is only an idle fallback, not an exclusive action or an inbox barrier.
        std::string background_wait;
        for (const auto &[pid, b] : x.proposals) {
            const auto & p = b.statement.arguments;
            auto state = text(p, "state");
            const auto & terms = at(p, "terms");
            auto kind = str(terms, "kind");
            auto signatures = get < std::set < Id >>(p, "signatures");
            if ((state == "offered" || state == "countered") && ! signatures.contains(x.actor.id) && get < Minute >(p, "expires") > x.time + 1) {
                bool ready = x.actor.needs.at("N01").value > 20 && x.actor.needs.at("N02").value > 15;
                const auto r = x.relation(text(p, "author"));
                double willingness = 8 * r.trust + 4 * r.affection - 3;
                if (kind == "loan_item" && text(terms, "lender") == x.actor.id) {
                    auto item = text(terms, "item");
                    ready = ready && x.inventory.contains(item) && text(x.inventory.at(item), "owner") == x.actor.id;
                } else if (kind == "loan_money" && text(terms, "lender") == x.actor.id) ready = ready && x.money >= get < Money >(terms,
                "amount") + 40;
                else if (kind == "joint" || kind == "private") {
                    auto route = x.route(str(terms, "place"));
                    ready = ready && route.possible && x.time + route.minutes + get < Minute >(terms, "minutes") <= get < Minute >(terms, "end");
                    if (kind == "private") ready = false;
                } else if (kind == "repair") ready = ready && x.actor.known_methods.contains("R02");
                d.goal = "answer_proposal";
