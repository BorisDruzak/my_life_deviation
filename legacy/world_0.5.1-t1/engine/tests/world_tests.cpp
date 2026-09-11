#include "fixtures.hpp"
using namespace testdata;
TEST(IT01_idle_and_hysteresis) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N01"].value = 45;
    World w(c, s);
    w.run(2);
    NEAR(w.debug_state().actors.at("a").needs.at("N01").value, 45 - 3.2 * 2 / 60, 1e-9);
    CHECK(w.debug_state().actors.at("a").needs.at("N01").active);
}
TEST(IT02_food_full) {
    auto c = config();
    auto s = state(c);
    s.items["meal"].owner = "a";
    s.items["meal"].placement = {
        "inventory", "a"
    };
    s.shops["store"].stock.clear();
    s.actors["a"].needs["N01"].value = 30;
    World w(c, s);
    do_action(w, cmd("a", 1, "A07", item_args("meal")), 15);
    NEAR(w.debug_state().actors.at("a").needs.at("N01").value, 64.2, 1e-9);
    CHECK(w.debug_state().items.at("meal").placement.kind == "tombstone");
}
TEST(IT03_interrupted_food) {
    auto c = config();
    auto s = state(c);
    s.items["meal"].owner = "a";
    s.items["meal"].placement = {
        "inventory", "a"
    };
    s.shops["store"].stock.clear();
    s.actors["a"].needs["N01"].value = 30;
    World w(c, s);
    do_action(w, cmd("a", 1, "A07", item_args("meal")), 7);
    w.submit(cmd("a", 2, "A22"));
    w.submit(cmd("a", 3, "A07", item_args("meal")));
    w.run(8);
    NEAR(w.debug_state().actors.at("a").needs.at("N01").value, 64.2, 1e-9);
}
TEST(IT04_sleep_net) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N02"].value = 30;
    World w(c, s);
    do_action(w, cmd("a", 1, "A09", Object {
        {
            "bed", "bed"
        }, {
            "minutes", 60
        }
    }), 60);
    NEAR(w.debug_state().actors.at("a").needs.at("N02").value, 40, 1e-9);
}
TEST(IT05_travel_no_teleport) {
    auto c = config();
    World w(c, state(c));
    do_action(w, cmd("a", 1, "A02", Object {
        {
            "edge", "shop_home"
        }
    }), 4);
    CHECK(w.debug_state().actors.at("a").position.kind == "transit");
    CHECK(w.debug_state().actors.at("a").position.place.empty());
    w.run(6);
    CHECK(w.debug_state().actors.at("a").position.place == "home");
}
TEST(IT06_rest_stress) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].stress = .8;
    World w(c, s);
    do_action(w, cmd("a", 1, "A23", Object {
        {
            "minutes", 1
        }
    }));
    NEAR(w.debug_state().actors.at("a").stress, .8 * std::exp2(- 1.0 / 120) - .006, 1e-12);
}
TEST(IT07_exact_close) {
    auto c = config();
    auto s = state(c);
    s.time = 1317;
    s.places["shop"].open_windows = {
        {
            480, 1320
        }
    };
    World w(c, s);
    auto q = cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    });
    do_action(w, q, 3);
    CHECK(receipt(w, q).status == "completed");
    CHECK(w.debug_state().accounts.at("a") == 50);
}
TEST(IT08_misses_open_window) {
    auto c = config();
    auto s = state(c);
    s.time = 1318;
    s.places["shop"].open_windows = {
        {
            480, 1320
        }
    };
    World w(c, s);
    auto q = cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().accounts.at("a") == 80);
}
TEST(IT09_competing_purchase) {
    auto c = config();
    World w(c, state(c));
    auto a = cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    }), b = cmd("b", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    });
    w.submit(b);
    w.submit(a);
    w.run(3);
    CHECK(events(w, "purchase") == 1);
    CHECK(w.debug_state().items.at("meal").owner == "a");
    CHECK(receipt(w, b).status == "rejected");
}
TEST(IT10_purchase_abort) {
    auto c = config();
    World w(c, state(c));
    do_action(w, cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    }));
    CHECK(w.debug_state().accounts.at("a") == 80);
    do_action(w, cmd("a", 2, "A22"));
    CHECK(w.debug_state().accounts.at("a") == 80);
    CHECK(w.debug_state().items.at("meal").owner == "store");
    CHECK(w.debug_state().reservations.empty());
}
TEST(IT11_command_idempotence) {
    auto c = config();
    World w(c, state(c));
    auto q = cmd("a", 17, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    });
    do_action(w, q, 3);
    auto n = w.debug_state().ledger.size();
    CHECK(w.submit(q).status == "completed");
    CHECK(w.debug_state().ledger.size() == n);
    q.args.as_object()["item"] = "tool";
    THROWS(w.submit(q));
}
TEST(IT13_rational_wages) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].position.place = "work";
    World w(c, s);
    do_action(w, cmd("a", 1, "A10", Object {
        {
            "contract", "employment"
        }, {
            "minutes", 61
        }
    }), 61);
    CHECK(w.debug_state().accounts.at("a") == 117);
    CHECK(w.debug_state().contracts.at("employment").accrued_numerator == 37);
}
TEST(IT14_closed_container) {
    auto c = config();
    auto s = state(c);
    s.items["tool"].placement = {
        "in", "box"
    };
    World w(c, s);
    do_action(w, cmd("b", 1, "A03", Object {
        {
            "item", "box"
        }
    }), 2);
    auto v = canonical(w.view("b"));
    CHECK(v.find("\"subject\":\"tool\"") == std::string::npos);
    CHECK(v.find("\"subject\":\"box\"") != std::string::npos);
}
TEST(IT15_hidden_world_noninterference) {
    auto c = config();
    auto s1 = state(c), s2 = s1;
    s1.items["tool"].placement = {
        "at", "home"
    };
    s2.items["tool"].placement = {
        "at", "park"
    };
    World a(c, s1), b(c, s2);
    a.run(2);
    b.run(2);
    CHECK(canonical(a.view("b")) == canonical(b.view("b")));
}
TEST(IT22_repair_resume) {
    auto c = config();
    World w(c, state(c));
    auto args = Object {
        {
            "recipe", "R02"
        }, {
            "target", "target"
        }, {
            "tools", Array {
                "tool"
            }
        }, {
            "materials", Array {
                "kit"
            }
        }
    };
    do_action(w, cmd("a", 1, "A21", args), 12);
    CHECK(w.debug_state().jobs.size() == 1);
    auto id = w.debug_state().jobs.begin()->first;
    w.submit(cmd("a", 2, "A22"));
    w.submit(cmd("a", 3, "A21", Object {
        {
            "job", id
        }
    }));
    w.advance();
    auto restored = World::restore(c, w.snapshot());
    restored.run(17);
    CHECK(restored.debug_state().jobs.at(id).worked == 30);
    NEAR(restored.debug_state().items.at("target").condition, 1, 1e-12);
    CHECK(events(restored, "job_completed") == 1);
}
TEST(IT29_unobserved_theft) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].position.place = "home";
    s.items["tool"].placement = {
        "at", "shop"
    };
    World w(c, s);
    do_action(w, cmd("b", 1, "A20", item_args("tool")));
    CHECK(w.debug_state().items.at("tool").owner == "a");
    CHECK(w.debug_state().items.at("tool").placement.ref == "b");
    NEAR(w.debug_state().actors.at("a").stress, 0, 1e-12);
    CHECK(canonical(w.view("a")).find("unauthorized_transfer") == std::string::npos);
}
TEST(IT31_sampling_repeatability) {
    auto c = config(false);
    World a(c, state(c)), b(c, state(c));
    auto x = cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    });
    a.submit(x);
    b.submit(x);
    a.run(3);
    b.run(3);
    CHECK(canonical(a.snapshot()) == canonical(b.snapshot()));
}
TEST(IT32_forecast_isolation) {
    auto c = config();
    World w(c, state(c));
    auto before = canonical(w.snapshot());
    for (int i = 0; i < 100; ++ i) {
        auto a = w.debug_state().actors.at("a");
        auto predicted = predict_physiology(c, a, 120, "ordinary");
        CHECK(predicted.needs.at("N01").value < a.needs.at("N01").value);
    }
    CHECK(canonical(w.snapshot()) == before);
}
TEST(IT33_snapshot_active) {
    auto c = config();
    World a(c, state(c));
    a.submit(cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    }));
    a.advance();
    auto b = World::restore(c, a.snapshot());
    a.run(7);
    b.run(7);
    CHECK(canonical(a.snapshot()) == canonical(b.snapshot()));
}
TEST(IT34_hash_mismatch) {
    auto c = config();
    World w(c, state(c));
    auto p = c.parameters;
    p.as_object()["traits"].as_object()["patience"] = .9;
    auto d = Config::from_json(p, c.catalog);
    THROWS(World::restore(d, w.snapshot()));
}
TEST(IT35_traits_do_not_change_nutrition) {
    auto c = config();
    auto x = state(c), y = x;
    y.actors["a"].traits["patience"] = .99;
    y.actors["a"].horizon = 999;
    World a(c, x), b(c, y);
    a.run(60);
    b.run(60);
    NEAR(a.debug_state().actors.at("a").needs.at("N01").value, b.debug_state().actors.at("a").needs.at("N01").value,
    1e-12);
}
TEST(IT36_no_automatic_drive) {
    auto c = config();
    World w(c, state(c));
    for (int i = 0; i < 4; ++ i) do_action(w, cmd("a", i + 1, "A16", Object {
        {
            "item", "book"
        }, {
            "minutes", 30
        }
    }), 30);
    NEAR(w.debug_state().actors.at("a").drives.at("appropriation").intensity, 0, 1e-12);
}
TEST(IT37_fatigue_interrupt) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].fatigue = .949;
    World w(c, s);
    do_action(w, cmd("a", 1, "A21", Object {
        {
            "recipe", "R02"
        }, {
            "target", "target"
        }, {
            "tools", Array {
                "tool"
            }
        }, {
            "materials", Array {
                "kit"
            }
        }
    }));
    CHECK(w.debug_state().actors.at("a").active_action.empty());
    CHECK(! w.debug_state().actors.at("a").heavy_allowed);
    CHECK(w.debug_state().jobs.begin()->second.worked == 1);
}
TEST(IT38_terminal_no_resurrection) {
    auto c = config();
    auto s = state(c);
    External e;
    e.id = "injury";
    e.at = 1;
    e.kind = "injury";
    e.args = Object {
        {
            "actor", "a"
        }, {
            "amount", 1.0
        }
    };
    s.external.push_back(e);
    World w(c, s);
    w.advance();
    CHECK(! w.debug_state().actors.at("a").alive);
    auto q = cmd("a", 1, "A09", Object {
        {
            "bed", "bed"
        }, {
            "minutes", 10
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().items.contains("tool"));
}
TEST(V07_invalid_state) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].health = std::numeric_limits < double >::quiet_NaN();
    THROWS(World(c, s));
}
TEST(V08_container_cycle) {
    auto c = config();
    auto s = state(c);
    s.items["box"].placement = {
        "in", "box"
    };
    THROWS(World(c, s));
}
TEST(V09_non_integer_command) {
    THROWS(decode < Command >(parse_json("{\"actor\":\"a\",\"sequence\":1.5}")));
}
