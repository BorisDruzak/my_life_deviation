#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    bool World::precommit(const Action & a) const {
        for (auto & who : a.participants) {
            const auto & x = state_.actors.at(who);
            if (! x.alive || ! x.capable || x.active_action != a.id) return false;
        }
        const auto & actor = state_.actors.at(a.actor);
        const auto & id = a.type;
        auto usable =[&](const Id & item) {
            return accessible(a.actor, item) && permitted(a.actor, item);
        };
        if (id == "A02") return actor.position.kind == "transit";
        if (id == "A06") {
            const auto & sh = state_.shops.at(str(a.args, "shop"));
            auto it = state_.items.find(str(a.args, "item"));
            if (it == state_.items.end()) return false;
            const auto & i = it->second;
            return at_place(a.actor, sh.place) && open_interval(sh.place, state_.time, a.due) && sh.stock.contains(i.id) && i.owner == text(a.captured,
            "owner") && i.placement.kind == "at" && i.placement.ref == sh.place && available(actor.account, a.id) >= a.price && state_.accounts.at(sh.account) <= max_money - a.price && carried_mass(a.actor) + item_mass(i.id) <= actor.capacity_kg + 1e-9;
        }
        if (id == "A07") {
            const auto & i = state_.items.at(str(a.args, "item"));
            return(i.remaining_units == 0 && i.placement.kind == "tombstone") ||(usable(i.id) && holder(i.id) == a.actor);
        }
        if (id == "A04" || id == "A20") {
            auto item = str(a.args, "item");
            return accessible(a.actor, item) &&(id == "A20" ? ! permitted(a.actor, item) : permitted(a.actor, item)) && carried_mass(a.actor) +(holder(item) == a.actor ? 0 : item_mass(item)) <= actor.capacity_kg + 1e-9;
        }
        if (id == "A05") {
            auto item = str(a.args, "item");
            if (! accessible(a.actor, item) || holder(item) != a.actor) return false;
            if (has(a.args, "container")) return usable(str(a.args, "container")) && state_.items.at(str(a.args, "container")).open;
            return true;
        }
        if (id == "A09") return usable(str(a.args, "bed"));
        if (id == "A10") {
            const auto & ct = state_.contracts.at(str(a.args, "contract"));
            return actor.heavy_allowed && usable(ct.workplace);
        }
        if (id == "A08" || id == "A21") {
            const auto & j = state_.jobs.at(a.job);
            if (! at_place(a.actor, j.place) || ! location_access(a.actor, j.place)) return false;
            if (id == "A21" && ! actor.heavy_allowed) return false;
            if (! j.fixture.empty() && ! usable(j.fixture)) return false;
            if (! j.target.empty() && ! usable(j.target)) return false;
            for (auto & t : j.tools) if (! usable(t) || state_.items.at(t).condition < get_or < double >(config_.item_type(state_.items.at(t).type),
            "required_condition", 0)) return false;
            for (auto & m : j.materials) if (state_.items.at(m).placement.kind != "escrow" || state_.items.at(m).placement.ref != j.id || ! permitted(a.actor,
            m)) return false;
        }
        if (id == "A11" || id == "A17" || id == "A18" || id == "A19" || id == "A27") return channel_available(a.actor, str(a.args,
        "to"), text(a.args, "channel", "speech"));
        if (id == "A14") {
            for (auto & who : get < std::set < Id >>(a.args, "participants")) if (who != a.actor && ! channel_available(a.actor,
            who, text(a.args, "channel", "speech"))) return false;
        }
        if (id == "A12") {
            const auto & p = state_.proposals.at(a.proposal);
            return p.version == a.proposal_version &&(p.state == "offered" || p.state == "countered" || p.state == "accepted") && ! p.executed && p.expires >= state_.time + 1;
        }
        if (id == "A13") {
            auto mode = str(a.args, "mode"), to = str(a.args, "to");
            if (has(a.args, "item")) {
                auto item = str(a.args, "item");
                if (! accessible(a.actor, item) || holder(item) != a.actor || ! channel_available(a.actor, to, "speech") || carried_mass(to) + item_mass(item) > state_.actors.at(to).capacity_kg + 1e-9) return false;
                if ((mode == "gift" || mode == "loan_item") && state_.items.at(item).owner != a.actor) return false;
            }
            if (a.price > 0 &&(available(actor.account, a.id) < a.price || state_.accounts.at(state_.actors.at(to).account) > max_money - a.price)) return false;
            if (! a.proposal.empty()) {
                const auto & p = state_.proposals.at(a.proposal);
                if (p.state != "accepted" || p.version != a.proposal_version || p.executed) return false;
            }
            if (has(a.args, "obligation")) {
                const auto & o = state_.obligations.at(str(a.args, "obligation"));
                if (o.status != "pending" && o.status != "breached") return false;
                if (mode == "repay" && a.price > o.remaining) return false;
            }
        }
        if (id == "A15" || id == "A32") {
            const auto & p = state_.proposals.at(a.proposal);
            if (p.state != "accepted" || p.version != a.proposal_version || p.signatures != p.participants) return false;
            auto place = str(p.terms, "place");
            for (auto & who : a.participants) if (! at_place(who, place) || ! location_access(who, place)) return false;
            if (id == "A32") for (const auto &[who, x] : state_.actors) if (! p.participants.contains(who) && x.alive && at_place(who,
            place)) return false;
        }
        if (id == "A16") {
            auto item = str(a.args, "item");
            if (! usable(item)) return false;
            if (text(config_.item_type(state_.items.at(item).type), "category") == "sport" && ! actor.heavy_allowed) return false;
        }
        if (id == "A24" && has(a.args, "record")) {
            auto & r = state_.records.at(str(a.args, "record"));
            const auto & v = r.versions.back();
            if (! v.deleted && ! record_access(a.actor, v)) return false;
            return record_access(a.actor, decode < RecordVersion >(a.captured));
        }
        if (id == "A25") {
            auto r = str(a.args, "resource");
            auto it = state_.permissions.find(r);
            auto version = it == state_.permissions.end() ? 1 : it->second.version;
            auto authority = it == state_.permissions.end() ?(state_.items.contains(r) ? state_.items.at(r).owner : state_.places.at(r).owner) : it->second.authority;
            return authority == a.actor && version == get < std::int64_t >(a.args, "version");
        }
        if (id == "A26") return usable(str(a.args, "item"));
        if (id == "A29") {
            const auto & r = state_.records.at(str(a.args, "record"));
            return r.author == a.actor && r.versions.back().version == get < std::int64_t >(a.args, "version");
        }
        if (id == "A30") return usable(str(a.args, "tool"));
        if (id == "A31") for (const auto &[who, x] : state_.actors) if (who != a.actor && x.alive && at_place(who, actor.position.place)) return false;
        return true;
    }
    void World::move_item(const Id & id, const Placement & where, const Id & owner, const Action & a, const std::string & type) {
        auto & i = state_.items.at(id);
        auto before = encode(i.placement);
        i.placement = where;
        if (! owner.empty()) i.owner = owner;
        ++ i.version;
        for (auto &[sid, sh] : state_.shops) if (sh.stock.contains(id) && !(where.kind == "at" && where.ref == sh.place &&(i.owner == sid || i.owner == sh.account))) sh.stock.erase(id);
        auto actors = a.participants;
        if (where.kind == "inventory" && std::find(actors.begin(), actors.end(), where.ref) == actors.end()) actors.push_back(where.ref);
        emit(a.id, type, actors, {
            id
        }, Object {
            {
                "item", id
            }, {
                "holder", holder(id)
            }, {
                "place", item_place(id)
            }, {
                "mode", text(a.args, "mode")
            }, {
                "agreement", a.proposal
            }
        }, false, a.proposal, Object {
            {
                "owner", i.owner
            }, {
                "old_placement", before
            }, {
                "new_placement", encode(where)
            }
        });
    }
    void World::transfer_money(const Id & from, const Id & to, Money amount, const Action & a) {
        if (amount < 0 || from == to || ! state_.accounts.contains(from) || ! state_.accounts.contains(to) || state_.accounts.at(from) < amount || state_.accounts.at(to) > max_money - amount) throw InvariantError("invalid money transaction after validation");
        state_.accounts.at(from) -= amount;
        state_.accounts.at(to) += amount;
        emit(a.id, "money_transferred", a.participants, {}, Object {
            {
                "from", from
            }, {
                "to", to
            }, {
                "amount", amount
            }
        }, true, a.proposal);
    }
    void World::satisfy_obligation(Obligation & o, const Id & cause) {
        if (o.status == "fulfilled" || o.status == "waived" || o.status == "renegotiated") return;
        o.status = "fulfilled";
        o.fulfilled_at = state_.time + 1;
        if (o.kind == "repay_money") o.remaining = 0;
        emit(cause, "obligation_fulfilled", {
            o.debtor
        }, {}, Object {
            {
                "obligation", o.id
            }, {
                "creditor", o.creditor
            }, {
                "debtor", o.debtor
            }, {
                "important", o.important
            }, {
                "late", o.breached_at >= 0
            }
        }, true, o.root);
        if (o.important) {
            Appraisal r;
            r.observer = o.debtor;
            r.root = o.root;
            r.type = "own_success";
            r.esteem_delta = config_.number({
                "emotions", "selfesteem_success_delta"
            });
            pending_appraisals_.push_back(r);
        }
        o.acknowledged.insert(o.debtor);
    }
    void World::transfer_action(const Action & a) {
        auto mode = str(a.args, "mode"), to = str(a.args, "to");
        if (mode == "gift" || mode == "loan_item" || mode == "return") {
            auto item = str(a.args, "item");
            move_item(item, {
                "inventory", to
            }, mode == "gift" ? to : Id {}, a, "item_transferred");
        }
        if (mode == "loan_money" || mode == "repay") transfer_money(state_.actors.at(a.actor).account, state_.actors.at(to).account,
        a.price, a);
        if (mode == "loan_item" || mode == "loan_money") {
            auto & p = state_.proposals.at(a.proposal);
            p.executed = true;
            Obligation o;
            o.id = p.id + "/obligation";
            o.root = p.root;
            o.debtor = to;
            o.creditor = a.actor;
            o.kind = mode == "loan_item" ? "return_item" : "repay_money";
            o.item = text(a.args, "item");
            o.amount = a.price;
            o.remaining = a.price;
            o.due = get < Minute >(p.terms, "due");
            o.important = get_or < bool >(p.terms, "important", false);
            o.acknowledged = {
                a.actor, to
            };
            state_.obligations.emplace(o.id, o);
            emit(a.id, "obligation_created", {
                a.actor, to
            }, {}, encode(o), true, o.root);
            appraise(to, a.actor, o.root, "help");
        } else if (mode == "return") {
            auto & o = state_.obligations.at(str(a.args, "obligation"));
            satisfy_obligation(o, a.id);
            o.acknowledged.insert(to);
            appraise(to, a.actor, o.root, "return");
        } else if (mode == "repay") {
            auto & o = state_.obligations.at(str(a.args, "obligation"));
            o.remaining -= a.price;
            if (o.remaining == 0) {
                satisfy_obligation(o, a.id);
                o.acknowledged.insert(to);
                appraise(to, a.actor, o.root, "return");
            }
        }
    }
    void World::complete_job(const Action & a) {
        auto & j = state_.jobs.at(a.job);
        if (j.worked != j.required || j.status == "completed") throw InvariantError("job completion repeated/early");
        if (j.recipe == "R01") {
            auto id = j.id + "/output/0";
            auto out = make_item(id, "I01", j.creator, {
                "at", j.place
            }, config_);
            out.created_by = j.id;
            if (! state_.items.emplace(id, out).second) throw InvariantError("duplicate job output");
            j.outputs.push_back(id);
            j.recorded_loss_kg = get < double >(config_.recipes.at(j.recipe), "recorded_waste_kg");
        } else if (j.recipe == "R02") {
            auto & t = state_.items.at(j.target);
            t.condition = 1;
            ++ t.version;
            j.recorded_loss_kg = .3;
        } else if (j.recipe == "R03") {
            auto & t = state_.items.at(j.target);
            t.secured = true;
            ++ t.version;
            auto & r = state_.items.at(j.tools.front());
            r.placement = {
                "installed", j.target
            };
            ++ r.version;
        }
        for (auto & id : j.materials) {
            auto & i = state_.items.at(id);
            i.placement = {
                "tombstone", j.id
            };
            i.remaining_units = 0;
            ++ i.version;
        }
        j.status = "completed";
        j.active_action.clear();
        emit(a.id, "job_completed", {
            a.actor
        }, j.outputs, Object {
            {
                "job", j.id
            }, {
                "recipe", j.recipe
            }, {
                "target", j.target
            }
        }, false, j.id);
        emit(a.id, "item_transformed", {
            a.actor
        }, j.outputs, Object {
            {
                "inputs", encode(j.materials)
            }, {
                "outputs", encode(j.outputs)
            }, {
                "recorded_loss_kg", j.recorded_loss_kg
            }
        }, false, j.id);
        for (auto &[id, o] : state_.obligations) if ((o.status == "pending" || o.status == "breached") && o.kind == "repair" && o.debtor == a.actor && o.target == j.target) satisfy_obligation(o,
        a.id);
    }
    void World::commit(const Id & id) {
        Action a = state_.actions.at(id);
        auto & actor = state_.actors.at(a.actor);
        if (a.type == "A02") {
            auto dest = actor.position.to;
            actor.position = Position {};
            actor.position.place = dest;
            emit(a.id, "arrived", {
                a.actor
            }, {}, Object {
                {
                    "place", dest
                }
            });
        } else if (a.type == "A03") {
            std::vector < Id > ids;
            if (has(a.args, "item")) {
                auto target = str(a.args, "item");
                ids.push_back(target);
                for (const auto &[x, i] : state_.items) if (i.placement.kind == "in" && i.placement.ref == target && accessible(a.actor,
                x)) ids.push_back(x);
            } else if (text(a.args, "scope", "all") == "all") for (const auto &[x, i] : state_.items) if (accessible(a.actor,
            x)) ids.push_back(x);
            for (auto & x : ids) {
                const auto & i = state_.items.at(x);
                Evidence e;
                e.id = a.id + "/inspect/" + x;
                e.source = a.actor;
                e.roots = {
                    e.id
                };
                e.learned_at = state_.time + 1;
                e.statement = {
                    x, "item_seen", Object {
                        {
                            "type", i.type
                        }, {
                            "place", item_place(x)
                        }, {
                            "holder", holder(x)
                        }, {
                            "condition", i.condition
                        }
                    }, true, state_.time + 1, - 1
                };
                remember(a.actor, e);
            }
            if (has(a.args, "place") && text(a.args, "scope", "all") == "all") for (const auto &[who, b] : state_.actors) if (who != a.actor && at_place(who,
            str(a.args, "place"))) {
                Evidence e;
                e.id = a.id + "/person/" + who;
                e.source = a.actor;
                e.roots = {
                    e.id
                };
                e.learned_at = state_.time + 1;
                e.statement = {
                    who, "at", Object {
                        {
                            "place", str(a.args, "place")
                        }
                    }, true, state_.time + 1, - 1
                };
                remember(a.actor, e);
            }
            if (has(a.args, "place")) for (const auto &[sid, shop] : state_.shops) if (at_place(a.actor, shop.place)) {
                Array offers;
                for (const auto &[item, price] : shop.stock) if (accessible(a.actor, item)) offers.push_back(Object {
                    {
                        "item", item
                    }, {
                        "type", state_.items.at(item).type
                    }, {
                        "price", price
                    }
                });
                Evidence e;
                e.id = a.id + "/stock/" + sid;
                e.source = a.actor;
                e.roots = {
                    e.id
                };
                e.learned_at = state_.time + 1;
                e.statement = {
                    sid, "shop_stock", Object {
                        {
                            "place", shop.place
                        }, {
                            "offers", offers
                        }, {
                            "open_windows", encode(state_.places.at(shop.place).open_windows)
                        }
                    }, true, state_.time + 1, - 1
                };
                remember(a.actor, e);
            }
            emit(a.id, "observation_created", {
                a.actor
            }, {}, Object {
                {
                    "count", static_cast < std::int64_t >(ids.size())
                }
            }, true);
        } else if (a.type == "A04" || a.type == "A20") {
            auto x = str(a.args, "item");
            move_item(x, {
                "inventory", a.actor
            }, {}, a, a.type == "A20" ? "unauthorized_transfer" : "item_moved");
            if (a.type == "A20") {
                auto & d = actor.drives["appropriation"];
                if (d.intensity > 0) d.pressure = math::clip(d.pressure - config_.number({
                    "drive", "success_relief"
                }));
            }
        } else if (a.type == "A05") {
            auto x = str(a.args, "item");
            move_item(x, has(a.args, "container") ? Placement {
                "in", str(a.args, "container")
            }
            : Placement {
                "at", actor.position.place
            }, {}, a, "item_moved");
        } else if (a.type == "A06") {
            const auto & sh = state_.shops.at(str(a.args, "shop"));
            auto item = str(a.args, "item");
            transfer_money(actor.account, sh.account, a.price, a);
            move_item(item, {
                "inventory", a.actor
            }, a.actor, a, "item_transferred");
            emit(a.id, "purchase", {
                a.actor
            }, {
                item
            }, Object {
                {
                    "shop", sh.id
                }, {
                    "item", item
                }, {
                    "holder", a.actor
                }, {
                    "place", sh.place
                }
            });
            Record rec;
            rec.id = a.id + "/receipt";
            rec.author = a.actor;
            RecordVersion v;
            v.time = state_.time + 1;
            v.acl = {
                a.actor
            };
            v.content = Object {
                {
                    "type", "receipt"
                }, {
                    "buyer", a.actor
                }, {
                    "seller", sh.id
                }, {
                    "item", item
                }, {
                    "price", a.price
                }
            };
            rec.versions.push_back(v);
            state_.records.emplace(rec.id, rec);
            emit(a.id, "record_created", {
                a.actor
            }, {}, Object {
                {
                    "record", rec.id
                }, {
                    "version", 1
                }
            }, true);
        } else if (a.type == "A08" || a.type == "A21") complete_job(a);
        else if (a.type == "A13") transfer_action(a);
        else if (a.type == "A25") {
            auto id = str(a.args, "resource");
            auto & acl = state_.permissions[id];
            if (acl.resource.empty()) {
                acl.resource = id;
                acl.authority = a.actor;
            }
            auto who = str(a.args, "grantee");
            if (get < bool >(a.args, "grant")) {
                acl.grantees.insert(who);
                acl.denied.erase(who);
            } else {
                acl.grantees.erase(who);
                acl.denied.insert(who);
            }
            ++ acl.version;
            emit(a.id, "access_changed", {
                a.actor, str(a.args, "grantee")
            }, {}, Object {
                {
                    "resource", id
                }, {
                    "granted", get < bool >(a.args, "grant")
                }, {
                    "version", acl.version
                }
            }, true);
        } else if (a.type == "A26") {
            auto & i = state_.items.at(str(a.args, "item"));
            i.open = get < bool >(a.args, "open");
            ++ i.version;
            emit(a.id, "container_state_changed", {
                a.actor
            }, {
                i.id
            }, Object {
                {
                    "item", i.id
                }, {
                    "open", i.open
                }
            });
        } else if (a.type == "A11" || a.type == "A12" || a.type == "A14" || a.type == "A17" || a.type == "A18" || a.type == "A19" || a.type == "A24" || a.type == "A27" || a.type == "A28" || a.type == "A29") social_commit(a);
        for (auto & key : a.commands) state_.receipts.at(key).status = "completed";
        emit(a.id, a.type == "A09" ? "sleep_ended" : a.type == "A15" ? "joint_ended" : "action_ended", a.participants, {},
        Object {
            {
                "action_type", a.type
            }
        }, a.type == "A31" || a.type == "A32");
        release(id);
    }
}
