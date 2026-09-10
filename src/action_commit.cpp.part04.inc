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
