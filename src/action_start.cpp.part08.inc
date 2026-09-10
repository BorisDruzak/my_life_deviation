            if (! get_or < bool >(c.args, "delete", false)) static_cast < void >(decode < Statement >(at(c.args, "statement")));
            lock("record/" + id);
        } else if (c.type == "A30") {
            need(config_.enabled(state_.profile, "skill_learning"), "learning profile disabled");
            d = duration(c.args, 30, 120);
            auto skill = str(c.args, "skill"), tool = str(c.args, "tool");
            need(skill == "repair" || skill == "cooking", "unknown practice contract");
            allowed(tool);
            need((skill == "repair" && item(tool).type == "I05") ||(skill == "cooking" && item(tool).type == "F02"), "wrong practice tool");
            lock("item/" + tool);
        } else if (c.type == "A31") {
            d = 10;
            need(config_.enabled(state_.profile, "intimacy") && actor.needs.contains("N05") && actor.needs.at("N05").enabled && state_.time / 1440 - actor.birth_day >= 365 * 18,
            "adult module disabled");
            need(state_.places.at(actor.position.place).private_space && location_access(c.actor, actor.position.place), "private place required");
            for (auto &[who, x] : state_.actors) if (who != c.actor) need(! at_place(who, actor.position.place) || ! x.alive,
            "privacy interrupted");
        } else throw InputError("unimplemented action type");
        need(d > 0 && d <= 14400 && state_.time <= max_minute - d, "invalid fixed duration");
        a.due = state_.time + d;
        for (auto & who : a.participants) lock("actor/" + who);
        need(lockable(locks), "required resources busy");
        // All checks precede changes. From here on there are no expected input failures.
        state_.actions.emplace(a.id, a);
        for (std::size_t i = 0; i < locks.size(); ++ i) {
            auto r = locks[i];
            r.id = a.id + "/lock/" + std::to_string(i);
            r.until = a.due;
            state_.reservations.emplace(r.id, r);
        }
        for (auto & who : a.participants) state_.actors.at(who).active_action = a.id;
        for (auto & key : a.commands) {
            auto & r = state_.receipts.at(key);
            r.status = "started";
            r.action = a.id;
        }
        // The departure event captures the origin geometry, before changing position.
        emit(a.id, c.type == "A02" ? "departed" : c.type == "A09" ? "sleep_started" : c.type == "A31" || c.type == "A32" ? "private_activity_started" : c.type == "A15" ? "joint_started" : "action_started",
        a.participants, {}, Object {
            {
                "action_type", c.type
            }
        }, c.type == "A31" || c.type == "A32", {}, Object {
            {
                "intent", c.intent
            }
        });
        if (c.type == "A02") {
            const auto & e = state_.edges.at(str(c.args, "edge"));
            Position p;
            p.kind = "transit";
            p.edge = e.id;
            p.from = e.from;
            p.to = e.to;
            p.started = state_.time;
            p.due = a.due;
            p.base_progress = get < double >(a.captured, "base_progress");
            p.progress = p.base_progress;
            actor.position = p;
        }
        if (! a.job.empty()) {
            if (new_job) {
                auto j = * new_job;
                j.active_action = a.id;
                j.status = "active";
                state_.jobs[j.id] = j;
                for (auto & x : j.materials) {
                    auto & i = state_.items.at(x);
                    i.placement = {
                        "escrow", j.id
                    };
                    ++ i.version;
                }
                emit(a.id, "job_started", {
                    c.actor
                }, j.materials, Object {
                    {
                        "job", j.id
                    }, {
                        "recipe", j.recipe
                    }
                }, false, j.id);
            } else {
