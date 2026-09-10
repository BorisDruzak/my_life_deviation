#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
#include <numeric>
namespace npc {
    namespace {
        Receipt public_receipt(Receipt r) {
            r.debug_reason.clear();
            r.fingerprint.clear();
            return r;
        }
        void validate_argument_types(const Json & args) {
            const std::set < std::string > strings = {
                "item", "place", "edge", "shop", "job", "recipe", "fixture", "bed", "contract", "to", "channel", "proposal", "answer",
                "mode", "obligation", "target", "operator", "question", "message", "record", "resource", "grantee", "episode", "skill",
                "tool", "container", "scope"
            };
            const std::set < std::string > integers = {
                "minutes", "expires", "version", "amount"
            };
            const std::set < std::string > booleans = {
                "open", "grant", "fabricated", "public", "delete", "forceful"
            };
            const std::set < std::string > lists = {
                "materials", "tools", "participants", "evidence", "acl"
            };
            for (const auto & kv : args.as_object()) {
                const auto k = std::string(kv.key());
                if (strings.contains(k)) static_cast < void >(decode < std::string >(kv.value()));
                if (integers.contains(k)) static_cast < void >(decode < std::int64_t >(kv.value()));
                if (booleans.contains(k)) static_cast < void >(decode < bool >(kv.value()));
                if (lists.contains(k)) static_cast < void >(decode < std::vector < std::string >>(kv.value()));
                if (k == "terms" && ! kv.value().is_object()) throw InputError("terms must be an object");
                if (k == "statement") static_cast < void >(decode < Statement >(kv.value()));
            }
        }
        void finite_json(const Json & j) {
            if (j.is_double() && ! std::isfinite(j.as_double())) throw InputError("non-finite command value");
            if (j.is_array()) for (auto & v : j.as_array()) finite_json(v);
            if (j.is_object()) for (auto & v : j.as_object()) finite_json(v.value());
        }
    }
    World::World(Config c, State s) : World(std::move(c), std::move(s), false) {} World::World(Config c, State s, bool restoring) : config_(std::move(c)),
    state_(std::move(s)) {
        if (state_.config_hash.empty()) state_.config_hash = config_.hash;
        validate_state(config_, state_, restoring);
        if (! restoring) {
            for (auto &[id, a] : state_.actors) {
                for (auto &[n, v] : a.needs) {
                    if (v.value <= v.activation) v.active = true;
                    if (v.value >= v.target) v.active = false;
                    v.critical_active = v.value <= v.critical;
                }
                if (a.health <= 0) {
                    a.alive = false;
                    a.capable = false;
                } else if (a.health <= config_.number({
                    "physical", "incapacitated_at"
                })) a.capable = false;
                if (a.fatigue >= config_.number({
                    "thresholds", "fatigue_heavy_block", "on"
                })) a.heavy_allowed = false;
            }
        }
        rebuild_local_evidence_index();
    }
    World World::from_scenario(Config c, const Json & j) {
        auto s = scenario_state(c, j);
        return World(std::move(c), std::move(s));
    }
    bool World::can_decide(const Id & id) const {
        if (phase_ != "ready" || ! state_.actors.contains(id)) throw InputError("invalid scheduler query");
        const auto & a = state_.actors.at(id);
        return a.alive && a.capable && a.active_action.empty();
    }
    Receipt World::submit(const Command & command) {
        if (phase_ != "ready") throw InputError("submit only at a ready boundary");
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
                auto & p = state_.proposals.at(pid);
                p.signatures.erase(c.actor);
                p.state = "withdrawn";
            }
            interrupt(active, "cancelled_by_participant");
        }
        if (! proposal.empty()) {
            auto & p = state_.proposals.at(proposal);
            p.signatures.erase(c.actor);
            p.state = "withdrawn";
            emit(command_key(c), "participation_withdrawn", {
                c.actor
            }, {}, Object {
                {
                    "proposal", proposal
                }
            }, true, p.root);
        }
        auto & r = state_.receipts.at(command_key(c));
        r.status = "completed";
    }
    void World::check_invariants() const {
        if (phase_ != "ready") throw InvariantError("invariants requested mid-step");
        validate_state(config_, state_, true);
    }
}
