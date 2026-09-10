        if (! state_.actors.contains(command.actor) || command.sequence < 1 || command.sequence > max_minute || ! config_.actions.contains(command.type) || ! command.args.is_object()) throw InputError("invalid command envelope");
        finite_json(command.args);
        validate_argument_types(command.args);
        if (canonical(command.args).size() > 65536 || command.intent.size() > 256) throw InputError("command too large");
        for (auto &[id, v] : command.expected_versions) if (v < 1) throw InputError("invalid expected version");
        const auto key = command_key(command), fingerprint = math::sha256(canonical(encode(command)));
        if (auto it = state_.receipts.find(key); it != state_.receipts.end()) {
            if (it->second.fingerprint != fingerprint) throw InputError("command ID reused with different content");
            return public_receipt(it->second);
        }
        for (auto & pending : state_.pending) if (pending.actor == command.actor &&((pending.type == "A22") ==(command.type == "A22"))) throw InputError("only one ordinary and one control command per actor per boundary");
        Receipt r;
        r.request = encode(command);
        r.actor = command.actor;
        r.fingerprint = fingerprint;
        r.received = state_.time;
        state_.receipts[key] = r;
        state_.pending.push_back(command);
        return public_receipt(r);
    }
    void World::reject(const Command & c, const std::string & debug) {
        auto & r = state_.receipts.at(command_key(c));
        r.status = "rejected";
        r.public_reason = "conditions_not_met";
        r.debug_reason = debug;
        emit(command_key(c), "command_rejected", {
            c.actor
        }, {}, Object {
            {
                "reason", "conditions_not_met"
            }
        }, true, {}, Object {
            {
                "detail", debug
            }
        });
    }
    TickReport World::advance() {
        if (phase_ != "ready") throw InvariantError("cannot advance a failed/in-progress world; restore a checkpoint");
        if (state_.time >= max_minute - 1) throw InputError("world time limit reached");
        tick_event_start_ = state_.ledger.size();
        pending_appraisals_.clear();
        auto batch = std::move(state_.pending);
        state_.pending.clear();
        phase_ = "control";
        for (const auto & c : batch) if (c.type == "A22") {
            try {
                control(c);
            } catch (const InputError & e) {
                reject(c, e.what());
            }
        }
        std::vector < Id > ids;
        for (const auto &[id, a] : state_.actors) ids.push_back(id);
        auto rank =[&](const Id & id) {
            auto pos = static_cast < std::size_t >(std::lower_bound(ids.begin(), ids.end(), id) - ids.begin());
            return(pos + ids.size() - static_cast < std::size_t >(state_.time % static_cast < Minute >(ids.size()))) % ids.size();
        };
        std::stable_sort(batch.begin(), batch.end(),[&](const auto & a, const auto & b) {
            auto priority =[&](const Command & x) {
                const auto p = text(x.args, "proposal");
                auto it = state_.proposals.find(p);
                return it != state_.proposals.end() && it->second.state == "accepted" ? it->second.created : max_minute;
            };
            if (priority(a) != priority(b)) return priority(a) < priority(b);
            return std::pair {
                rank(a.actor), a.sequence
            }
            < std::pair {
                rank(b.actor), b.sequence
            };
        });
        phase_ = "start";
        starting_ = true;
        for (const auto & c : batch) if (c.type != "A22" && state_.receipts.at(command_key(c)).status == "queued") {
            try {
                if (! try_start(c, batch)) reject(c, "preconditions rejected");
            } catch (const InputError & e) {
                reject(c, e.what());
            }
        }
