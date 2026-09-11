#include "fixtures.hpp"
using namespace testdata;
namespace {
    Id propose(World & w, const Id & author, const std::set < Id > & group, Json terms, std::int64_t seq = 1, std::string channel = "speech") {
        auto q = cmd(author, seq, "A14", Object {
            {
                "participants", encode(group)
            }, {
                "terms", terms
            }, {
                "expires", w.debug_state().time + 120
            }, {
                "channel", channel
            }
        });
        do_action(w, q);
        CHECK(receipt(w, q).status == "completed");
        return receipt(w, q).action + "/proposal";
    }
    void accept(World & w, const Id & who, const Id & pid, std::int64_t seq = 1) {
        auto q = cmd(who, seq, "A12", Object {
            {
                "proposal", pid
            }, {
                "version", w.debug_state().proposals.at(pid).version
            }, {
                "answer", "accept"
            }
        });
        do_action(w, q);
        CHECK(receipt(w, q).status == "completed");
    }
    Id joint(World & w, std::set < Id > people = {
        "a", "b"
    }, Minute length = 30, Json extra = Object {}) {
        Object terms {
            {
                "kind", "joint"
            }, {
                "place", "shop"
            }, {
                "start", 0
            }, {
                "end", 120
            }, {
                "minutes", length
            }, {
                "topic", "conversation"
            }
        };
        for (auto & kv : extra.as_object()) terms[kv.key()] = kv.value();
        auto pid = propose(w, "a", people, terms);
        for (auto & id : people) if (id != "a") accept(w, id, pid);
        return pid;
    }
    void start_joint(World & w, const Id & pid, const std::string & type = "A15", std::int64_t seq = 10) {
        const auto & p = w.debug_state().proposals.at(pid);
        for (auto & who : p.participants) w.submit(cmd(who, seq, type, Object {
            {
                "proposal", pid
            }, {
                "version", p.version
            }
        }));
        w.advance();
        w.check_invariants();
    }
    Evidence belief(const std::string & id = "fact", std::string subject = "tool", std::string pred = "at") {
        Evidence e;
        e.id = id;
        e.source = "genesis";
        e.roots = {
            "root"
        };
        e.statement = {
            subject, pred, Object {
                {
                    "place", "shop"
                }
            }, true, 0, - 1
        };
        return e;
    }
}
TEST(S01_loan_money_conservation) {
    auto c = config();
    World w(c, state(c));
    auto p = propose(w, "b", {
        "a", "b"
    }, Object {
        {
            "kind", "loan_money"
        }, {
            "lender", "a"
        }, {
            "borrower", "b"
        }, {
            "amount", 30
        }, {
            "due", 20
        }
    });
    accept(w, "a", p);
    do_action(w, cmd("a", 2, "A13", Object {
        {
            "mode", "loan_money"
        }, {
            "to", "b"
        }, {
            "proposal", p
        }
    }));
    CHECK(w.debug_state().accounts.at("a") == 50);
    CHECK(w.debug_state().accounts.at("b") == 80);
    do_action(w, cmd("b", 2, "A13", Object {
        {
            "mode", "repay"
        }, {
            "to", "a"
        }, {
            "obligation", p + "/obligation"
        }, {
            "amount", 30
        }
    }));
    CHECK(w.debug_state().accounts.at("a") == 80);
    CHECK(w.debug_state().accounts.at("b") == 50);
    CHECK(w.debug_state().obligations.at(p + "/obligation").status == "fulfilled");
}
TEST(S02_sleeping_digital_recipient) {
    auto c = config();
    auto s = state(c);
    s.items["bed"].owner = "b";
    World w(c, s);
    w.submit(cmd("b", 1, "A09", Object {
        {
            "bed", "bed"
        }, {
            "minutes", 10
        }
    }));
    auto p = propose(w, "a", {
        "a", "b"
    }, Object {
        {
            "kind", "joint"
        }, {
            "place", "shop"
        }, {
            "start", 20
        }, {
            "end", 100
        }, {
            "minutes", 10
        }
    }, 1, "digital");
    w.advance();
    auto & b = w.debug_state().actors.at("b");
    CHECK(b.inbox.size() == 1);
    CHECK(w.debug_state().messages.at(b.inbox[0]).readers.empty());
    CHECK(w.debug_state().proposals.at(p).signatures == std::set < Id > {
        "a"
    });
    CHECK(w.debug_state().proposals.at(p).state == "offered");
}
TEST(S03_counter_clears_old_signatures) {
    auto c = config();
    World w(c, state(c));
    auto p = joint(w);
    auto terms = w.debug_state().proposals.at(p).terms;
    terms.as_object()["start"] = 10;
    do_action(w, cmd("a", 2, "A12", Object {
        {
            "proposal", p
        }, {
            "version", 1
        }, {
            "answer", "counter"
        }, {
            "terms", terms
        }
    }));
    CHECK(w.debug_state().proposals.at(p).version == 2);
    CHECK(w.debug_state().proposals.at(p).signatures == std::set < Id > {
        "a"
    });
    start_joint(w, p);
    CHECK(w.debug_state().actions.empty());
}
TEST(S04_refusal_is_not_random_acceptance) {
    auto c = config();
    World w(c, state(c));
    auto p = propose(w, "a", {
        "a", "b"
    }, Object {
        {
            "kind", "joint"
        }, {
            "place", "shop"
        }, {
            "start", 0
        }, {
            "end", 100
        }, {
            "minutes", 10
        }
    });
    auto before = w.debug_state().actors.at("a").relations;
    do_action(w, cmd("b", 1, "A12", Object {
        {
            "proposal", p
        }, {
            "version", 1
        }, {
            "answer", "decline"
        }
    }));
    CHECK(w.debug_state().proposals.at(p).state == "declined");
    CHECK(events(w, "joint_started") == 0);
    CHECK(canonical(encode(before)) == canonical(encode(w.debug_state().actors.at("a").relations)));
}
TEST(S05_joint_requires_both_commands) {
    auto c = config();
    World w(c, state(c));
    auto p = joint(w);
    do_action(w, cmd("a", 2, "A15", Object {
        {
            "proposal", p
        }, {
            "version", 1
        }
    }));
    CHECK(w.debug_state().actors.at("b").active_action.empty());
    CHECK(events(w, "joint_started") == 0);
}
TEST(S06_withdrawal_stops_future_effects) {
    auto c = config();
    World w(c, state(c));
    auto p = joint(w);
    start_joint(w, p);
    w.run(9);
    auto before = w.debug_state().actors.at("b").needs.at("N03").value;
    do_action(w, cmd("b", 11, "A22", Object {
        {
            "proposal", p
        }
    }));
    NEAR(w.debug_state().actors.at("b").needs.at("N03").value, before - 1.5 / 60, 1e-9);
    CHECK(w.debug_state().actions.empty());
    CHECK(w.debug_state().proposals.at(p).state == "withdrawn");
}
TEST(S07_agreement_no_teleport) {
    auto c = config();
    World w(c, state(c));
    auto p = joint(w);
    do_action(w, cmd("a", 2, "A02", Object {
        {
            "edge", "shop_home"
        }
    }), 10);
    start_joint(w, p);
    CHECK(events(w, "joint_started") == 0);
    CHECK(w.debug_state().actors.at("a").position.place == "home");
}
TEST(S08_item_loan_and_return_on_deadline) {
    auto c = config();
    World w(c, state(c));
    auto p = propose(w, "b", {
        "a", "b"
    }, Object {
        {
            "kind", "loan_item"
        }, {
            "lender", "a"
        }, {
            "borrower", "b"
        }, {
            "item", "tool"
        }, {
            "due", 10
        }
    });
    accept(w, "a", p);
    do_action(w, cmd("a", 2, "A13", Object {
        {
            "mode", "loan_item"
        }, {
            "to", "b"
        }, {
            "item", "tool"
        }, {
            "proposal", p
        }
    }));
    CHECK(w.debug_state().items.at("tool").owner == "a");
    CHECK(w.debug_state().items.at("tool").placement.ref == "b");
    w.run(6);
    CHECK(w.debug_state().time == 9);
    do_action(w, cmd("b", 2, "A13", Object {
        {
            "mode", "return"
        }, {
            "to", "a"
        }, {
            "item", "tool"
        }, {
            "obligation", p + "/obligation"
        }
    }));
    CHECK(w.debug_state().obligations.at(p + "/obligation").status == "fulfilled");
    CHECK(w.debug_state().obligations.at(p + "/obligation").breached_at == - 1);
    CHECK(events(w, "obligation_breached") == 0);
}
TEST(S09_group_attention_normalized) {
    auto c = config();
    auto s = state(c);
    for (auto &[id, a] : s.actors) for (auto &[other, b] : s.actors) if (id != other) a.relations[other].affection = .5;
    World w(c, s);
    auto p = joint(w, {
        "a", "b", "c"
    });
    auto before = w.debug_state().actors.at("a").needs.at("N03").value;
    start_joint(w, p);
    NEAR(w.debug_state().actors.at("a").needs.at("N03").value, before - .025 + .4, 1e-9);
}
TEST(S10_rumour_root_not_counted_three_times) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].beliefs.push_back(belief());
    World w(c, s);
    for (int i = 1; i <= 3; ++ i) do_action(w, cmd("a", i, "A19", Object {
        {
            "to", "b"
        }, {
            "statement", encode(belief().statement)
        }, {
            "evidence", Array {
                "fact"
            }
        }
    }));
    auto & b = w.debug_state().actors.at("b");
    int claims = 0;
    for (auto & e : b.beliefs) if (e.statement.predicate == "at") {
        ++ claims;
        CHECK(e.roots == std::vector < Id > {
            "root"
        });
    }
    CHECK(claims == 3);
    NEAR(evaluate_belief(b, belief().statement, w.debug_state().time), .725, 1e-12);
}
TEST(S11_erroneous_claim_no_lie_flag) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].beliefs.push_back(belief());
    s.items["tool"].placement = {
        "at", "home"
    };
    World w(c, s);
    do_action(w, cmd("a", 1, "A19", Object {
        {
            "to", "b"
        }, {
            "statement", encode(belief().statement)
        }, {
            "evidence", Array {
                "fact"
            }
        }
    }));
    auto v = canonical(w.view("b"));
    CHECK(v.find("speaker_belief") == std::string::npos);
    CHECK(v.find("fabricated") == std::string::npos);
    CHECK(v.find("requested_mode") == std::string::npos);
}
TEST(S12_intention_change_not_false_past) {
    auto c = config();
    auto s = state(c);
    auto e = belief("plan", "a", "intended_repair");
    s.actors["a"].beliefs.push_back(e);
    World w(c, s);
    do_action(w, cmd("a", 1, "A19", Object {
        {
            "to", "b"
        }, {
            "statement", encode(e.statement)
        }, {
            "evidence", Array {
                "plan"
            }
        }
    }));
    w.run(60);
    CHECK(w.debug_state().actors.at("b").relations.at("a").trust == .5);
    CHECK(w.debug_state().items.at("target").condition < 1);
}
TEST(S13_deleted_post_preserves_read_version) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].beliefs.push_back(belief());
    World w(c, s);
    do_action(w, cmd("a", 1, "A28", Object {
        {
            "statement", encode(belief().statement)
        }, {
            "evidence", Array {
                "fact"
            }
        }, {
            "public", true
        }
    }), 2);
    auto rid = w.debug_state().records.begin()->first;
    do_action(w, cmd("b", 1, "A24", Object {
        {
            "record", rid
        }
    }));
    auto before = w.debug_state().actors.at("b").beliefs.size();
    do_action(w, cmd("a", 2, "A29", Object {
        {
            "record", rid
        }, {
            "version", 1
        }, {
            "delete", true
        }
    }));
    CHECK(w.debug_state().records.at(rid).versions.back().deleted);
    CHECK(w.debug_state().actors.at("b").beliefs.size() >= before);
    bool found = false;
    for (auto & e : w.debug_state().actors.at("b").beliefs) if (e.kind == "record" && e.statement.predicate == "at") found = true;
    CHECK(found);
}
TEST(S14_borrowed_item_is_usable) {
    auto c = config();
    auto s = state(c);
    s.profile = "extended";
    World w(c, s);
    auto p = propose(w, "b", {
        "a", "b"
    }, Object {
        {
            "kind", "loan_item"
        }, {
            "lender", "a"
        }, {
            "borrower", "b"
        }, {
            "item", "tool"
        }, {
            "due", 100
        }
    });
    accept(w, "a", p);
    do_action(w, cmd("a", 2, "A13", Object {
        {
            "mode", "loan_item"
        }, {
            "to", "b"
        }, {
            "item", "tool"
        }, {
            "proposal", p
        }
    }));
    auto q = cmd("b", 2, "A30", Object {
        {
            "skill", "repair"
        }, {
            "tool", "tool"
        }, {
            "minutes", 1
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "completed");
    CHECK(events(w, "unauthorized_transfer") == 0);
}
TEST(S15_breach_once_and_late_return) {
    auto c = config();
    World w(c, state(c));
    auto p = propose(w, "b", {
        "a", "b"
    }, Object {
        {
            "kind", "loan_item"
        }, {
            "lender", "a"
        }, {
            "borrower", "b"
        }, {
            "item", "tool"
        }, {
            "due", 10
        }, {
            "important", true
        }
    });
    accept(w, "a", p);
    do_action(w, cmd("a", 2, "A13", Object {
        {
            "mode", "loan_item"
        }, {
            "to", "b"
        }, {
            "item", "tool"
        }, {
            "proposal", p
        }
    }));
    w.run(10);
    NEAR(w.debug_state().actors.at("a").relations.at("b").trust, .4, 1e-12);
    CHECK(events(w, "obligation_breached") == 1);
    do_action(w, cmd("b", 2, "A13", Object {
        {
            "mode", "return"
        }, {
            "to", "a"
        }, {
            "item", "tool"
        }, {
            "obligation", p + "/obligation"
        }
    }));
    CHECK(w.debug_state().obligations.at(p + "/obligation").breached_at == 10);
    CHECK(w.debug_state().obligations.at(p + "/obligation").status == "fulfilled");
}
TEST(S16_private_adults_need_current_consent) {
    auto c = config();
    auto s = state(c);
    s.profile = "extended";
    s.actors["a"].position.place = "home";
    s.actors["b"].position.place = "home";
    for (auto id : {
        "a", "b"
    }) {
        Need n;
        n.enabled = true;
        n.critical = 5;
        n.activation = 30;
        n.target = 70;
        n.awake_drain = .6;
        n.sleep_drain = .1;
        s.actors[id].needs["N05"] = n;
    }
    World w(c, s);
    auto p = propose(w, "a", {
        "a", "b"
    }, Object {
        {
            "kind", "private"
        }, {
            "place", "home"
        }, {
            "start", 0
        }, {
            "end", 100
        }, {
            "minutes", 30
        }
    });
    accept(w, "b", p);
    start_joint(w, p, "A32");
    CHECK(! w.debug_state().actions.empty());
    auto old = w.debug_state().actors.at("b").needs.at("N05").value;
    do_action(w, cmd("b", 11, "A22", Object {
        {
            "proposal", p
        }
    }));
    CHECK(w.debug_state().actions.empty());
    NEAR(w.debug_state().actors.at("b").needs.at("N05").value, old - .6 / 60, 1e-9);
}
TEST(S17_minor_module_rejected) {
    auto c = config();
    auto s = state(c);
    s.profile = "extended";
    s.actors["b"].birth_day = - 365 * 16;
    Need n;
    n.critical = 5;
    n.activation = 30;
    n.target = 70;
    s.actors["b"].needs["N05"] = n;
    THROWS(World(c, s));
}
TEST(S18_partial_attention) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].relations["b"].affection = .5;
    World w(c, s);
    auto p = joint(w, {
        "a", "b"
    }, 10, Object {
        {
            "attention", Object {
                {
                    "a", .5
                }, {
                    "b", 1.0
                }
            }
        }
    });
    auto before = w.debug_state().actors.at("a").needs.at("N03").value;
    start_joint(w, p);
    NEAR(w.debug_state().actors.at("a").needs.at("N03").value, before - .025 + .2, 1e-9);
}
TEST(S19_proposal_default_expiry) {
    auto c = config();
    World w(c, state(c));
    auto q = cmd("a", 1, "A11", Object {
        {
            "to", "b"
        }, {
            "terms", Object {
                {
                    "kind", "loan_item"
                }, {
                    "lender", "b"
                }, {
                    "borrower", "a"
                }, {
                    "item", "unknown_tool"
                }, {
                    "due", 100
                }
            }
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "completed");
}
TEST(S20_digital_response_not_leaked_early) {
    auto c = config();
    World w(c, state(c));
    auto p = propose(w, "a", {
        "a", "b"
    }, Object {
        {
            "kind", "joint"
        }, {
            "place", "shop"
        }, {
            "start", 10
        }, {
            "end", 100
        }, {
            "minutes", 10
        }
    }, 1, "digital");
    w.advance();
    auto m = w.debug_state().actors.at("b").inbox.front();
    do_action(w, cmd("b", 1, "A24", Object {
        {
            "message", m
        }
    }));
    do_action(w, cmd("b", 2, "A12", Object {
        {
            "proposal", p
        }, {
            "version", 1
        }, {
            "answer", "decline"
        }, {
            "channel", "digital"
        }
    }));
    CHECK(w.debug_state().actors.at("a").outcome_counts.empty());
    w.advance();
    auto msg = w.debug_state().actors.at("a").inbox.back();
    do_action(w, cmd("a", 2, "A24", Object {
        {
            "message", msg
        }
    }));
    CHECK(w.debug_state().actors.at("a").outcome_counts.at("b/joint/declined") == 1);
}
