            need(actor.known_methods.contains(j.recipe), "recipe not known");
            const auto & recipe = config_.recipes.at(j.recipe);
            auto sk = actor.skills.find(j.recipe == "R01" ? "cooking" : "repair");
            need((sk == actor.skills.end() ? 0 : sk->second) >= get < double >(recipe, "min_skill"), "skill insufficient");
            if (j.recipe == "R01") {
                allowed(j.fixture);
                need(item(j.fixture).type == "F02", "kitchen required");
                lock("item/" + j.fixture);
            } else {
                need(actor.heavy_allowed, "heavy work temporarily blocked");
                allowed(j.target);
                need(item(j.target).type == text(recipe, "target"), "wrong technical target");
                lock("item/" + j.target);
            }
            for (auto & id : j.tools) {
                allowed(id);
                const auto & i = item(id);
                const auto & t = config_.item_type(i.type);
                if (j.recipe == "R02") need(i.type == "I05" && i.condition >= get_or < double >(t, "required_condition", .5), "unsuitable repair tool");
                if (j.recipe == "R03") need(get_or < double >(t, "length_m", 0) >= 3 && get_or < double >(t, "strength_game_units",
                0) >= 20, "fastener properties insufficient");
                lock("item/" + id);
            }
            for (auto & id : j.materials) {
                if (! new_job) need(item(id).placement.kind == "escrow" && item(id).placement.ref == j.id, "lost job material");
                lock("item/" + id);
            }
            a.job = j.id;
            d = j.required - j.worked;
            need(d > 0, "job already completed");
        } else if (c.type == "A09") {
            d = duration(c.args, 60, 720);
            auto id = str(c.args, "bed");
            allowed(id);
            need(item(id).type == "F01", "sleep place required");
            lock("item/" + id);
        } else if (c.type == "A10") {
            need(actor.heavy_allowed, "heavy work blocked");
            d = duration(c.args, 60, 480);
            auto id = str(c.args, "contract");
            need(state_.contracts.contains(id), "employment unavailable");
            const auto & ct = state_.contracts.at(id);
            need(ct.worker == c.actor, "not your contract");
            allowed(ct.workplace);
            for (Minute t = state_.time; t < state_.time + d; ++ t) {
                bool okay = false;
                for (auto & w : ct.windows) if (w[0] <= t % 1440 && t % 1440 < w[1]) okay = true;
                need(okay, "outside work schedule");
            }
            lock("item/" + ct.workplace);
            lock("contract/" + id);
        } else if (c.type == "A11" || c.type == "A14") {
            std::set < Id > participants;
            if (c.type == "A11") participants = {
                c.actor, str(c.args, "to")
            };
            else {
                participants = get < std::set < Id >>(c.args, "participants");
                participants.insert(c.actor);
            }
            need(participants.size() >= 2 && participants.size() <= 16, "invalid participant count");
            for (auto & who : participants) if (who != c.actor) contact(who);
            auto expires = get_or < Minute >(c.args, "expires", state_.time + 120);
            need(expires > state_.time + 1 && expires <= state_.time + 10080, "invalid expiry");
            validate_terms(c.actor, participants, at(c.args, "terms"));
        } else if (c.type == "A12") {
            auto pid = str(c.args, "proposal");
            need(state_.proposals.contains(pid), "unknown proposal");
            const auto & p = state_.proposals.at(pid);
            need(p.participants.contains(c.actor) && p.version == get < std::int64_t >(c.args, "version") && known_proposal(c.actor,
            pid, p.version), "proposal version not received");
