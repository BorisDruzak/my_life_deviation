#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        void require(bool b, const char * message) {
            if (! b) throw InputError(message);
        }
    }
    void World::validate_terms(const Id & author, const std::set < Id > & participants, const Json & t) const {
        require(t.is_object() && participants.contains(author) && participants.size() >= 2 && participants.size() <= 16,
        "invalid participants/terms");
        for (const auto & id : participants) require(state_.actors.contains(id), "unknown participant");
        const auto kind = str(t, "kind");
        std::set < std::string > allowed {
            "kind", "important"
        };
        if (kind == "loan_item" || kind == "loan_money") {
            allowed.insert({
                "lender", "borrower", "item", "amount", "due"
            });
            auto lender = str(t, "lender"), borrower = str(t, "borrower");
            require(lender != borrower && participants == std::set < Id > {
                lender, borrower
            }, "invalid loan parties");
            auto due = get < Minute >(t, "due");
            require(due > state_.time + 1 && due <= state_.time + 43200, "invalid loan deadline");
            if (kind == "loan_item") require(! str(t, "item").empty(), "requested item missing");
            else {
                auto amount = get < Money >(t, "amount");
                require(amount > 0 && amount <= max_money, "invalid amount");
            }
        } else if (kind == "joint" || kind == "private") {
            allowed.insert({
                "place", "start", "end", "minutes", "topic", "attention"
            });
            auto start = get < Minute >(t, "start"), end = get < Minute >(t, "end"), d = get < Minute >(t, "minutes");
            require(state_.places.contains(str(t, "place")), "unknown meeting place");
            require(d >= 1 && d <= 120 && start >= state_.time && end > start && end - start >= d && end <= state_.time + 10080,
            "invalid meeting interval");
            if (has(t, "attention")) {
                require(at(t, "attention").is_object(), "attention must be a map");
                for (const auto & v : at(t, "attention").as_object()) {
                    require(participants.contains(std::string(v.key())), "foreign attention participant");
                    auto f = decode < double >(v.value());
                    require(f >= 0 && f <= 1, "attention out of range");
                }
            }
            if (kind == "private") require(participants.size() == 2 && config_.enabled(state_.profile, "intimacy"), "private module unavailable");
        } else if (kind == "repair") {
            allowed.insert({
                "debtor", "creditor", "target", "due"
            });
            auto d = str(t, "debtor"), c = str(t, "creditor");
            require(d != c && participants == std::set < Id > {
                d, c
            }, "invalid task parties");
            require(state_.items.contains(str(t, "target")), "unknown task target");
            require(get < Minute >(t, "due") > state_.time + 1 && get < Minute >(t, "due") <= state_.time + 43200, "invalid task deadline");
        } else if (kind == "reschedule") {
            allowed.insert({
                "obligation", "due"
            });
            auto id = str(t, "obligation");
            require(state_.obligations.contains(id), "unknown obligation");
            const auto & o = state_.obligations.at(id);
            require(participants == std::set < Id > {
                o.debtor, o.creditor
            }
            &&(o.status == "pending" || o.status == "breached"), "invalid rescheduling parties/status");
            require(get < Minute >(t, "due") > state_.time + 1 && get < Minute >(t, "due") <= state_.time + 43200, "invalid rescheduled deadline");
        } else throw InputError("unknown agreement kind");
        for (const auto & kv : t.as_object()) require(allowed.contains(std::string(kv.key())), "unknown agreement parameter");
        if (has(t, "important")) static_cast < void >(get < bool >(t, "important"));
    }
    bool World::known_proposal(const Id & who, const Id & id, std::int64_t version) const {
        auto it = state_.proposals.find(id);
        if (it == state_.proposals.end() || ! it->second.participants.contains(who)) return false;
        for (const auto & b : state_.actors.at(who).beliefs) if ((b.statement.predicate == "proposal" || b.statement.predicate == "proposal_update") && text(b.statement.arguments,
        "proposal") == id && get_or < std::int64_t >(b.statement.arguments, "version", 0) == version) return true;
        return false;
    }
    void World::social_commit(const Action & a) {
        auto & actor = state_.actors.at(a.actor);
        auto roots =[&]() {
            std::vector < Id > out;
            for (auto & id : get_or < std::vector < Id >>(a.args, "evidence", {})) for (const auto & b : actor.beliefs) if (b.id == id) out.insert(out.end(),
            b.roots.begin(), b.roots.end());
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            if (out.empty() || get_or < bool >(a.args, "fabricated", false)) out = {
                a.id + "/assertion"
            };
            return out;
        };
        auto proposal_message =[&](Proposal & p, const std::string & type) {
            Json content = Object {
                {
                    "type", type
                }, {
                    "proposal", p.id
                }, {
                    "version", p.version
                }, {
                    "state", p.state
                }, {
                    "author", p.author
                }, {
                    "participants", encode(p.participants)
                }, {
                    "signatures", encode(p.signatures)
                }, {
                    "terms", p.terms
                }, {
                    "expires", p.expires
                }
            };
            if (type == "proposal_update") content.as_object()["answer"] = text(a.args, "answer");
            Evidence e;
            e.id = a.id + "/own_agreement";
            e.source = a.actor;
            e.learned_at = state_.time + 1;
            e.roots = {
                p.root
            };
            e.statement = {
                a.actor, type, content, true, state_.time + 1, state_.time + 1
            };
            remember(a.actor, e);
            std::vector < Id > others;
            for (auto & who : p.participants) if (who != a.actor) others.push_back(who);
            send_message(a, content, others, {
                p.root
            });
        };
        if (a.type == "A11" || a.type == "A14") {
            Proposal p;
            p.id = a.id + "/proposal";
            p.root = p.id;
            p.author = a.actor;
            p.created = state_.time + 1;
            p.expires = get_or < Minute >(a.args, "expires", a.started + 120);
            p.terms = at(a.args, "terms");
            p.participants = a.type == "A11" ? std::set < Id > {
                a.actor, str(a.args, "to")
            }
            : get < std::set < Id >>(a.args, "participants");
            p.participants.insert(a.actor);
            p.signatures = {
                a.actor
            };
            state_.proposals.emplace(p.id, p);
            auto & stored = state_.proposals.at(p.id);
            proposal_message(stored, "proposal");
            emit(a.id, "proposal_created", {
                a.actor
            }, {}, Object {
                {
                    "proposal", p.id
                }, {
                    "version", 1
                }
            }, true, p.root);
        } else if (a.type == "A12") {
            auto & p = state_.proposals.at(a.proposal);
            auto answer = str(a.args, "answer");
            if (answer == "accept") {
                p.signatures.insert(a.actor);
                if (p.signatures == p.participants) p.state = "accepted";
            } else if (answer == "decline") p.state = "declined";
            else if (answer == "counter") {
                p.prior_versions.push_back(encode(p.terms));
                ++ p.version;
                p.terms = at(a.args, "terms");
                p.signatures = {
                    a.actor
                };
                p.expires = get_or < Minute >(a.args, "expires", p.expires);
                p.state = "countered";
            }
            proposal_message(p, "proposal_update");
            emit(a.id, "proposal_answered", {
                a.actor
            }, {}, Object {
                {
                    "proposal", p.id
                }, {
                    "version", p.version
                }, {
                    "answer", answer
                }
            }, true, p.root);
            if (p.state == "accepted") {
                auto kind = str(p.terms, "kind");
                if (kind == "repair") {
                    Obligation o;
                    o.id = p.id + "/obligation";
                    o.root = p.root;
                    o.debtor = str(p.terms, "debtor");
                    o.creditor = str(p.terms, "creditor");
                    o.kind = "repair";
                    o.target = str(p.terms, "target");
                    o.due = get < Minute >(p.terms, "due");
                    o.important = get_or < bool >(p.terms, "important", false);
                    o.acknowledged = {
                        a.actor
                    };
                    state_.obligations.emplace(o.id, o);
                    p.executed = true;
                } else if (kind == "reschedule") {
                    auto & old = state_.obligations.at(str(p.terms, "obligation"));
                    old.status = "renegotiated";
                    Obligation n = old;
                    n.id = p.id + "/obligation";
                    n.root = p.root;
                    n.status = "pending";
                    n.due = get < Minute >(p.terms, "due");
                    n.fulfilled_at = - 1;
                    n.breached_at = - 1;
                    n.acknowledged = {
                        a.actor
                    };
                    state_.obligations.emplace(n.id, n);
                    p.executed = true;
                }
                emit(a.id, "agreement_confirmed", {
                    a.actor
                }, {}, Object {
                    {
                        "proposal", p.id
                    }, {
                        "version", p.version
                    }
                }, true, p.root);
            }
        } else if (a.type == "A17") {
            Array selected;
            for (auto & id : get < std::vector < Id >>(a.args, "evidence")) for (const auto & b : actor.beliefs) if (b.id == id) selected.push_back(encode(b.statement));
            send_message(a, Object {
                {
                    "type", "question"
                }, {
                    "operator", str(a.args, "operator")
                }, {
                    "statements", selected
                }
            }, {
                str(a.args, "to")
            }, roots());
        } else if (a.type == "A18" || a.type == "A19") {
            auto mode = text(a.args, "mode", "truth");
            Json content = Object {
                {
                    "type", mode == "refuse" ? "refusal" : "claim"
                }, {
                    "question", text(a.args, "question")
                }
            };
            if (mode != "refuse") content.as_object()["statement"] = at(a.args, "statement");
            // Deliberateness is a debug annotation, never a recipient-visible flag.
            std::string assessed = "not_asserted";
            if (mode != "refuse") {
                auto st = decode < Statement >(at(a.args, "statement"));
                assessed = truth_value(actor, st, state_.time + 1);
            }
            emit(a.id, "utterance", {
                a.actor
            }, {}, Object {
                {
                    "to", str(a.args, "to")
                }
            }, true, {}, Object {
                {
                    "requested_mode", mode
                }, {
                    "speaker_belief", assessed
                }, {
                    "fabricated", get_or < bool >(a.args, "fabricated", false)
                }
            });
            send_message(a, content, {
                str(a.args, "to")
            }, roots());
            Evidence e;
            e.id = a.id + "/own_utterance";
            e.source = a.actor;
            e.kind = "observation";
            e.learned_at = state_.time + 1;
            e.roots = {
                e.id
            };
            e.statement = {
                a.actor, "said", Object {
                    {
                        "to", str(a.args, "to")
                    }, {
                        "content", content
                    }
                }, true, state_.time + 1, state_.time + 1
            };
            remember(a.actor, e);
        } else if (a.type == "A24") {
            if (has(a.args, "message")) read_message(a.actor, str(a.args, "message"));
            else {
                auto rid = str(a.args, "record");
                auto v = decode < RecordVersion >(a.captured);
                Evidence e;
                e.id = a.id + "/record";
                e.kind = "record";
                e.source = state_.records.at(rid).author;
                e.learned_at = state_.time + 1;
                e.confidence = .8;
                e.roots = get_or < std::vector < Id >>(v.content, "roots", {
                    rid + "/v" + std::to_string(v.version)
                });
                if (has(v.content, "statement")) e.statement = decode < Statement >(at(v.content, "statement"));
                else e.statement = {
                    rid, "record", v.content, true, v.time, v.time
                };
                remember(a.actor, e);
            }
        } else if (a.type == "A27") {
            auto to = str(a.args, "to"), oid = str(a.args, "obligation");
            const auto & o = state_.obligations.at(oid);
            send_message(a, Object {
                {
                    "type", "demand"
                }, {
                    "obligation", oid
                }, {
                    "due", o.due
                }, {
                    "forceful", get_or < bool >(a.args, "forceful", false)
                }, {
                    "episode", str(a.args, "episode")
                }
            }, {
                to
            }, {
                o.root
            });
        } else if (a.type == "A28") {
            Record r;
            r.id = a.id + "/record";
            r.author = a.actor;
            RecordVersion v;
            v.time = state_.time + 1;
            v.acl = get_or < std::set < Id >>(a.args, "acl", {});
            v.acl.insert(a.actor);
            v.public_read = get_or < bool >(a.args, "public", false);
            v.content = Object {
                {
                    "statement", at(a.args, "statement")
                }, {
                    "roots", encode(roots())
                }
            };
            r.versions.push_back(v);
            state_.records.emplace(r.id, r);
            Evidence e;
            e.id = a.id + "/publication";
            e.source = a.actor;
            e.learned_at = state_.time + 1;
            e.roots = {
                e.id
            };
            e.statement = {
                r.id, "published", Object {
                    {
                        "record", r.id
                    }, {
                        "version", 1
                    }
                }, true, state_.time + 1, state_.time + 1
            };
            remember(a.actor, e);
            emit(a.id, "record_created", {
                a.actor
            }, {}, Object {
                {
                    "record", r.id
                }, {
                    "version", 1
                }
            }, true);
        } else if (a.type == "A29") {
            auto & r = state_.records.at(str(a.args, "record"));
            RecordVersion v = r.versions.back();
            ++ v.version;
            v.time = state_.time + 1;
            v.deleted = get_or < bool >(a.args, "delete", false);
            if (! v.deleted) {
                v.content = Object {
                    {
                        "statement", at(a.args, "statement")
                    }, {
                        "roots", Array {
                            r.id + "/v" + std::to_string(v.version)
                        }
                    }
                };
                if (has(a.args, "acl")) v.acl = get < std::set < Id >>(a.args, "acl");
                v.acl.insert(a.actor);
                if (has(a.args, "public")) v.public_read = get < bool >(a.args, "public");
            }
            r.versions.push_back(v);
            emit(a.id, "record_revised", {
                a.actor
            }, {}, Object {
                {
                    "record", r.id
                }, {
                    "version", v.version
                }, {
                    "deleted", v.deleted
                }
            }, true);
        }
    }
}
