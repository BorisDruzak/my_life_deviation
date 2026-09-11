#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        bool same_claim(const Statement & a, const Statement & b) {
            return a.subject == b.subject && a.predicate == b.predicate && canonical(a.arguments) == canonical(b.arguments) && a.valid_from == b.valid_from && a.valid_until == b.valid_until;
        }
        std::string visible_action(const std::string & a) {
            if (a == "A20") return "A04";
            return a;
        }
    }
    double evaluate_belief(const Actor & a, const Statement & s, Minute now) {
        double yes = .5, no = .5;
        for (const auto & e : a.beliefs) if (e.learned_at <= now && same_claim(e.statement, s)) {
            double p = e.half_life > 0 ? math::stale(e.confidence, e.prior, static_cast < double >(now - e.learned_at), e.half_life) : e.confidence;
            // Repeated or correlated sources cannot accumulate certainty. Max is deliberately conservative.
            if (e.statement.polarity == s.polarity) yes = std::max(yes, p);
            else no = std::max(no, p);
        }
        return yes > no ? yes : no > yes ? 1 - no : .5;
    }
    std::string truth_value(const Actor & a, const Statement & s, Minute now) {
        auto p = evaluate_belief(a, s, now);
        return p >= .75 ? "true" : p <= .25 ? "false" : "unknown";
    }
    void World::hypothesize(const Id & id, const Evidence & e) {
        if (phase_ != "ready" || ! state_.actors.contains(id)) throw InputError("invalid hypothesis boundary");
        auto & actor = state_.actors.at(id);
        if (e.kind != "hypothesis" || e.roots.empty() || e.id.empty() || e.statement.subject.empty() || e.statement.predicate.empty() || e.learned_at != state_.time || ! std::isfinite(e.confidence) || e.confidence < .5 || e.confidence > 1 || ! std::isfinite(e.severity) || e.severity < 0 || e.severity > 1) throw InputError("invalid local hypothesis");
        for (const auto & r : e.roots) {
            bool known = false;
            for (const auto & b : actor.beliefs) if (std::find(b.roots.begin(), b.roots.end(), r) != b.roots.end()) known = true;
            if (! known) throw InputError("hypothesis references unknown evidence root");
        }
        remember(id, e);
    }
    void World::remember(const Id & id, Evidence e) {
        auto & a = state_.actors.at(id);
        auto found = std::find_if(a.beliefs.begin(), a.beliefs.end(),[&](const Evidence & x) {
            return x.id == e.id;
        });
        if (found != a.beliefs.end()) {
            if (canonical(encode(* found)) != canonical(encode(e))) throw InvariantError("evidence identifier reused");
            return;
        }
        if (e.id.empty() || e.roots.empty()) throw InvariantError("evidence lacks provenance");
        std::sort(e.roots.begin(), e.roots.end());
        e.roots.erase(std::unique(e.roots.begin(), e.roots.end()), e.roots.end());
        a.beliefs.push_back(std::move(e));
        auto& prefixes = local_evidence_prefixes_.at(id);
        prefixes.push_back(local_history_next(prefixes.back(), a.beliefs.back()));
    }
    Event & World::emit(const Id & cause, const std::string & type, const std::vector < Id > & actors, const std::vector < Id > & objects,
    Json data, bool owner_only, const Id & root, Json secret) {
        Event e;
        auto count = ++ state_.event_counters[cause];
        e.uid = cause + "/event/" + std::to_string(count);
        e.sequence = state_.next_event_sequence ++;
        e.time = starting_ || phase_ == "control" ? state_.time : state_.time + 1;
        e.phase = phase_;
        e.type = type;
        e.cause = cause;
        e.root = root.empty() ? e.uid : root;
        e.participants = actors;
        e.objects = objects;
        e.data = std::move(data);
        e.private_data = std::move(secret);
        for (const auto &[id, x] : state_.actors) {
            bool participant = std::find(actors.begin(), actors.end(), id) != actors.end();
            if (owner_only && ! participant) continue;
            if (! participant) {
                if (! awake(id)) continue;
                bool visible = false;
                for (auto & who : actors) if (state_.actors.contains(who) && x.position.kind == "at" && at_place(who, x.position.place)) visible = true;
                if (actors.empty()) for (auto & item : objects) if (state_.items.contains(item) && accessible(id, item)) visible = true;
                if (! visible) continue;
                double probability = x.attention *(1 - .5 * x.impairment) *(1 - .5 * x.pain) *(x.active_action.empty() ? 1 : .5);
                if (! get < bool >(at(config_.parameters, "perception"), "perfect_observation") && math::keyed_sample(state_.seed,
                e.uid, id, "sight") >= probability) continue;
            }
            // System audit classifications, intents, account balances and private causes never enter an observer projection.
            Json public_data = e.data;
            if (has(public_data, "action_type")) public_data.as_object()["action_type"] = visible_action(str(public_data, "action_type"));
            if (! participant) {
                for (auto key : {
                    "mode", "agreement", "price", "from", "to", "amount", "count"
                }) public_data.as_object().erase(key);
            }
            e.projections[id] = Object {
                {
                    "id", e.uid
                }, {
                    "time", e.time
                }, {
                    "type", type == "unauthorized_transfer" ? "item_moved" : type
                }, {
                    "participants", encode(actors)
                }, {
                    "objects", encode(objects)
                }, {
                    "data", public_data
                }
            };
        }
        state_.ledger.push_back(std::move(e));
        return state_.ledger.back();
    }
    void World::deliver_projections(std::size_t begin) {
        // Projections were captured at occurrence, not recomputed using later positions.
        for (std::size_t n = begin; n < state_.ledger.size(); ++ n) {
            const auto e = state_.ledger[n];
            for (const auto &[id, p] : e.projections) {
                auto key = id + "/" + e.uid;
                if (! state_.delivered_evidence.insert(key).second) continue;
                Evidence b;
                b.id = key;
                b.kind = "observation";
                b.source = id;
                b.learned_at = e.time;
                b.roots = {
                    e.root
                };
                b.statement = {
                    e.participants.empty() ? id : e.participants.front(), "event", p, true, e.time, e.time
                };
                remember(id, b);
            }
        }
    }
    void World::information() {
        deliver_projections(tick_event_start_);
        for (auto &[id, m] : state_.messages) if (! m.delivered && m.deliver_at <= state_.time + 1) {
            m.delivered = true;
            for (auto & who : m.recipients) {
                state_.actors.at(who).inbox.push_back(id);
                auto& prefixes=local_inbox_prefixes_.at(who);
                prefixes.push_back(local_inbox_next(prefixes.back(),{id,m.author,m.channel,m.deliver_at,false}));
                if (m.channel == "speech") read_message(who, id);
            }
        }
    }
    void World::read_message(const Id & reader, const Id & id) {
        auto & m = state_.messages.at(id);
        if (! m.delivered || std::find(m.recipients.begin(), m.recipients.end(), reader) == m.recipients.end()) throw InvariantError("reading undelivered message");
        if (! m.readers.insert(reader).second) return;
        Evidence e;
        e.id = id + "/read/" + reader;
        e.kind = "claim";
        e.learned_at = state_.time + 1;
        e.source = m.author;
        e.roots = m.roots.empty() ? std::vector < Id > {
            id
        }
        : m.roots;
        e.confidence = .7;
        if (text(m.content, "type") == "claim" && has(m.content, "statement")) {
            e.statement = decode < Statement >(at(m.content, "statement"));
            auto & r = state_.actors.at(reader).relations[m.author];
            e.confidence = .5 + .45 * r.trust;
        } else {
            e.statement = {
                m.author, text(m.content, "type", "message"), m.content, true, m.sent, m.sent
            };
            e.confidence = 1;
        }
        if (text(m.content, "type") == "proposal_update") {
            const auto answer = text(m.content, "answer");
            if (reader == text(m.content, "author") &&(answer == "accept" || answer == "decline")) ++ state_.actors.at(reader).outcome_counts[m.author + "/" + str(at(m.content,
            "terms"), "kind") + "/" +(answer == "accept" ? "accepted" : "declined")];
        }
        if (text(m.content, "type") == "demand" && get_or < bool >(m.content, "forceful", false)) appraise(reader, m.author,
        "demand/" + m.author + "/" + reader + "/" + text(m.content, "episode"), "demand");
        if (text(m.content, "type") == "claim" && truth_value(state_.actors.at(reader), e.statement, state_.time + 1) == "false") appraise(reader,
        m.author, e.roots.front(), "false_statement");
        remember(reader, e);
    }
    void World::send_message(const Action & a, const Json & content, const std::vector < Id > & recipients, const std::vector < Id > & roots) {
        Message m;
        m.id = a.id + "/message";
        m.author = a.actor;
        m.recipients = recipients;
        m.content = content;
        m.roots = roots;
        m.channel = text(a.args, "channel", "speech");
        m.sent = state_.time + 1;
        m.deliver_at = m.sent +(m.channel == "digital" ? 1 : 0);
        // Speech only reaches awake participants who are present at this exact commit.
        if (m.channel == "speech") m.recipients.erase(std::remove_if(m.recipients.begin(), m.recipients.end(),[&](const Id & id) {
            return ! channel_available(a.actor, id, "speech");
        }), m.recipients.end());
        state_.messages.emplace(m.id, m);
        emit(a.id, "message_sent", {
            a.actor
        }, {}, Object {
            {
                "message", m.id
            }, {
                "channel", m.channel
            }
        }, true, a.proposal);
    }
    void World::appraise(const Id & observer, const Id & target, const Id & root, const std::string & type) {
        if (! state_.actors.contains(observer) || ! state_.actors.contains(target) || observer == target) return;
        auto old = state_.actors.at(observer).relations.find(target);
        auto trust = old == state_.actors.at(observer).relations.end() ? .5 : old->second.trust;
        Appraisal a;
        a.observer = observer;
        a.target = target;
        a.root = root;
        a.type = type;
        const auto & effects = at(at(config_.parameters, "relationships"), "effects");
        if (has(effects, type)) {
            const auto & v = at(effects, type);
            a.trust_delta = get_or < double >(v, "trust_delta", 0) + get_or < double >(v, "trust_gap_positive", 0) *(1 - trust);
            a.tension_delta = get_or < double >(v, "tension_delta", 0);
        }
        if (type == "breach") a.anger_delta = config_.number({
            "stimuli", "other_promise_breach", "anger"
        });
        if (type == "demand") a.anger_delta = config_.number({
            "stimuli", "new_demand_episode", "anger_gap_trust_multiplier"
        }) *(1 - trust);
        pending_appraisals_.push_back(a);
    }
    void World::reactions() {
        std::map < Id, Appraisal > effects;
        for (auto a : pending_appraisals_) {
            // Reclassification replaces the old appraisal for the same cause instead of punishing twice.
            auto family =(a.type == "false_statement" || a.type == "inferred_deliberate_lie") ? "statement" : a.type;
            auto key = a.observer + "|" + a.target + "|" + a.root + "|" + family;
            if (a.type == "help" || a.type == "return") {
                auto pair = a.observer + "|" + a.target + "|positive";
                auto it = state_.positive_cooldowns.find(pair);
                if (! state_.appraisal_history.contains(key) && it != state_.positive_cooldowns.end() && state_.time + 1 - it->second < config_.number({
                    "relationships", "positive_pair_cooldown_minutes"
                })) continue;
                if (! state_.appraisal_history.contains(key)) state_.positive_cooldowns[pair] = state_.time + 1;
            }
            Appraisal delta = a;
            if (auto it = state_.appraisal_history.find(key); it != state_.appraisal_history.end()) {
                if (it->second.type == a.type || it->second.type == "inferred_deliberate_lie") continue;
                delta.trust_delta -= it->second.trust_delta;
                delta.tension_delta -= it->second.tension_delta;
                delta.stress_delta -= it->second.stress_delta;
                delta.anger_delta -= it->second.anger_delta;
                delta.esteem_delta -= it->second.esteem_delta;
            }
            state_.appraisal_history[key] = a;
            auto & sum = effects[a.observer + "|" + a.target];
            sum.observer = a.observer;
            sum.target = a.target;
            sum.trust_delta += delta.trust_delta;
            sum.tension_delta += delta.tension_delta;
            sum.stress_delta += delta.stress_delta;
            sum.anger_delta += delta.anger_delta;
            sum.esteem_delta += delta.esteem_delta;
        }
        // Accumulate first, clip once per target and once per observer.
        std::map < Id, std::pair < double, double >> emotional;
        for (const auto &[key, a] : effects) {
            auto & x = state_.actors.at(a.observer);
            if (! a.target.empty()) {
                auto & r = x.relations[a.target];
                r.trust = math::clip(r.trust + a.trust_delta);
                r.tension = math::clip(r.tension + a.tension_delta);
                x.anger[a.target] = math::clip(x.anger[a.target] + a.anger_delta);
            }
            emotional[a.observer].first += a.stress_delta;
            emotional[a.observer].second += a.esteem_delta;
        }
        for (auto &[id, x] : state_.actors) {
            x.stress = math::clip(x.stress + emotional[id].first);
            x.selfesteem = math::clip(x.selfesteem + emotional[id].second);
            double fear = 0;
            for (const auto & b : x.beliefs) if (b.kind == "hypothesis" && b.statement.predicate == "threat" &&(b.statement.valid_until < 0 || b.statement.valid_until >= state_.time + 1)) {
                double p = b.half_life > 0 ? math::stale(b.confidence, b.prior, static_cast < double >(state_.time + 1 - b.learned_at),
                b.half_life) : b.confidence;
                fear = std::max(fear, p * b.severity);
            }
            x.fear = math::clip(fear);
            x.wellbeing = math::ema(x.wellbeing, static_cast < double >(state_.accounts.at(x.account)), config_.number({
                "money", "wellbeing_half_life_minutes"
            }));
        }
    }
    Json World::view(const Id & id) const {
        if (phase_ != "ready" || ! state_.actors.contains(id)) throw InputError("invalid view request");
        const auto & a = state_.actors.at(id);
        Json self = encode(a);
        self.as_object().erase("inbox");
        // Message bodies have a separate read boundary.
        Object inventory;
        for (const auto &[key, i] : state_.items) if (holder(key) == id && accessible(id, key)) inventory[key] = Object {
            {
                "id", key
            }, {
                "type", i.type
            }, {
                "owner", i.owner
            }, {
                "condition", i.condition
            }, {
                "remaining_units", i.remaining_units
            }, {
                "version", i.version
            }
        };
        Array inbox;
        for (const auto & mid : a.inbox) {
            const auto & m = state_.messages.at(mid);
            Object entry {
                {
                    "id", mid
                }, {
                    "from", m.author
                }, {
                    "channel", m.channel
                }, {
                    "read", m.readers.contains(id)
                }
            };
            if (m.readers.contains(id)) entry["content"] = m.content;
            inbox.push_back(entry);
        }
        Array receipts;
        for (const auto &[key, r] : state_.receipts) if (r.actor == id) receipts.push_back(Object {
            {
                "id", key
            }, {
                "status", r.status
            }, {
                "action", r.action
            }, {
                "reason", r.public_reason
            }, {
                "received", r.received
            }, {
                "request", r.request
            }
        });
        Array jobs;
        for (const auto &[key, j] : state_.jobs) if (j.creator == id) jobs.push_back(encode(j));
        Array employment;
        for (const auto &[key, c] : state_.contracts) if (c.worker == id) employment.push_back(encode(c));
        return Object {
            {
                "own_jobs", jobs
            }, {
                "employment", employment
            }, {
                "version", std::string(rules_version)
            }, {
                "time", state_.time
            }, {
                "self", self
            }, {
                "money", state_.accounts.at(a.account)
            }, {
                "inventory", inventory
            }, {
                "inbox", inbox
            }, {
                "receipts", receipts
            }
        };
    }
}
