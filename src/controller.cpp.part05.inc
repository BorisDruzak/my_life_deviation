                d.command = x.cmd("A12", Object {{"proposal", pid}, {"version", get < std::int64_t >(p, "version")}, {"answer", ready && willingness > 0 ? "accept" : "decline"}, {"channel", x.owns_type("I10") ? "digital" : "speech"}}, d.goal);
                return d;
            }
            if ((state == "offered" || state == "countered") && signatures.contains(x.actor.id) && get < Minute >(p, "expires") > x.time) {
                if (background_wait.empty()) background_wait = "await_response";
            }
            if (state == "accepted") {
                bool executed = false;
                for (const auto & r : at(view, "receipts").as_array()) if (has(r, "request")) {
                    const auto & q = at(r, "request");
                    const auto & args = at(q, "args");
                    if (text(args, "proposal") != pid) continue;
                    const auto status = text(r, "status");
                    if (text(q, "type") == "A13" && status == "completed") executed = true;
                    if (text(q, "type") == "A15" && (status == "completed" || status == "interrupted") &&
                        get_or<std::int64_t>(args, "version", 0) == get<std::int64_t>(p, "version")) executed = true;
                }
                if (executed) continue;
                if ((kind == "loan_item" || kind == "loan_money") && text(terms, "lender") == x.actor.id) {
                    auto to = str(terms, "borrower");
                    if (kind == "loan_item") {
                        auto item = str(terms, "item");
                        if (! x.inventory.contains(item) || x.recently_rejected("A13", "proposal", pid, 10)) continue;
                        d.goal = "fulfil_loan";
                        d.command = x.cmd("A13", Object {{"mode", kind}, {"to", to}, {"item", item}, {"proposal", pid}}, d.goal);
                        return d;
                    }
                }
                if (kind == "loan_item" && text(terms, "borrower") == x.actor.id && ! x.inventory.contains(text(terms, "item")) && x.time < get < Minute >(terms, "due")) {
                    background_wait = "await_handoff";
                    continue;
                }
                if (kind == "joint" && x.time + get < Minute >(terms, "minutes") <= get < Minute >(terms, "end")) {
                    auto route = x.route(str(terms, "place"));
                    if (! route.possible) continue;
                    d.goal = "attend_agreement";
                    if (! route.first.empty()) d.command = x.cmd("A02", Object {{"edge", route.first}}, d.goal);
                    else if (x.time >= get < Minute >(terms, "start")) d.command = x.cmd("A15", Object {{"proposal", pid}, {"version", get < std::int64_t >(p, "version")}}, d.goal);
                    return d;
                }
            }
        }
        if (mode == "acquire") {
            d.goal = "acquire_item";
