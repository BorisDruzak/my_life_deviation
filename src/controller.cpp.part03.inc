                const double p =(2.0 + static_cast < double >(yes)) /(4.0 + static_cast < double >(yes + no));
                auto route = x.route(text(b.statement.arguments, "place"));
                if (! route.possible) continue;
                std::string channel = x.owns_type("I10") ? "digital" : "speech";
                const auto time =(channel == "digital" ? 3.0 : route.minutes + 2);
                if (x.time + time >= due) continue;
                Candidate c;
                c.method = "borrow";
                c.expected_minutes = time;
                c.score = p * 30 -(1 - p) *(5 + 10 * x.actor.traits.at("caution")) + 3 * r.trust - 3 *(1 - x.actor.traits.at("social_initiative")) - delay_price * time;
                c.assumptions = {"holder from " + b.id, "estimated acceptance=" + std::to_string(p), "other actor decides independently"};
                if (channel == "speech" && ! route.first.empty()) c.first = x.cmd("A02", Object {{"edge", route.first}}, d.goal);
                else c.first = x.cmd("A11", Object {{"to", holder}, {"channel", channel}, {"expires", x.time + 120}, {"terms", Object {{"kind", "loan_item"}, {"lender", holder}, {"borrower", x.actor.id}, {"item", item}, {"due", due}}}}, d.goal);
                d.candidates.push_back(c);
            }
            if (d.candidates.empty()) for (const auto &[shop, b] : x.stores) {
                if (x.time - b.learned_at < 60 || ! at(b.statement.arguments, "offers").as_array().empty()) continue;
                auto route = x.route(str(b.statement.arguments, "place"));
                if (! route.possible) continue;
                Candidate p;
                p.method = "inspect_store";
                p.score = - 2 - route.minutes;
                p.expected_minutes = route.minutes + 2;
                p.assumptions = {"refresh stale stock"};
                p.first = route.first.empty() ? x.cmd("A03", Object {{"place", str(b.statement.arguments, "place")}, {"scope", "stock"}}, d.goal) : x.cmd("A02", Object {{"edge", route.first}}, d.goal);
                d.candidates.push_back(p);
            }
            choose(d, x);
        }
    }
    Decision decide(const Config & config, const Json & view, const Json & objective) {
        Local x(config, view);
        Decision d;
        d.actor = x.actor.id;
        d.time = x.time;
        const auto mode = text(objective, "mode", "household");
        if (mode != "household" && mode != "acquire" && mode != "repair" && mode != "passive") throw InputError("unknown controller mode");
        if (! x.actor.alive || ! x.actor.capable || ! x.actor.active_action.empty() || mode == "passive") { d.goal = "unavailable"; return d; }
        if (x.actor.position.kind == "stranded") {
            d.goal = "resume_route";
            d.command = x.cmd("A02", Object {{"edge", x.actor.position.edge}}, d.goal);
            return d;
        }
