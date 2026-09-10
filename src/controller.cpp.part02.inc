                    d.candidates.push_back(refresh);
                    continue;
                }
                Minute wait = 0;
                if (has(b.statement.arguments, "open_windows")) {
                    const auto windows = decode < std::vector < std::vector < Minute >> >(at(b.statement.arguments, "open_windows"));
                    bool found = false;
                    for (Minute offset = 0; offset <= 1440; ++ offset) {
                        bool fits = true;
                        for (Minute dt = 0; dt < 3; ++ dt) {
                            auto t =(x.time + static_cast < Minute >(route.minutes) + offset + dt) % 1440;
                            bool open = false;
                            for (const auto & w : windows) if (w.size() == 2 && w[0] <= t && t < w[1]) open = true;
                            if (! open) fits = false;
                        }
                        if (fits) { wait = offset; found = true; break; }
                    }
                    if (! found || x.time + route.minutes + wait + 3 > due) continue;
                }
                for (const auto & offer : at(b.statement.arguments, "offers").as_array()) {
                    if (str(offer, "type") != type) continue;
                    auto item = str(offer, "item");
                    auto price = get < Money >(offer, "price");
                    if (price > x.money || x.purchased_since(item, b.learned_at) || x.recently_rejected("A06", "item", item)) continue;
                    Candidate p;
                    p.method = "buy";
                    p.expected_minutes = route.minutes + static_cast < double >(wait) + 3;
                    p.score = 30 - 10 * std::log1p(static_cast < double >(price) / std::max(1.0, static_cast < double >(x.money))) - delay_price * p.expected_minutes;
                    p.assumptions = {"stock from " + b.id, "availability is checked only on execution"};
                    p.first = ! route.first.empty() ? x.cmd("A02", Object {{"edge", route.first}}, d.goal) : wait > 0 ? x.cmd("A01", Object {{"minutes", std::min < Minute >(30, wait)}}, d.goal) : x.cmd("A06", Object {{"shop", shop}, {"item", item}}, d.goal);
                    d.candidates.push_back(p);
                }
            }
            for (const auto &[item, b] : x.known_items) {
                if (text(b.statement.arguments, "type") != type) continue;
                auto holder = text(b.statement.arguments, "holder");
                if (holder.empty() || holder == x.actor.id) continue;
                bool addressed = false;
                for (const auto &[pid, e] : x.proposals) {
                    const auto & terms = at(e.statement.arguments, "terms");
                    if (text(terms, "item") == item && text(terms, "borrower") == x.actor.id) addressed = true;
                }
                if (addressed || x.recently_rejected("A11", "to", holder)) continue;
                const auto r = x.relation(holder);
                const auto key = holder + "/loan_item/";
                const auto yes = x.actor.outcome_counts.contains(key + "accepted") ? x.actor.outcome_counts.at(key + "accepted") : 0;
                const auto no = x.actor.outcome_counts.contains(key + "declined") ? x.actor.outcome_counts.at(key + "declined") : 0;
