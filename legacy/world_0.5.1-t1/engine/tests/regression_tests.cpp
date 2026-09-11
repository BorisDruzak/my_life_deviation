#include "fixtures.hpp"
using namespace testdata;
TEST(R01_cook_resume_consumes_once) {
    auto c = config();
    World w(c, state(c));
    do_action(w, cmd("a", 1, "A08", Object {
        {
            "recipe", "R01"
        }, {
            "fixture", "kitchen"
        }, {
            "materials", Array {
                "ingredients"
            }
        }
    }), 7);
    auto id = w.debug_state().jobs.begin()->first;
    w.submit(cmd("a", 2, "A22"));
    w.submit(cmd("a", 3, "A08", Object {
        {
            "job", id
        }
    }));
    w.run(13);
    w.check_invariants();
    CHECK(w.debug_state().jobs.at(id).outputs.size() == 1);
    CHECK(w.debug_state().items.at("ingredients").placement.kind == "tombstone");
    CHECK(w.debug_state().items.at(id + "/output/0").remaining_units == 15);
    CHECK(events(w, "job_completed") == 1);
}
TEST(R02_rope_becomes_installed_not_duplicated) {
    auto c = config();
    World w(c, state(c));
    do_action(w, cmd("a", 1, "A21", Object {
        {
            "recipe", "R03"
        }, {
            "target", "cargo"
        }, {
            "tools", Array {
                "rope"
            }
        }, {
            "materials", Array {}
        }
    }), 10);
    CHECK(w.debug_state().items.at("cargo").secured);
    CHECK(w.debug_state().items.at("rope").placement.kind == "installed");
    CHECK(w.debug_state().items.at("rope").owner == "a");
}
TEST(R03_continuous_leisure_split_invariant) {
    auto c = config();
    World a(c, state(c)), b(c, state(c));
    do_action(a, cmd("a", 1, "A16", Object {
        {
            "item", "book"
        }, {
            "minutes", 30
        }
    }), 30);
    for (int i = 1; i <= 3; ++ i) do_action(b, cmd("a", i, "A16", Object {
        {
            "item", "book"
        }, {
            "minutes", 10
        }
    }), 10);
    NEAR(a.debug_state().actors.at("a").needs.at("N04").value, b.debug_state().actors.at("a").needs.at("N04").value,
    1e-12);
    NEAR(a.debug_state().actors.at("a").repetition.at("reading"), b.debug_state().actors.at("a").repetition.at("reading"),
    1e-12);
}
TEST(R04_restores_pending_commands) {
    auto c = config();
    World a(c, state(c));
    a.submit(cmd("a", 1, "A06", Object {
        {
            "shop", "store"
        }, {
            "item", "meal"
        }
    }));
    auto b = World::restore(c, a.snapshot());
    a.run(5);
    b.run(5);
    CHECK(canonical(a.snapshot()) == canonical(b.snapshot()));
}
TEST(R05_mutated_snapshot_rejected) {
    auto c = config();
    World w(c, state(c));
    auto snap = w.snapshot();
    snap.as_object()["state"].as_object()["accounts"].as_object()["a"] = 999;
    THROWS(World::restore(c, snap));
}
TEST(R06_bad_type_does_not_poison_world) {
    auto c = config();
    World w(c, state(c));
    auto before = canonical(w.snapshot());
    THROWS(w.submit(cmd("a", 1, "A12", Object {
        {
            "proposal", 12
        }, {
            "version", 1
        }, {
            "answer", "accept"
        }
    })));
    CHECK(canonical(w.snapshot()) == before);
    w.advance();
}
TEST(R07_loan_foreign_owner_rejected) {
    auto c = config();
    auto s = state(c);
    s.items["tool"].owner = "c";
    Proposal p;
    p.id = "p";
    p.author = "a";
    p.participants = {
        "a", "b"
    };
    p.signatures = p.participants;
    p.state = "accepted";
    p.expires = 100;
    p.terms = Object {
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
    };
    s.proposals[p.id] = p;
    World w(c, s);
    auto q = cmd("a", 1, "A13", Object {
        {
            "mode", "loan_item"
        }, {
            "to", "b"
        }, {
            "item", "tool"
        }, {
            "proposal", "p"
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().items.at("tool").placement.ref == "a");
}
TEST(R08_invalid_cancel_does_not_withdraw_unrelated_contract) {
    auto c = config();
    auto s = state(c);
    Proposal p;
    p.id = "p";
    p.author = "a";
    p.participants = {
        "a", "b"
    };
    p.signatures = p.participants;
    p.state = "accepted";
    p.expires = 100;
    p.terms = Object {
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
    };
    s.proposals[p.id] = p;
    World w(c, s);
    do_action(w, cmd("a", 1, "A02", Object {
        {
            "edge", "shop_home"
        }
    }));
    do_action(w, cmd("a", 2, "A22", Object {
        {
            "proposal", "p"
        }
    }));
    CHECK(w.debug_state().proposals.at("p").state == "accepted");
    CHECK(w.debug_state().proposals.at("p").signatures.contains("a"));
}
TEST(R09_hypotheses_require_own_roots) {
    auto c = config();
    World w(c, state(c));
    Evidence e;
    e.id = "h";
    e.kind = "hypothesis";
    e.roots = {
        "not_known"
    };
    e.statement = {
        "a", "threat", Object {}, true, 0, 100
    };
    e.confidence = .8;
    e.severity = .8;
    THROWS(w.hypothesize("a", e));
}
TEST(R10_wellbeing_initializes_from_account) {
    auto c = config();
    Json raw = encode(state(c));
    raw.as_object()["actors"].as_object()["a"].as_object().erase("wellbeing");
    auto s = scenario_state(c, raw);
    NEAR(s.actors.at("a").wellbeing, 80, 1e-12);
}
TEST(R11_recipe_duplicate_tools_rejected) {
    auto c = config();
    World w(c, state(c));
    auto q = cmd("a", 1, "A21", Object {
        {
            "recipe", "R02"
        }, {
            "target", "target"
        }, {
            "tools", Array {
                "tool", "tool"
            }
        }, {
            "materials", Array {
                "kit"
            }
        }
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().jobs.empty());
}
TEST(R12_stranded_progress_survives_injury) {
    auto c = config();
    auto s = state(c);
    External e;
    e.id = "hurt";
    e.kind = "injury";
    e.at = 4;
    e.args = Object {
        {
            "actor", "a"
        }, {
            "amount", .95
        }
    };
    s.external.push_back(e);
    World w(c, s);
    do_action(w, cmd("a", 1, "A02", Object {
        {
            "edge", "shop_home"
        }
    }), 4);
    CHECK(w.debug_state().actors.at("a").position.kind == "stranded");
    NEAR(w.debug_state().actors.at("a").position.progress, .4, 1e-12);
    CHECK(w.debug_state().actors.at("a").position.place.empty());
    auto restored = World::restore(c, w.snapshot());
    CHECK(restored.debug_state().actors.at("a").position.kind == "stranded");
}
TEST(R13_digital_unrelated_messages_do_not_change_visible_ids) {
    auto c = config();
    auto s1 = state(c), s2 = s1;
    Message m;
    m.id = "hidden";
    m.author = "a";
    m.recipients = {
        "c"
    };
    m.content = Object {
        {
            "type", "hidden"
        }
    };
    m.delivered = true;
    s2.messages[m.id] = m;
    World a(c, s1), b(c, s2);
    Statement st {
        "a", "at", Object {
            {
                "place", "shop"
            }
        }, true, 0, - 1
    };
    auto q = cmd("a", 1, "A19", Object {
        {
            "to", "b"
        }, {
            "statement", encode(st)
        }, {
            "fabricated", true
        }
    });
    do_action(a, q);
    do_action(b, q);
    CHECK(canonical(a.view("b")) == canonical(b.view("b")));
}
TEST(R14_no_internal_theft_classification_for_witness) {
    auto c = config();
    auto s = state(c);
    s.items["tool"].placement = {
        "at", "shop"
    };
    World w(c, s);
    do_action(w, cmd("b", 1, "A20", item_args("tool")));
    Array observed;
    for (const auto & e : w.debug_state().actors.at("c").beliefs) if (e.kind == "observation") observed.push_back(encode(e));
    auto v = canonical(observed);
    CHECK(v.find("unauthorized_transfer") == std::string::npos);
    CHECK(v.find("A20") == std::string::npos);
    CHECK(v.find("item_moved") != std::string::npos);
}
TEST(R15_state_rejects_unknown_external_edge) {
    auto c = config();
    auto s = state(c);
    External e;
    e.id = "bad";
    e.at = 1;
    e.kind = "edge";
    e.args = Object {
        {
            "edge", "missing"
        }, {
            "open", false
        }
    };
    s.external.push_back(e);
    THROWS(World(c, s));
}
TEST(R16_full_480min_sleep) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N02"].value = 15;
    s.actors["a"].needs["N01"].value = 60;
    World w(c, s);
    do_action(w, cmd("a", 1, "A09", Object {
        {
            "bed", "bed"
        }, {
            "minutes", 480
        }
    }), 480);
    NEAR(w.debug_state().actors.at("a").needs.at("N02").value, 95, 1e-9);
    NEAR(w.debug_state().actors.at("a").needs.at("N01").value, 47.2, 1e-9);
}
TEST(R17_exact_need_hysteresis) {
    auto c = config();
    auto s = state(c);
    auto & n = s.actors["a"].needs["N01"];
    n.value = 45;
    n.awake_drain = 0;
    World a(c, s);
    CHECK(a.debug_state().actors.at("a").needs.at("N01").active);
    auto snap = a.snapshot();
    auto st = decode < State >(at(snap, "state"));
    st.actors["a"].needs["N01"].value = 60;
    World b(c, st);
    b.advance();
    CHECK(b.debug_state().actors.at("a").needs.at("N01").active);
    st.actors["a"].needs["N01"].value = 80;
    World d(c, st);
    d.advance();
    CHECK(! d.debug_state().actors.at("a").needs.at("N01").active);
}
TEST(R18_practice_profile_gate) {
    auto c = config();
    World a(c, state(c));
    auto q = cmd("a", 1, "A30", Object {
        {
            "tool", "tool"
        }, {
            "skill", "repair"
        }, {
            "minutes", 10
        }
    });
    do_action(a, q);
    CHECK(receipt(a, q).status == "rejected");
    auto s = state(c);
    s.profile = "extended";
    World b(c, s);
    do_action(b, q, 10);
    CHECK(receipt(b, q).status == "completed");
    CHECK(b.debug_state().actors.at("a").skills.at("repair") > .5);
}
TEST(R19_no_repeated_appropriation_reward) {
    auto c = config();
    auto s = state(c);
    s.items["tool"].placement = {
        "at", "shop"
    };
    s.actors["b"].drives["appropriation"] = {
        1, 1
    };
    World w(c, s);
    do_action(w, cmd("b", 1, "A20", item_args("tool")));
    auto d = w.debug_state().actors.at("b").drives.at("appropriation").pressure;
    auto q = cmd("b", 2, "A20", item_args("tool"));
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().actors.at("b").drives.at("appropriation").pressure > d);
    CHECK(events(w, "unauthorized_transfer") == 1);
}
TEST(R20_unknown_record_acl_reader_rejected) {
    auto c = config();
    auto s = state(c);
    Record r;
    r.id = "r";
    r.author = "a";
    RecordVersion v;
    v.acl = {
        "ghost"
    };
    r.versions.push_back(v);
    s.records[r.id] = r;
    THROWS(World(c, s));
}
TEST(R21_rule_domain_validation) {
    auto c = config();
    auto p = c.parameters;
    p.as_object()["physical"].as_object()["fatigue_modes"].as_object()["walk"] = Array {
        - 1, 0
    };
    THROWS(Config::from_json(p, c.catalog));
}
TEST(R22_opaque_default_is_closed) {
    auto c = config();
    auto box = make_item("box", "I12", "a", {
        "at", "shop"
    }, c);
    CHECK(! box.open);
}
TEST(R23_duplicate_json_keys_rejected) {
    THROWS(parse_json("{\"x\":1,\"x\":2}"));
    THROWS(parse_json("{\"nested\":{\"a\":1,\"\\u0061\":2}}"));
}
TEST(R24_exact_placement_capacity_rechecked) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].relations["b"].affection = .5;
    s.items["box"].open = true;
    World w(c, s);
    do_action(w, cmd("a", 1, "A05", Object {
        {
            "item", "tool"
        }, {
            "container", "box"
        }
    }));
    CHECK(w.debug_state().items.at("tool").placement.ref == "box");
    do_action(w, cmd("a", 2, "A26", Object {
        {
            "item", "box"
        }, {
            "open", false
        }
    }));
    auto q = cmd("a", 3, "A04", item_args("tool"));
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
}
TEST(R25_false_claim_appraisal_is_cause_deduplicated) {
    auto c = config();
    auto s = state(c);
    Evidence e;
    e.id = "source";
    e.source = "genesis";
    e.roots = {
        "sharedroot"
    };
    e.statement = {
        "tool", "at", Object {
            {
                "place", "shop"
            }
        }, true, 0, 0
    };
    s.actors["a"].beliefs.push_back(e);
    auto contrary = e;
    contrary.id = "contra";
    contrary.roots = {
        "independent"
    };
    contrary.statement.polarity = false;
    s.actors["b"].beliefs.push_back(contrary);
    World w(c, s);
    for (int i = 1; i <= 3; ++ i) do_action(w, cmd("a", i, "A19", Object {
        {
            "to", "b"
        }, {
            "statement", encode(e.statement)
        }, {
            "evidence", Array {
                "source"
            }
        }
    }));
    NEAR(w.debug_state().actors.at("b").relations.at("a").trust, .46, 1e-12);
}
TEST(R26_decimal_roundtrip_preserves_snapshot_hash) {
    auto c = config();
    World w(c, state(c));
    w.run(17);
    auto s = w.snapshot();
    auto disk = parse_json(canonical(s));
    CHECK(canonical(s) == canonical(disk));
    auto restored = World::restore(c, disk);
    CHECK(canonical(restored.snapshot()) == canonical(s));
}
TEST(R27_revoke_overrides_loan_use_not_physical_return) {
    auto c = config();
    auto s = state(c);
    s.items["tool"].placement = {
        "inventory", "b"
    };
    Obligation o;
    o.id = "loan";
    o.kind = "return_item";
    o.item = "tool";
    o.debtor = "b";
    o.creditor = "a";
    o.due = 100;
    o.root = "loan-root";
    s.obligations[o.id] = o;
    World w(c, s);
    do_action(w, cmd("a", 1, "A25", Object {
        {
            "resource", "tool"
        }, {
            "grantee", "b"
        }, {
            "grant", false
        }, {
            "version", 1
        }
    }));
    auto q = cmd("b", 1, "A21", Object {
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
    });
    do_action(w, q);
    CHECK(receipt(w, q).status == "rejected");
    // a separate pickup permission test avoids a false positive from target/material ownership
    auto snapshot = decode < State >(at(w.snapshot(), "state"));
    snapshot.items["tool"].placement = {
        "at", "shop"
    };
    World x(c, snapshot);
    auto pickup = cmd("b", 2, "A04", item_args("tool"));
    do_action(x, pickup);
    CHECK(receipt(x, pickup).status == "rejected");
}
TEST(R28_scripted_move_validated_at_current_completion_boundary) {
    auto c = config();
    auto s = state(c);
    External e;
    e.id = "move";
    e.kind = "move_item";
    e.at = 1;
    e.args = Object {
        {
            "item", "book"
        }, {
            "placement", encode(Placement {
                "at", "park"
            })
        }
    };
    s.external.push_back(e);
    World w(c, s);
    w.submit(cmd("a", 1, "A03", item_args("tool")));
    w.advance();
    w.check_invariants();
    CHECK(w.debug_state().items.at("book").placement.ref == "park");
}
TEST(R29_record_edit_unknown_acl_is_rejected_before_commit) {
    auto c = config();
    auto s = state(c);
    Record r;
    r.id = "r";
    r.author = "a";
    RecordVersion v;
    v.acl = {
        "a"
    };
    v.content = Object {
        {
            "text", "old"
        }
    };
    r.versions.push_back(v);
    s.records[r.id] = r;
    World w(c, s);
    Statement st {
        "a", "at", Object {
            {
                "place", "shop"
            }
        }, true, 0, 0
    };
    auto q = cmd("a", 1, "A29", Object {
        {
            "record", "r"
        }, {
            "version", 1
        }, {
            "statement", encode(st)
        }, {
            "acl", Array {
                "ghost"
            }
        }
    });
    w.submit(q);
    w.run(2);
    CHECK(receipt(w, q).status == "rejected");
    CHECK(w.debug_state().records.at("r").versions.size() == 1);
    w.check_invariants();
}
TEST(R30_signals_reach_owner_memory_once) {
    auto c = config();
    auto s = state(c);
    s.actors["a"].needs["N01"].value = 45.01;
    World w(c, s);
    w.advance();
    std::size_t count = 0;
    for (const auto & b : w.debug_state().actors.at("a").beliefs) if (b.statement.predicate == "event" && text(b.statement.arguments,
    "type") == "need_threshold") ++ count;
    CHECK(count == 1);
    w.advance();
    std::size_t after = 0;
    for (const auto & b : w.debug_state().actors.at("a").beliefs) if (b.statement.predicate == "event" && text(b.statement.arguments,
    "type") == "need_threshold") ++ after;
    CHECK(after == 1);
}
TEST(R31_hidden_completion_does_not_change_creditor_reaction) {
    auto c = config();
    auto s = state(c);
    s.actors["c"].position.place = "park";
    Obligation o;
    o.id = "repair-promise";
    o.kind = "repair";
    o.target = "target";
    o.debtor = "a";
    o.creditor = "c";
    o.due = 1;
    o.root = "repair-root";
    o.acknowledged = {
        "a", "c"
    };
    s.obligations[o.id] = o;
    auto done = s;
    done.obligations[o.id].status = "fulfilled";
    done.obligations[o.id].fulfilled_at = 0;
    World a(c, s), b(c, done);
    a.advance();
    b.advance();
    CHECK(canonical(a.view("c")) == canonical(b.view("c")));
}
TEST(R32_unknown_scripted_shipment_is_rejected_on_load) {
    auto c = config();
    auto s = state(c);
    External e;
    e.id = "ship";
    e.kind = "shipment";
    e.at = 100;
    e.args = Object {
        {
            "shop", "missing"
        }, {
            "item", "newmeal"
        }, {
            "item_type", "I01"
        }, {
            "price", 30
        }
    };
    s.external.push_back(e);
    THROWS(World(c, s));
}
