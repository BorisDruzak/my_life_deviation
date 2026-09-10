        starting_ = false;
        // Any permissions revoked at the last boundary apply before the next minute.
        std::vector < Id > broken;
        for (const auto &[id, a] : state_.actions) if (! precommit(a)) broken.push_back(id);
        for (auto & id : broken) interrupt(id, "invariant_changed");
        phase_ = "interval";
        continuous();
        phase_ = "physical";
        physical_limits();
        phase_ = "completion";
        std::vector < Id > due;
        for (const auto &[id, a] : state_.actions) if (a.due == state_.time + 1) due.push_back(id);
        for (const auto & id : due) if (state_.actions.contains(id)) {
            if (precommit(state_.actions.at(id))) commit(id);
            else interrupt(id, "commit_conditions_changed");
        }
        phase_ = "clock";
        clocks();
        finish_proposals_and_obligations();
        physical_limits();
        phase_ = "information";
        information();
        phase_ = "reaction";
        reactions();
        phase_ = "signals";
        const auto signals_begin = state_.ledger.size();
        thresholds();
        deliver_projections(signals_begin);
        ++ state_.time;
        phase_ = "ready";
        return {
            state_.time, state_.ledger.size() - tick_event_start_
        };
    }
    void World::run(Minute n) {
        if (n < 0 || n > 10000000) throw InputError("run length out of range");
        for (Minute i = 0; i < n; ++ i) advance();
    }
    void World::release(const Id & id) {
        auto it = state_.actions.find(id);
        if (it == state_.actions.end()) return;
        for (auto & who : it->second.participants) if (state_.actors.at(who).active_action == id) state_.actors.at(who).active_action.clear();
        for (auto r = state_.reservations.begin(); r != state_.reservations.end();) if (r->second.action == id) r = state_.reservations.erase(r);
        else ++ r;
        state_.actions.erase(it);
    }
    void World::interrupt(const Id & id, const std::string & reason, bool physical) {
        auto it = state_.actions.find(id);
        if (it == state_.actions.end()) return;
        const Action a = it->second;
        if (a.type == "A02") {
            auto & pos = state_.actors.at(a.actor).position;
            if (! physical) throw InvariantError("movement cannot be voluntarily interrupted in an edge");
            pos.kind = "stranded";
            pos.place.clear();
        }
        if (! a.job.empty()) {
            auto & j = state_.jobs.at(a.job);
            j.active_action.clear();
            j.status = "paused";
        }
        for (auto & k : a.commands) {
            auto & r = state_.receipts.at(k);
            r.status = "interrupted";
            r.public_reason = reason;
        }
        emit(a.id, "action_interrupted", a.participants, {}, Object {
            {
                "action_type", a.type
            }, {
                "reason", reason
            }
        }, false, {}, Object {
            {
                "intent", a.intent
            }
        });
        release(id);
    }
    void World::control(const Command & c) {
        auto & a = state_.actors.at(c.actor);
        auto proposal = text(c.args, "proposal");
        if (! proposal.empty()) {
            auto it = state_.proposals.find(proposal);
            if (it == state_.proposals.end() || ! it->second.participants.contains(c.actor)) throw InputError("not a participant");
        }
        if (! a.active_action.empty()) {
            const auto active = a.active_action;
            if (state_.actions.at(active).type == "A02") throw InputError("wait until the end of the edge");
            if (! proposal.empty() && state_.actions.at(active).proposal != proposal) throw InputError("different active agreement");
            // Conservative group policy: stop the whole session; remaining members must agree again.
            auto pid = state_.actions.at(active).proposal;
            if (! pid.empty()) {
