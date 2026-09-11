#include "npc/world.hpp"
#include <algorithm>
namespace npc {
    void World::clocks() {
        const auto now = state_.time + 1;
        for (auto &[id, c] : state_.contracts) if (now >= c.next_payout) {
            Money due = c.accrued_numerator / 60;
            Money payable = std::min(due, available(c.employer_account));
            const auto & worker = state_.actors.at(c.worker);
            payable = std::min(payable, max_money - state_.accounts.at(worker.account));
            if (payable > 0) {
                Action synthetic;
                synthetic.id = "wage/" + id + "/" + std::to_string(c.next_payout);
                synthetic.actor = c.worker;
                synthetic.participants = {
                    c.worker
                };
                transfer_money(c.employer_account, worker.account, payable, synthetic);
                c.accrued_numerator -= payable * 60;
                emit(synthetic.id, "wage_paid", {
                    c.worker
                }, {}, Object {
                    {
                        "contract", id
                    }, {
                        "amount", payable
                    }
                }, true);
            }
            do {
                if (c.next_payout > max_minute - c.payout_period) throw InvariantError("payout timestamp overflow");
                c.next_payout += c.payout_period;
            }
            while (c.next_payout <= now);
        }
        std::vector < std::size_t > pending;
        for (std::size_t i = 0; i < state_.external.size(); ++ i) if (! state_.external[i].applied && state_.external[i].at <= now) pending.push_back(i);
        std::sort(pending.begin(), pending.end(),[&](auto a, auto b) {
            const auto & x = state_.external[a];
            const auto & y = state_.external[b];
            return std::tuple {
                x.at, x.priority, x.id
            }
            < std::tuple {
                y.at, y.priority, y.id
            };
        });
        for (auto i : pending) {
            auto & e = state_.external[i];
            if (e.kind == "injury") {
                auto id = str(e.args, "actor");
                auto amount = get < double >(e.args, "amount");
                if (! state_.actors.contains(id) || ! std::isfinite(amount) || amount < 0 || amount > 1) throw InputError("invalid scripted injury");
                auto & a = state_.actors.at(id);
                if (a.alive) {
                    a.health = std::max(0.0, a.health - amount);
                    a.pain = std::min(1.0, a.pain + amount);
                }
                emit(e.id, "external_injury", {
                    id
                }, {}, Object {
                    {
                        "actor", id
                    }, {
                        "amount", amount
                    }
                });
            } else if (e.kind == "cash") {
                auto id = str(e.args, "account");
                auto amount = get < Money >(e.args, "amount");
                if (! state_.accounts.contains(id) || amount > max_money || amount < - max_money || state_.accounts.at(id) + amount < 0 || state_.accounts.at(id) + amount > max_money || state_.external_money + amount > max_money || state_.external_money + amount < - max_money) throw InputError("invalid scripted cash transaction");
                state_.accounts.at(id) += amount;
                state_.external_money += amount;
                std::vector < Id > actors;
                for (const auto &[who, a] : state_.actors) if (a.account == id) actors.push_back(who);
                emit(e.id, "external_cash", actors, {}, Object {
                    {
                        "account", id
                    }, {
                        "amount", amount
                    }
                }, true);
            } else if (e.kind == "edge") {
                auto id = str(e.args, "edge");
                if (! state_.edges.contains(id)) throw InputError("unknown scripted edge");
                state_.edges.at(id).open = get < bool >(e.args, "open");
                emit(e.id, "edge_changed", {}, {}, Object {
                    {
                        "edge", id
                    }, {
                        "open", get < bool >(e.args, "open")
                    }
                }, true);
            } else if (e.kind == "move_item") {
                auto id = str(e.args, "item");
                auto p = decode < Placement >(at(e.args, "placement"));
                if (! state_.items.contains(id)) throw InputError("unknown scripted item");
                // Administrative stimuli are validated on a private candidate state, then committed atomically.
                auto candidate = state_;
                candidate.time = now;
                candidate.items.at(id).placement = p;
                ++ candidate.items.at(id).version;
                for (auto &[sid, sh] : candidate.shops) if (sh.stock.contains(id) && !(p.kind == "at" && p.ref == sh.place)) sh.stock.erase(id);
                validate_state(config_, candidate);
                state_.items.at(id) = candidate.items.at(id);
                state_.shops = std::move(candidate.shops);
                emit(e.id, "external_item_moved", {}, {
                    id
                }, Object {
                    {
                        "item", id
                    }, {
                        "place", item_place(id)
                    }
                });
            } else if (e.kind == "shipment") {
                auto shop = str(e.args, "shop");
                if (! state_.shops.contains(shop)) throw InputError("unknown shipment shop");
                auto id = str(e.args, "item"), type = str(e.args, "item_type");
                auto price = get < Money >(e.args, "price");
                if (state_.items.contains(id) || price < 0 || price > max_money) throw InputError("invalid shipment item");
                auto item = make_item(id, type, shop, {
                    "at", state_.shops.at(shop).place
                }, config_);
                item.created_by = e.id;
                state_.items.emplace(id, item);
                state_.shops.at(shop).stock[id] = price;
                emit(e.id, "external_shipment", {}, {
                    id
                }, Object {
                    {
                        "item", id
                    }, {
                        "shop", shop
                    }, {
                        "place", item_place(id)
                    }
                });
            }
            e.applied = true;
        }
    }
    void World::finish_proposals_and_obligations() {
        const auto now = state_.time + 1;
        for (auto &[id, p] : state_.proposals) if ((p.state == "offered" || p.state == "countered") && now >= p.expires) {
            p.state = "expired";
            emit("expiry/" + id, "proposal_expired", {
                p.author
            }, {}, Object {
                {
                    "proposal", id
                }, {
                    "version", p.version
                }
            }, true, p.root);
        }
        for (auto &[id, o] : state_.obligations) {
            if (o.status == "pending" && now >= o.due) {
                o.status = "breached";
                o.breached_at = now;
                emit("deadline/" + id, "obligation_breached", {}, {}, Object {
                    {
                        "obligation", id
                    }
                }, true, o.root);
            }
            // Only a character who knows the signed promise can react to its deadline; world truth is not broadcast.
            if (o.status == "breached") for (const auto & who : {
                o.debtor, o.creditor
            }) {
                bool knows = o.acknowledged.contains(who);
                if (! knows) for (const auto & b : state_.actors.at(who).beliefs) if ((b.statement.predicate == "proposal_update" || b.statement.predicate == "proposal") && text(b.statement.arguments,
                "state") == "accepted" && std::find(b.roots.begin(), b.roots.end(), o.root) != b.roots.end()) knows = true;
                if (! knows) continue;
                if (who == o.debtor) {
                    Appraisal a;
                    a.observer = who;
                    a.root = o.root;
                    a.type = "own_breach";
                    a.stress_delta = config_.number({
                        "stimuli", "own_promise_breach", "stress"
                    });
                    if (o.important) a.esteem_delta = config_.number({
                        "stimuli", "own_promise_breach", "selfesteem"
                    });
                    pending_appraisals_.push_back(a);
                } else if (o.kind != "repair") appraise(who, o.debtor, o.root, "breach");
                // Remote repair completion is not directly observable; inspection is required.
            }
        }
    }
}
