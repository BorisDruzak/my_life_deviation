#include "fixtures.hpp"
#include "npc/controller.hpp"
using namespace testdata;
namespace {
    void known_item(State & s, const Id & actor, const Id & item, const Id & owner, const std::string & type) {
        Evidence e;
        e.id = "known/" + actor + "/" + item;
        e.source = "genesis";
        e.roots = {
            e.id
        };
        e.statement = {
            item, "item_seen", Object {
                {
                    "type", type
                }, {
                    "holder", owner
                }, {
                    "place", "shop"
                }
            }, true, 0, - 1
        };
        s.actors[actor].beliefs.push_back(e);
    }
    void known_store(State & s, const Id & actor, const Id & item, const std::string & type, Money price) {
        Evidence e;
        e.id = "store/" + actor;
        e.source = "genesis";
        e.roots = {
            e.id
        };
        e.statement = {
            "store", "shop_stock", Object {
                {
                    "place", "shop"
                }, {
                    "offers", Array {
                        Object {
                            {
                                "item", item
                            }, {
                                "type", type
                            }, {
                                "price", price
                            }
                        }
                    }
                }
            }, true, 0, - 1
        };
        s.actors[actor].beliefs.push_back(e);
    }
}
TEST(C01_hungry_uses_owned_food) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N01"].value = 30;
    s.items["meal"].owner = "a";
    s.items["meal"].placement = {
        "inventory", "a"
    };
    s.shops["store"].stock.clear();
    World w(c, s);
    auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A07");
}
TEST(C02_known_lender_creates_candidate) {
    auto c = config();
    auto s = state(c);
    known_item(s, "b", "tool", "a", "I05");
    s.actors["b"].relations["a"].trust = .9;
    World w(c, s);
    auto d = decide(c, w.view("b"), Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A11");
    CHECK(str(d.command->args, "to") == "a");
}
TEST(C03_removed_knowledge_removes_lender_plan) {
    auto c = config();
    World w(c, state(c));
    auto d = decide(c, w.view("b"), Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    for (auto & x : d.candidates) CHECK(x.method != "borrow");
}
TEST(C04_resource_deprivation_changes_plan) {
    auto c = config();
    auto s = state(c);
    known_store(s, "b", "tool2", "I05", 40);
    World w(c, s);
    auto d = decide(c, w.view("b"), Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A06");
    auto v = w.view("b");
    v.as_object()["money"] = 0;
    auto e = decide(c, v, Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    CHECK(! e.command.has_value() || e.command->type != "A06");
}
TEST(C05_policy_has_no_world_side_effect) {
    auto c = config();
    auto s = state(c);
    known_store(s, "b", "tool2", "I05", 40);
    World w(c, s);
    auto before = canonical(w.snapshot());
    for (int i = 0; i < 20; ++ i) decide(c, w.view("b"), Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    CHECK(canonical(w.snapshot()) == before);
}
TEST(C06_busy_controller_does_not_invent_slots) {
    auto c = config();
    World w(c, state(c));
    do_action(w, cmd("a", 1, "A09", Object {
        {
            "bed", "bed"
        }, {
            "minutes", 30
        }
    }));
    auto d = decide(c, w.view("a"));
    CHECK(! d.command.has_value());
}
TEST(C07_forecast_prepares_does_not_overeat) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N01"].value = 62;
    s.actors["a"].horizon = 360;
    known_store(s, "a", "meal", "I01", 30);
    World w(c, s);
    auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A06");
    CHECK(d.goal == "prepare_food");
}
TEST(C08_known_closed_shop_is_not_retried_each_item) {
    auto c = config();
    auto s = state(c);
    s.actors["b"].needs["N01"].value = 30;
    known_store(s, "b", "meal", "I01", 30);
    s.actors["b"].beliefs.back().statement.arguments.as_object()["open_windows"] = encode(std::vector<std::vector<Minute>>{{60, 120}});
    World w(c, s);
    auto d = decide(c, w.view("b"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A01");
}
TEST(C09_failed_purchase_prompts_refresh_not_item_spam) {
    auto c = config();
    auto s = state(c);
    known_store(s, "b", "wrong", "I05", 40);
    World w(c, s);
    auto q = cmd("b", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "wrong"
        }
    });
    do_action(w, q);
    auto d = decide(c, w.view("b"), Object {
        {
            "mode", "acquire"
        }, {
            "item_type", "I05"
        }, {
            "due", 100
        }
    });
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A03");
}

namespace {
    Json audit_fixture(const std::string & name) {
        return read_json(std::filesystem::path(NPC_SOURCE_DIR) /
            "reports/audit_2026-09-10/fixtures" / (name + ".scenario.json"));
    }
    Json conversation_terms() {
        return Object {{"kind", "joint"}, {"place", "shop"}, {"start", 0},
            {"end", 120}, {"minutes", 3}, {"topic", "conversation"}};
    }
    Id pending_conversation(World & w, const Id & author = "a", const Id & other = "b",
                            const std::string & channel = "speech") {
        auto terms = conversation_terms();
        terms.as_object()["start"] = w.debug_state().time;
        const auto q = cmd(author, 1, "A14", Object {{"participants", Array {author, other}},
            {"expires", 100}, {"terms", terms}, {"channel", channel}});
        do_action(w, q);
        CHECK(receipt(w, q).status == "completed");
        return receipt(w, q).action + "/proposal";
    }
    void give_meal(State & s, const Id & actor) {
        s.items.at("meal").owner = actor;
        s.items.at("meal").placement = {"inventory", actor};
        s.shops.at("store").stock.clear();
    }
    void pending_handoff(World & w) {
        const auto proposal = cmd("b", 1, "A14", Object {
            {"participants", Array {"a", "b"}}, {"expires", 100},
            {"terms", Object {{"kind", "loan_item"}, {"lender", "a"},
                {"borrower", "b"}, {"item", "tool"}, {"due", 120}}}});
        do_action(w, proposal);
        CHECK(receipt(w, proposal).status == "completed");
        const auto answer = cmd("a", 1, "A12", Object {{"proposal", "b@1/proposal"},
            {"version", 1}, {"answer", "accept"}});
        do_action(w, answer);
        CHECK(receipt(w, answer).status == "completed");
    }
    World accepted_conversation(const Config & c) {
        const auto scenario = audit_fixture("completed_joint_retry");
        auto w = World::from_scenario(c, at(scenario, "world"));
        // Exact first two boundaries of the audit fixture; no autonomous readiness yet.
        for (Minute time = 0; time < 2; ++time) {
            for (const auto & row : at(scenario, "script").as_array()) {
                if (get<Minute>(row, "at") == time) w.submit(decode<Command>(at(row, "command")));
            }
            w.advance();
            w.check_invariants();
        }
        CHECK(w.debug_state().proposals.at("a@1/proposal").state == "accepted");
        return w;
    }
    void start_autonomous_conversation(World & w, const Config & c) {
        for (const auto & actor : {"a", "b"}) {
            const auto d = decide(c, w.view(actor));
            CHECK(d.command.has_value());
            CHECK(d.command->type == "A15");
            w.submit(*d.command);
        }
        w.advance();
        w.check_invariants();
        CHECK(events(w, "joint_started") == 1);
    }
    void append_local_receipt(Json & view, const Id & proposal, std::int64_t version,
                              const std::string & status) {
        // Policy-boundary fixture, not a claim that World allows countering an executed session.
        const auto q = cmd("a", 90, "A15", Object {{"proposal", proposal}, {"version", version}});
        view.as_object()["receipts"].as_array().push_back(Object {
            {"id", "a@90"}, {"status", status}, {"received", 1}, {"request", encode(q)}});
    }
    void check_old_version_is_not_terminal(const std::string & status) {
        const auto c = config();
        auto w = accepted_conversation(c);
        auto view = w.view("a");
        // A v2 local agreement and an old v1 receipt must not be treated as the same session.
        for (auto & evidence : view.as_object()["self"].as_object()["beliefs"].as_array()) {
            auto & statement = evidence.as_object()["statement"];
            if (has(at(statement, "arguments"), "proposal") &&
                text(at(statement, "arguments"), "proposal") == "a@1/proposal") {
                statement.as_object()["arguments"].as_object()["version"] = 2;
            }
        }
        append_local_receipt(view, "a@1/proposal", 1, status);
        const auto d = decide(c, view);
        CHECK(d.command.has_value());
        CHECK(d.command->type == "A15");
        CHECK(get<std::int64_t>(d.command->args, "version") == 2);
    }
}

TEST(C10_pending_response_eats_before_critical_threshold) {
    const auto c = config();
    const auto scenario = audit_fixture("waiting_overrides_hunger");
    auto w = World::from_scenario(c, at(scenario, "world"));
    do_action(w, decode<Command>(at(at(scenario, "script").as_array().front(), "command")));
    const auto & hunger = w.debug_state().actors.at("a").needs.at("N01");
    CHECK(hunger.value > hunger.critical);
    const auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A07");
    do_action(w, *d.command);
    CHECK(w.debug_state().items.at("meal").remaining_units < 15);
    CHECK(events(w, "need_critical") == 0);
}
TEST(C11_safety_respects_passive_and_busy_actor) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("a").needs.at("N01").value = 20;
    give_meal(s, "a");
    World w(c, s);
    CHECK(!decide(c, w.view("a"), Object {{"mode", "passive"}}).command.has_value());
    do_action(w, cmd("a", 1, "A01", Object {{"minutes", 2}}));
    CHECK(!decide(c, w.view("a")).command.has_value());
}
TEST(C12_pending_response_allows_leisure) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("a").needs.at("N04").value = 20;
    World w(c, s);
    pending_conversation(w);
    const auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A16");
    do_action(w, *d.command);
    CHECK(receipt(w, *d.command).status == "started");
}
TEST(C13_pending_handoff_allows_eating) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("b").needs.at("N01").value = 20;
    give_meal(s, "b");
    World w(c, s);
    pending_handoff(w);
    const auto d = decide(c, w.view("b"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A07");
    do_action(w, *d.command);
    CHECK(w.debug_state().items.at("meal").remaining_units < 15);
}
TEST(C14_pending_handoff_allows_rest) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("b").fatigue = .8;
    World w(c, s);
    pending_handoff(w);
    const auto d = decide(c, w.view("b"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A23");
    do_action(w, *d.command);
    CHECK(receipt(w, *d.command).status == "started");
}
TEST(C15_pending_response_allows_sleep) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("a").needs.at("N02").value = 20;
    known_item(s, "a", "bed", "a", "F01");
    World w(c, s);
    pending_conversation(w);
    const auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A09");
    do_action(w, *d.command);
    CHECK(receipt(w, *d.command).status == "started");
}
TEST(C16_unread_message_does_not_preempt_eating) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("b").needs.at("N01").value = 20;
    give_meal(s, "b");
    World w(c, s);
    pending_conversation(w, "a", "b", "digital");
    w.run(2);
    const auto view = w.view("b");
    CHECK(!at(view, "inbox").as_array().empty());
    CHECK(!get<bool>(at(view, "inbox").as_array().front(), "read"));
    CHECK(!has(at(view, "inbox").as_array().front(), "content"));
    const auto d = decide(c, view);
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A07");
    CHECK(canonical(w.view("b")) == canonical(view));
}
TEST(C17_unread_message_does_not_preempt_sleep) {
    const auto c = config();
    auto s = state(c);
    s.items.at("bed").owner = "b";
    s.actors.at("b").needs.at("N02").value = 20;
    known_item(s, "b", "bed", "b", "F01");
    World w(c, s);
    pending_conversation(w, "a", "b", "digital");
    w.run(2);
    const auto view = w.view("b");
    CHECK(!at(view, "inbox").as_array().empty());
    CHECK(!get<bool>(at(view, "inbox").as_array().front(), "read"));
    const auto d = decide(c, view);
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A09");
}
TEST(C18_task_modes_do_not_preempt_immediate_eating) {
    const auto c = config();
    auto s = state(c);
    s.actors.at("b").needs.at("N01").value = 20;
    give_meal(s, "b");
    World w(c, s);
    for (const auto & mode : {"household", "acquire", "repair"}) {
        const auto d = decide(c, w.view("b"), Object {{"mode", mode},
            {"item_type", "I05"}, {"target", "target"}, {"due", 100}});
        CHECK(d.command.has_value());
        CHECK(d.command->type == "A07");
    }
}
TEST(C19_outgoing_wait_does_not_hide_an_incoming_proposal) {
    const auto c = config();
    World w(c, state(c));
    pending_conversation(w);
    const auto incoming = pending_conversation(w, "b", "a");
    const auto d = decide(c, w.view("a"));
    CHECK(d.command.has_value());
    CHECK(d.command->type == "A12");
    CHECK(text(d.command->args, "proposal") == incoming);
}
TEST(C20_completed_joint_is_not_replayed) {
    const auto c = config();
    auto w = accepted_conversation(c);
    start_autonomous_conversation(w, c);
    w.run(2);
    CHECK(events(w, "joint_ended") == 1);
    for (const auto & actor : {"a", "b"}) {
        CHECK(w.debug_state().receipts.at(std::string(actor) + "@3").status == "completed");
        const auto d = decide(c, w.view(actor));
        CHECK(!d.command || d.command->type != "A15");
    }
    CHECK(events(w, "command_rejected") == 0);
}
TEST(C21_interrupted_joint_is_not_replayed) {
    const auto c = config();
    auto w = accepted_conversation(c);
    start_autonomous_conversation(w, c);
    do_action(w, cmd("b", 4, "A22", Object {{"proposal", "a@1/proposal"}}));
    CHECK(events(w, "action_interrupted") == 1);
    for (const auto & actor : {"a", "b"}) {
        CHECK(w.debug_state().receipts.at(std::string(actor) + "@3").status == "interrupted");
    }
    while (w.debug_state().time < 20) {
        for (const auto & actor : {"a", "b"}) {
            const auto d = decide(c, w.view(actor));
            CHECK(!d.command || d.command->type != "A15");
            if (d.command) w.submit(*d.command);
        }
        w.advance();
        w.check_invariants();
    }
    CHECK(events(w, "command_rejected") == 0);
    CHECK(events(w, "joint_started") == 1);
}
TEST(C22_rejected_readiness_can_be_retried) {
    const auto c = config();
    auto w = accepted_conversation(c);
    const auto first = decide(c, w.view("a"));
    CHECK(first.command && first.command->type == "A15");
    do_action(w, *first.command); // b has not independently submitted readiness.
    CHECK(receipt(w, *first.command).status == "rejected");
    start_autonomous_conversation(w, c);
    CHECK(events(w, "command_rejected") == 1);
}
TEST(C23_completed_old_version_does_not_block_current_joint) {
    check_old_version_is_not_terminal("completed");
}
TEST(C24_interrupted_old_version_does_not_block_current_joint) {
    check_old_version_is_not_terminal("interrupted");
}
TEST(C25_terminal_receipt_for_other_proposal_does_not_block_joint) {
    const auto c = config();
    auto w = accepted_conversation(c);
    for (const auto & status : {"completed", "interrupted"}) {
        auto view = w.view("a");
        append_local_receipt(view, "other@1/proposal", 1, status);
        const auto d = decide(c, view);
        CHECK(d.command && d.command->type == "A15");
        CHECK(text(d.command->args, "proposal") == "a@1/proposal");
    }
}
