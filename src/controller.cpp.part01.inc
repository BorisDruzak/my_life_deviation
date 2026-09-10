                            q.emplace(n, e.to, first.empty() ? id : first);
                        }
                    }
                }
                return {0, "", false};
            }
            bool recently_rejected(const std::string & type, const std::string & field, const Id & id, Minute within = 30) const {
                for (const auto & r : at(view, "receipts").as_array()) if (has(r, "request") && text(r, "status") == "rejected" && time - get_or < Minute >(r,
                "received", 0) < within) {
                    const auto & request = at(r, "request");
                    if (text(request, "type") == type && text(at(request, "args"), field) == id) return true;
                }
                return false;
            }
            bool purchased_since(const Id & item, Minute since) const {
                for (const auto & b : actor.beliefs) if (b.statement.predicate == "event" && b.learned_at >= since && text(b.statement.arguments,
                "type") == "purchase" && has(b.statement.arguments, "data") && text(at(b.statement.arguments, "data"), "item") == item) return true;
                return false;
            }
            bool owns_type(const std::string & type) const {
                for (auto &[id, j] : inventory) if (str(j, "type") == type) return true;
                return false;
            }
            Relation relation(const Id & id) const {
                auto it = actor.relations.find(id);
                return it == actor.relations.end() ? Relation {} : it->second;
            }
        };
        void choose(Decision & d, const Local & x) {
            std::stable_sort(d.candidates.begin(), d.candidates.end(),[](const Candidate & a, const Candidate & b) { return a.score > b.score; });
            if (d.candidates.size() > static_cast < std::size_t >(x.actor.search_width)) d.candidates.resize(static_cast < std::size_t >(x.actor.search_width));
            if (! d.candidates.empty() && d.candidates.front().first) d.command = d.candidates.front().first;
        }
        void acquire(Decision & d, const Local & x, const std::string & type, Minute due) {
            if (x.owns_type(type)) { d.goal = "already_available"; return; }
            const double delay_price = .02 + .08 *(1 - x.actor.traits.at("patience"));
            for (const auto &[shop, b] : x.stores) {
                auto place = str(b.statement.arguments, "place");
                auto route = x.route(place);
                if (! route.possible || x.time + route.minutes + 3 > due) continue;
                Minute last_failure = - 1;
                for (const auto & r : at(x.view, "receipts").as_array()) if (has(r, "request") && text(r, "status") == "rejected") {
                    const auto & request = at(r, "request");
                    if (text(request, "type") == "A06" && text(at(request, "args"), "shop") == shop) last_failure = std::max(last_failure,
                    get_or < Minute >(r, "received", 0));
                }
                if (last_failure >= b.learned_at) {
                    Candidate refresh;
                    refresh.method = "refresh_after_failure";
                    refresh.score = 1;
                    refresh.expected_minutes = route.minutes + 2;
                    refresh.assumptions = {"execution contradicted the previous stock belief"};
                    refresh.first = route.first.empty() ? x.cmd("A03", Object {{"place", place}, {"scope", "stock"}}, d.goal) : x.cmd("A02", Object {{"edge", route.first}}, d.goal);
