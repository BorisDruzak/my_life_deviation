#include "fixtures.hpp"
#include "npc/controller.hpp"
#include "npc/policy/knowledge_query.hpp"
#include <type_traits>

using namespace testdata;
namespace pol = npc::policy;
namespace {
const std::string policy_hash = math::sha256("T1-test-policy-configuration");

template<class F> void input_error(F fn) {
    bool caught = false;
    try { fn(); } catch (const InputError&) { caught = true; }
    CHECK(caught);
}
Evidence fact(Id id, bool polarity = true, Minute from = 0, Minute until = -1) {
    Evidence e;
    e.id = std::move(id); e.source = "genesis"; e.roots = {e.id};
    e.statement = {"store", "open", Object{{"place", "shop"}}, polarity, from, until};
    return e;
}
pol::BeliefQuery open_query(Minute at = 0) {
    return {"store", "open", Object{{"place", "shop"}}, at, true};
}
pol::LocalView read_page(World& w, pol::ControllerMemory& m, std::size_t count = 64,
                         const std::vector<Id>& watched = {}) {
    return pol::apply_delta(m, w.view_delta(m.state().actor, m.cursor(), count, watched));
}
pol::LocalView read_all(World& w, pol::ControllerMemory& m) {
    auto v = read_page(w, m);
    while (!v.history_complete()) v = read_page(w, m);
    return v;
}
World knowledge_world(const std::vector<Evidence>& evidence, Minute now = 0) {
    auto c = config(); auto s = state(c); s.time = now;
    s.actors["a"].beliefs = evidence;
    return World(c, s);
}
Evidence signed_repair(const Id& debtor = "b", const Id& creditor = "a") {
    Evidence e = fact("signed-repair");
    e.source = "b"; e.kind = "claim"; e.roots = {"b@1/proposal"};
    e.statement = {"b", "proposal_update", Object{
        {"type", "proposal_update"}, {"proposal", "b@1/proposal"}, {"version", 1},
        {"state", "accepted"}, {"author", "b"}, {"participants", Array{"a", "b"}},
        {"signatures", Array{"a", "b"}}, {"expires", 80}, {"answer", "accept"},
        {"terms", Object{{"kind", "repair"}, {"debtor", debtor}, {"creditor", creditor},
                         {"target", "target"}, {"due", 100}, {"important", true}}}
    }, true, 0, 0};
    return e;
}
pol::Goal waiting_goal() {
    pol::Goal g;
    g.id = "a/Need/N03/1"; g.actor = "a"; g.family = "Need";
    g.target = {"need", "N03", 0, Object{}}; g.source_root = "need/N03";
    g.activation_reason = "need_active"; g.desired_predicate = Object{{"at_least", 75}};
    g.status = "waiting_external"; g.next_review_at = 25; g.wake_trigger = "reply";
    return g;
}
void rehash(Json& j) { j.as_object()["state_hash"] = math::sha256(canonical(at(j, "state"))); }
}

TEST(PG_T1_P03_hidden_state_does_not_change_local_inputs_or_legacy_decision) {
    auto c = config(); auto x = state(c); auto y = x;
    y.actors["b"].needs["N01"].value = 11;
    y.actors["b"].relations["a"].trust = .03;
    y.actors["c"].relations["b"].affection = .95;
    y.items["phone_b"].condition = .1;
    World wx(c, x), wy(c, y);
    pol::ControllerMemory mx("a", c.hash, policy_hash), my("a", c.hash, policy_hash);
    const auto vx = read_all(wx, mx), vy = read_all(wy, my);
    CHECK(canonical(vx.export_json()) == canonical(vy.export_json()));
    CHECK(canonical(mx.snapshot()) == canonical(my.snapshot()));
    CHECK(canonical(wx.view("a")) == canonical(wy.view("a")));
    CHECK(canonical(npc::describe(decide(c, wx.view("a")))) ==
          canonical(npc::describe(decide(c, wy.view("a")))));
    static_assert(!std::is_constructible_v<pol::LocalView, World&>);
    static_assert(!std::is_constructible_v<pol::LocalView, State&>);
    static_assert(std::is_same_v<decltype(vx.own()), const OwnState&>);
}
TEST(PG_T1_projection_pages_are_bounded_and_resumable) {
    std::vector<Evidence> es;
    for (int i = 0; i < 145; ++i) es.push_back(fact("fact-" + std::to_string(i)));
    auto w = knowledge_world(es); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    const auto before = canonical(w.snapshot());
    const auto p1 = w.view_delta("a", m.cursor());
    CHECK(p1.evidence.size() == 64); CHECK(p1.records_examined == 64); CHECK(p1.has_more);
    auto first = pol::apply_delta(m, p1);
    CHECK(first.evidence_count() == 64);
    const auto p2 = w.view_delta("a", m.cursor());
    CHECK(p2.evidence.size() == 64); pol::apply_delta(m, p2);
    const auto p3 = w.view_delta("a", m.cursor());
    CHECK(p3.evidence.size() == 17); CHECK(!p3.has_more); pol::apply_delta(m, p3);
    const auto p4 = w.view_delta("a", m.cursor());
    CHECK(p4.evidence.empty()); CHECK(p4.records_examined == 0);
    CHECK(m.cursor().offset == 145);
    CHECK(canonical(w.snapshot()) == before);
    auto resumed = World::restore(w.config(), w.snapshot());
    CHECK(canonical(encode(resumed.view_delta("a", m.cursor()))) == canonical(encode(p4)));
    CHECK(first.evidence_count() == 64); CHECK(first.evidence("fact-144") == nullptr);
}
TEST(PG_T1_projection_rejects_foreign_forged_and_oversized_cursors) {
    auto w = knowledge_world({fact("e")}); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto c = m.cursor(); c.actor = "b";
    input_error([&] { w.view_delta("a", c); });
    c = m.cursor(); c.offset = 99;
    input_error([&] { w.view_delta("a", c); });
    input_error([&] { w.view_delta("a", m.cursor(), 0); });
    input_error([&] { w.view_delta("a", m.cursor(), 65); });
    input_error([&] { w.view_delta("a", m.cursor(), 1, {"a@1","a@2","a@3","a@4","a@5"}); });
    read_all(w, m); c = m.cursor(); c.prefix_hash = std::string(64, '0');
    input_error([&] { w.view_delta("a", c); });
}
TEST(PG_T1_out_of_order_delta_is_atomic_and_forks_are_independent) {
    auto w = knowledge_world({fact("one"), fact("two")});
    pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto page = w.view_delta("a", m.cursor(), 1); pol::apply_delta(m, page);
    const auto checkpoint = canonical(m.snapshot());
    input_error([&] { pol::apply_delta(m, page); });
    CHECK(canonical(m.snapshot()) == checkpoint);
    auto fork = m;
    auto old = read_page(w, fork);
    CHECK(old.evidence_count() == 2); CHECK(m.cursor().offset == 1);
}
TEST(PG_T1_current_receipt_bypasses_history_backlog_without_private_fields) {
    std::vector<Evidence> es;
    for (int i = 0; i < 200; ++i) es.push_back(fact("old-" + std::to_string(i)));
    auto w = knowledge_world(es);
    do_action(w, cmd("a", 1, "A01", Object{{"minutes", 1}}));
    pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto d = w.view_delta("a", m.cursor(), 1, {"a@1", "b@1", "a@999"});
    CHECK(d.evidence.size() == 1); CHECK(d.receipts.size() == 1);
    CHECK(d.receipts[0].status == "completed");
    CHECK(!has(encode(d.receipts[0]), "debug_reason"));
    CHECK(!has(encode(d.receipts[0]), "fingerprint"));
    CHECK(!has(encode(d.own), "beliefs")); CHECK(!has(encode(d.own), "relations"));
    auto v = pol::apply_delta(m, d);
    CHECK(v.receipt("a@1") && v.receipt("a@1")->status == "completed");
}
TEST(PG_T1_digital_delivery_does_not_reveal_unread_terms) {
    auto c = config(); World w(c, state(c));
    do_action(w, cmd("a", 1, "A11", Object{{"to", "b"}, {"channel", "digital"}, {"expires", 80},
        {"terms", Object{{"kind", "repair"}, {"debtor", "b"}, {"creditor", "a"},
                         {"target", "target"}, {"due", 100}}}}), 2);
    pol::ControllerMemory m("b", c.hash, policy_hash);
    auto before = read_all(w, m);
    CHECK(before.commitment("a@1/proposal", 1) == nullptr);
    CHECK(canonical(before.export_json()).find("proposal_update") == std::string::npos);
    // Reading reveals the offer, not a signed obligation.
    do_action(w, cmd("b", 1, "A24", Object{{"message", "a@1/message"}}));
    auto after = read_all(w, m);
    CHECK(after.commitment("a@1/proposal", 1) == nullptr);
}
TEST(PG_T1_truth_distinguishes_true_false_unknown_and_query_polarity) {
    for (bool positive : {false, true}) {
        auto w = knowledge_world({fact("one", positive)});
        pol::ControllerMemory m("a", w.config().hash, policy_hash); auto v = read_all(w, m);
        auto q = open_query(); auto r = pol::query_belief(v, q);
        CHECK(r.truth == (positive ? pol::Truth::KnownTrue : pol::Truth::KnownFalse));
        CHECK(r.sources.size() == 1); CHECK(r.sources[0].source == "genesis");
        q.polarity = false;
        CHECK(pol::query_belief(v, q).truth == (positive ? pol::Truth::KnownFalse : pol::Truth::KnownTrue));
        q.subject = "unknown_store";
        CHECK(pol::query_belief(v, q).truth == pol::Truth::Unknown);
    }
}
TEST(PG_T1_conflict_is_unknown_regardless_of_insertion_order) {
    auto e1 = fact("yes"); auto e2 = fact("no", false); e2.confidence = .8;
    for (auto es : {std::vector<Evidence>{e1,e2}, std::vector<Evidence>{e2,e1}}) {
        auto w = knowledge_world(es); pol::ControllerMemory m("a", w.config().hash, policy_hash);
        auto r = pol::query_belief(read_all(w, m), open_query());
        CHECK(r.truth == pol::Truth::Unknown); CHECK(r.reason == "conflicting_sources");
        NEAR(r.confidence, .5, 1e-12); CHECK(r.sources.size() == 2);
    }
}
TEST(PG_T1_correlated_sources_do_not_accumulate_certainty) {
    std::vector<Evidence> es;
    for (int i = 0; i < 15; ++i) {
        auto e = fact("echo-"+std::to_string(i)); e.roots = {"same-root"}; e.confidence = .7;
        es.push_back(e);
    }
    auto w = knowledge_world(es); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto r = pol::query_belief(read_all(w, m), open_query());
    CHECK(r.truth == pol::Truth::Unknown); NEAR(r.confidence, .7, 1e-12);
}
TEST(PG_T1_temporal_boundaries_and_history_are_not_rewritten) {
    auto e = fact("historical", true, 5, 10); e.learned_at = 5;
    auto w = knowledge_world({e}, 20); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto v = read_all(w, m);
    CHECK(pol::query_belief(v, open_query(4)).truth == pol::Truth::Unknown);
    CHECK(pol::query_belief(v, open_query(5)).truth == pol::Truth::KnownTrue);
    CHECK(pol::query_belief(v, open_query(10)).truth == pol::Truth::KnownTrue);
    CHECK(pol::query_belief(v, open_query(11)).truth == pol::Truth::Unknown);
    CHECK(canonical(encode(*v.evidence("historical"))) == canonical(encode(e)));
}
TEST(PG_T1_P13_legacy_open_ended_stock_is_not_current_availability) {
    auto e = fact("old-stock"); e.statement.predicate = "shop_stock";
    auto w = knowledge_world({e}, 120); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto v = read_all(w, m); auto q = open_query(120); q.predicate = "shop_stock";
    auto r = pol::query_belief(v, q);
    CHECK(r.truth == pol::Truth::Unknown); CHECK(r.reason == "expired_evidence");
    CHECK(v.evidence("old-stock")->statement.valid_until == -1);
}
TEST(PG_T1_persistence_is_explicit_decaying_and_separate_from_observation) {
    auto e = fact("old", true, 0, 0);
    auto w = knowledge_world({e}, 30); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto v = read_all(w, m);
    CHECK(pol::query_belief(v, open_query(30)).truth == pol::Truth::Unknown);
    auto r = pol::query_belief(v, open_query(30), pol::PersistenceRule{"open", 60, .5});
    CHECK(r.truth == pol::Truth::KnownTrue); CHECK(r.reason == "persistence_assumption");
    CHECK(r.sources[0].assumed); CHECK(r.sources[0].observed_at == 0);
    NEAR(r.confidence, .5 + .5*std::pow(2.0, -.5), 1e-12);
    CHECK(v.evidence("old")->statement.valid_until == 0);
    auto late = knowledge_world({e}, 120); pol::ControllerMemory ml("a", late.config().hash, policy_hash);
    CHECK(pol::query_belief(read_all(late, ml), open_query(120), pol::PersistenceRule{"open",60,.5}).truth == pol::Truth::Unknown);
}
TEST(PG_T1_query_and_unprocessed_history_budgets_do_not_fabricate_certainty) {
    std::vector<Evidence> es;
    for (int i=0;i<65;++i) es.push_back(fact("e"+std::to_string(i)));
    auto w = knowledge_world(es); pol::ControllerMemory m("a", w.config().hash, policy_hash);
    auto partial = read_page(w, m);
    auto r = pol::query_belief(partial, open_query());
    CHECK(r.truth == pol::Truth::Unknown); CHECK(!r.complete);
    auto full = read_all(w, m); r = pol::query_belief(full, open_query());
    CHECK(r.examined == 64); CHECK(!r.complete); CHECK(r.reason == "query_budget_exhausted");
    CHECK(r.truth == pol::Truth::Unknown);
    input_error([&] { pol::query_belief(full, open_query(-1)); });
    input_error([&] { pol::query_belief(full, open_query(), {}, 0); });
    input_error([&] { pol::query_belief(full, open_query(), pol::PersistenceRule{"other",60,.5}); });
}
TEST(PG_T1_G09_commitment_uses_signed_evidence_not_global_status) {
    auto c = config(); auto s = state(c); s.actors["a"].beliefs = {signed_repair()};
    Obligation o; o.id = "b@1/proposal/obligation"; o.root = "b@1/proposal";
    o.debtor = "b"; o.creditor = "a"; o.kind = "repair"; o.target = "target"; o.due = 100;
    s.obligations[o.id] = o; auto changed = s;
    changed.obligations[o.id].status = "fulfilled"; changed.obligations[o.id].fulfilled_at = 0;
    World w(c,s), hidden(c,changed);
    pol::ControllerMemory m("a",c.hash,policy_hash), mh("a",c.hash,policy_hash);
    auto v = read_all(w,m), vh = read_all(hidden,mh);
    CHECK(v.commitment("b@1/proposal",1));
    CHECK(v.commitment("b@1/proposal",1)->status == "pending");
    CHECK(canonical(v.export_json()) == canonical(vh.export_json()));
    CHECK(v.nearest_commitments().size() == 1);
    CHECK(v.nearest_commitments()[0].due == 100);
}
TEST(PG_T1_unsigned_foreign_or_conflicting_terms_are_not_silently_accepted) {
    auto e = signed_repair(); at(e.statement.arguments,"signatures");
    e.statement.arguments.as_object()["signatures"] = Array{"b"};
    auto w = knowledge_world({e}); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    CHECK(read_all(w,m).commitment("b@1/proposal",1) == nullptr);
    auto one = signed_repair(), two = one; two.id = "conflicting-terms";
    two.statement.arguments.as_object()["terms"].as_object()["due"] = 150;
    auto conflict = knowledge_world({one,two}); pol::ControllerMemory mc("a",w.config().hash,policy_hash);
    auto v=read_all(conflict,mc); CHECK(v.commitment("b@1/proposal",1));
    CHECK(v.commitment("b@1/proposal",1)->status == "unknown");
}
TEST(PG_T1_visible_completion_changes_known_commitment_only_after_ingest) {
    auto one = signed_repair("a","b");
    auto done = fact("a@5/event/3"); done.source = "a"; done.learned_at = 1;
    done.statement = {"a","event",Object{{"id","a@5/event/3"},{"type","obligation_fulfilled"},
        {"data",Object{{"obligation","b@1/proposal/obligation"},{"debtor","a"},{"creditor","b"}}}},true,1,1};
    auto w = knowledge_world({one,done},1); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    auto before=read_page(w,m,1); CHECK(before.commitment("b@1/proposal",1)->status == "pending");
    auto after=read_page(w,m,1); CHECK(after.commitment("b@1/proposal",1)->status == "fulfilled");
    CHECK(after.nearest_commitments().empty());
    CHECK(before.commitment("b@1/proposal",1)->status == "pending");
}
TEST(PG_T1_G12_memory_roundtrip_preserves_waiting_state_and_next_sequence) {
    auto w=knowledge_world({fact("first"),fact("second")});
    pol::ControllerMemory m("a",w.config().hash,policy_hash); read_page(w,m,1);
    auto& s=m.edit_state(); s.decision_sequence=12; s.goals={waiting_goal()};
    s.goal_episode_counters["need/N03"]=1; s.next_regular_review_tick=25;
    s.cooldowns["a|b|conversation"]=120; s.fair_scan_cursor["partners"]=3;
    s.session_runtimes.push_back({"session-1","a@12/proposal",1,"waiting_response",0,{},0,{"first"}});
    s.pending_command_links.push_back({"a@12",s.goals[0].id,"","session-1",0});
    s.pending_notifications.push_back({"notice-1","reply","first",0});
    auto checkpoint=m.snapshot(); auto restored=pol::ControllerMemory::restore(checkpoint,w.config().hash,policy_hash);
    CHECK(canonical(restored.snapshot()) == canonical(checkpoint));
    auto w2=World::restore(w.config(),w.snapshot());
    auto live=read_all(w,m), resumed=read_all(w2,restored);
    CHECK(canonical(live.export_json()) == canonical(resumed.export_json()));
    CHECK(canonical(m.snapshot()) == canonical(restored.snapshot()));
    CHECK(++m.edit_state().decision_sequence == ++restored.edit_state().decision_sequence);
    CHECK(m.state().goals[0].id == restored.state().goals[0].id);
    CHECK(m.state().next_regular_review_tick == restored.state().next_regular_review_tick);
}
TEST(PG_T1_memory_validates_versions_hashes_and_checksum) {
    auto w=knowledge_world({}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto j=m.snapshot();
    input_error([&]{ pol::ControllerMemory::restore(j,w.config().hash,math::sha256("other")); });
    input_error([&]{ pol::ControllerMemory::restore(j,math::sha256("other"),policy_hash); });
    auto bad=j; bad.as_object()["version"]="future";
    input_error([&]{ pol::ControllerMemory::restore(bad,w.config().hash,policy_hash); });
    bad=j; bad.as_object()["state_hash"]=std::string(64,'0');
    input_error([&]{ pol::ControllerMemory::restore(bad,w.config().hash,policy_hash); });
}
TEST(PG_T1_memory_rejects_unknown_goal_status_negative_ticks_and_duplicate_keys) {
    auto w=knowledge_world({}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    m.edit_state().goals={waiting_goal()}; auto base=m.snapshot();
    for (int mode=0;mode<3;++mode) {
        auto bad=base; auto& control=bad.as_object()["state"].as_object()["control"].as_object();
        if(mode==0) control["goals"].as_array()[0].as_object()["status"]="teleported";
        if(mode==1) control["next_regular_review_tick"]=-1;
        if(mode==2) {
            auto duplicate=control["goals"].as_array()[0]; duplicate.as_object()["id"]="different-id-same-key";
            control["goals"].as_array().push_back(duplicate);
        }
        rehash(bad);
        input_error([&]{ pol::ControllerMemory::restore(bad,w.config().hash,policy_hash); });
    }
}
TEST(PG_T1_memory_requires_complete_schema_and_valid_plan_links) {
    auto w=knowledge_world({}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto j=m.snapshot(); j.as_object()["state"].as_object()["control"].as_object().erase("cooldowns"); rehash(j);
    input_error([&]{ pol::ControllerMemory::restore(j,w.config().hash,policy_hash); });
    auto& s=m.edit_state(); s.goals={waiting_goal()};
    pol::Plan p; p.id="plan-1"; p.goal_ids={"missing-goal"};
    p.steps.push_back({"step-1","read_known_message",cmd("a",0,"A24",Object{{"message","m"}}),1,true,{}});
    s.active_plan=p; input_error([&]{m.snapshot();});
    s.active_plan->goal_ids={s.goals[0].id}; s.plan_cursor=2;
    input_error([&]{m.snapshot();});
    s.plan_cursor=0; s.active_plan->status="running";
    auto good=m.snapshot(); CHECK(!good.is_null());
}
TEST(PG_T1_goal_identity_uses_canonical_target_and_episode) {
    auto a=waiting_goal(), b=a;
    a.target.specification=Object{{"b",2},{"a",1}};
    b.target.specification=Object{{"a",1},{"b",2}};
    CHECK(pol::goal_key(a)==pol::goal_key(b));
    ++b.episode; CHECK(pol::goal_key(a)!=pol::goal_key(b));
}

TEST(PG_T1_real_loan_partial_repayment_and_resume) {
    auto c=config(); World w(c,state(c));
    const Id proposal="b@1/proposal", obligation=proposal+"/obligation";
    do_action(w,cmd("b",1,"A11",Object{{"to","a"},{"expires",80},
        {"terms",Object{{"kind","loan_money"},{"lender","b"},{"borrower","a"},{"amount",20},{"due",200}}}}));
    do_action(w,cmd("a",1,"A12",Object{{"proposal",proposal},{"version",1},{"answer","accept"}}));
    pol::ControllerMemory m("a",c.hash,policy_hash);
    auto promised=read_all(w,m);
    CHECK(promised.commitment(proposal,1));
    CHECK(promised.commitment(proposal,1)->status=="agreed");
    CHECK(!promised.commitment(proposal,1)->remaining);
    do_action(w,cmd("b",2,"A13",Object{{"mode","loan_money"},{"to","a"},{"amount",20},{"proposal",proposal}}));
    auto borrowed=read_all(w,m);
    CHECK(borrowed.commitment(proposal,1)->status=="pending");
    CHECK(borrowed.commitment(proposal,1)->remaining==20);
    do_action(w,cmd("a",2,"A13",Object{{"mode","repay"},{"to","b"},{"amount",7},{"obligation",obligation}}));
    auto paid=read_page(w,m,64,{"a@2"});
    CHECK(paid.commitment(proposal,1)->remaining==13);
    w.run(1); auto duplicate=read_page(w,m,64,{"a@2"});
    CHECK(duplicate.commitment(proposal,1)->remaining==13);
    auto resumed=pol::ControllerMemory::restore(m.snapshot(),c.hash,policy_hash);
    CHECK(canonical(resumed.snapshot())==canonical(m.snapshot()));
    do_action(w,cmd("a",3,"A13",Object{{"mode","repay"},{"to","b"},{"amount",13},{"obligation",obligation}}));
    auto finished=read_page(w,m,64,{"a@3"});
    CHECK(finished.commitment(proposal,1)->status=="fulfilled");
    CHECK(finished.commitment(proposal,1)->remaining==0);
    CHECK(finished.nearest_commitments().empty());
    CHECK(canonical(pol::ControllerMemory::restore(m.snapshot(),c.hash,policy_hash).snapshot())==canonical(m.snapshot()));
    CHECK(w.debug_state().accounts.at("a")==80); CHECK(w.debug_state().accounts.at("b")==50);
}
TEST(PG_T1_all_gc11_slots_roundtrip_without_reinterpretation) {
    auto w=knowledge_world({fact("proof")},20); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    read_all(w,m); auto& s=m.edit_state();
    s.goals={waiting_goal()}; s.goals[0].source_refs={"proof"};
    s.goal_episode_counters["need/N03"]=1; s.decision_sequence=7;
    pol::Plan plan; plan.id="p"; plan.goal_ids={s.goals[0].id}; plan.created_at=10;
    plan.status="waiting_external"; plan.score=3.25; plan.source_refs={"proof"};
    plan.assumptions=Array{Object{{"source_ref","proof"},{"confidence",.8},{"assumed",true}}};
    plan.steps.push_back({"s1","read_known_message",cmd("a",0,"A24",Object{{"message","m"}}),1,true,{"proof"}});
    plan.steps.push_back({"s2","wait",cmd("a",0,"A01",Object{{"minutes",1}}),1,false,{}});
    s.active_plan=plan; s.plan_cursor=1;
    s.session_runtimes={{"session","a@7/proposal",1,"waiting_response",10,{},0,{"proof"}}};
    s.calendar_intents={{"calendar","a",s.goals[0].id,"session",30,40,2,"confirmed"}};
    s.cooldowns["a|b|conversation"]=120;
    s.outcome_history={{"response-root","b","conversation","accepted",10,"proof"}};
    s.next_regular_review_tick=25; s.fair_scan_cursor={{"partners",4},{"places",2}};
    s.pending_command_links={{"a@7",s.goals[0].id,"p","session",10}};
    s.pending_notifications={{"notification","reply","proof",10}};
    auto j=m.snapshot(); auto copy=pol::ControllerMemory::restore(j,w.config().hash,policy_hash);
    CHECK(canonical(j)==canonical(copy.snapshot()));
    CHECK(copy.state().active_plan->steps.size()==2); CHECK(copy.state().plan_cursor==1);
    CHECK(copy.state().calendar_intents[0].travel_buffer==2);
}
TEST(PG_T1_snapshot_rejects_edited_history_hash_at_write_boundary) {
    auto w=knowledge_world({fact("e")}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    m.edit_state().knowledge_cursor.prefix_hash=std::string(64,'0');
    input_error([&]{m.snapshot();});
}
TEST(PG_T1_receipt_observation_time_survives_delayed_ingest) {
    auto one=signed_repair();
    one.statement.arguments.as_object()["terms"]=Object{{"kind","joint"},{"place","shop"},{"start",0},{"end",100},{"minutes",1},{"topic","weather"}};
    auto w=knowledge_world({one},20); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto d=w.view_delta("a",m.cursor());
    const auto command=cmd("a",9,"A15",Object{{"proposal","b@1/proposal"},{"version",1}});
    d.receipts={{"a@9","a","a@9","completed","",4,encode(command),5}};
    auto v=pol::apply_delta(m,d);
    CHECK(v.commitment("b@1/proposal",1)->resolved_at==5);
    CHECK(canonical(pol::ControllerMemory::restore(m.snapshot(),w.config().hash,policy_hash).snapshot())==canonical(m.snapshot()));
}
TEST(PG_T1_first_terminal_session_proof_is_stable_and_versioned) {
    auto one=signed_repair();
    one.statement.arguments.as_object()["terms"]=Object{{"kind","joint"},{"place","shop"},{"start",0},{"end",100},{"minutes",1},{"topic","weather"}};
    auto w=knowledge_world({one},20); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto d=w.view_delta("a",m.cursor());
    auto completed=cmd("a",9,"A15",Object{{"proposal","b@1/proposal"},{"version",1}});
    auto interrupted=cmd("a",10,"A15",Object{{"proposal","b@1/proposal"},{"version",1}});
    d.receipts={{"a@9","a","a@9","completed","",4,encode(completed),5},
                {"a@10","a","a@10","interrupted","",6,encode(interrupted),7}};
    auto v=pol::apply_delta(m,d);
    CHECK(v.commitment("b@1/proposal",1)->status=="fulfilled");
    CHECK(v.commitment("b@1/proposal",1)->resolved_at==5);
    CHECK(canonical(pol::ControllerMemory::restore(m.snapshot(),w.config().hash,policy_hash).snapshot())==canonical(m.snapshot()));
    auto m2=pol::ControllerMemory("a",w.config().hash,policy_hash); read_all(w,m2);
    d=w.view_delta("a",m2.cursor()); completed.args.as_object()["version"]=2;
    d.receipts={{"a@9","a","a@9","completed","",4,encode(completed),5}};
    CHECK(pol::apply_delta(m2,d).commitment("b@1/proposal",1)->status=="agreed");
}
TEST(PG_T1_local_view_requires_a_valid_builder_result) {
    CHECK((!std::is_default_constructible_v<pol::LocalView>));
}
TEST(PG_T1_semantic_restore_validation_covers_nested_statuses_and_references) {
    auto w=knowledge_world({fact("proof")}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto& s=m.edit_state(); s.goals={waiting_goal()};
    s.session_runtimes={{"s","a@1/proposal",1,"waiting_response",0,{},0,{"proof"}}};
    s.calendar_intents={{"c","a",s.goals[0].id,"s",10,20,2,"tentative"}};
    s.outcome_history={{"root","b","conversation","accepted",0,"proof"}};
    const auto base=m.snapshot();
    using Edit=std::function<void(Object&)>;
    for(const auto& edit:std::vector<Edit>{
        [](Object& o){o["session_runtimes"].as_array()[0].as_object()["status"]="invented";},
        [](Object& o){o["session_runtimes"].as_array()[0].as_object()["first_wait"]=-1;},
        [](Object& o){o["calendar_intents"].as_array()[0].as_object()["status"]="invented";},
        [](Object& o){o["calendar_intents"].as_array()[0].as_object()["actor"]="b";},
        [](Object& o){o["outcome_history"].as_array()[0].as_object()["outcome"]="invented";},
        [](Object& o){o["goals"].as_array()[0].as_object()["deadline"]=-1;},
        [](Object& o){o["goals"].as_array()[0].as_object()["actor"]="b";},
        [](Object& o){o["goal_episode_counters"].as_object()["need/N03"]=-1;},
        [](Object& o){o["outcome_history"].as_array().push_back(o["outcome_history"].as_array()[0]);},
        [](Object& o){o["goals"].as_array()[0].as_object()["parent_goal_ids"]=Array{"missing"};},
        [](Object& o){o["goals"].as_array()[0].as_object()["status"]="blocked";o["goals"].as_array()[0].as_object()["next_review_at"]=nullptr;}
    }) {
        auto bad=base; edit(bad.as_object()["state"].as_object()["control"].as_object()); rehash(bad);
        input_error([&]{pol::ControllerMemory::restore(bad,w.config().hash,policy_hash);});
    }
}
TEST(PG_T1_malformed_protocol_evidence_is_retained_but_not_a_commitment) {
    auto e=signed_repair(); e.statement.arguments.as_object()["signatures"]="not-an-array";
    auto w=knowledge_world({e}); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    auto v=read_all(w,m); CHECK(v.evidence(e.id)); CHECK(v.nearest_commitments().empty());
    CHECK(canonical(pol::ControllerMemory::restore(m.snapshot(),w.config().hash,policy_hash).snapshot())==canonical(m.snapshot()));
}
TEST(PG_T1_invalid_page_does_not_leave_partially_indexed_evidence) {
    auto w=knowledge_world({fact("first"),fact("second")}); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    const auto before=canonical(m.snapshot()); auto d=w.view_delta("a",m.cursor());
    d.evidence[1].statement.valid_until=-2;
    input_error([&]{pol::apply_delta(m,d);}); CHECK(canonical(m.snapshot())==before);
}
TEST(PG_T1_appended_hypothesis_updates_only_own_cursor_and_not_old_view) {
    auto w=knowledge_world({fact("first")}); pol::ControllerMemory m("a",w.config().hash,policy_hash);
    auto old=read_all(w,m); auto before_other=w.view("b");
    auto h=fact("new",false); h.kind="hypothesis"; h.source="a"; h.roots={"first"}; h.confidence=.9; h.half_life=60;
    w.hypothesize("a",h); auto updated=read_page(w,m);
    CHECK(updated.evidence_count()==2); CHECK(old.evidence_count()==1);
    CHECK(pol::query_belief(old,open_query()).truth==pol::Truth::KnownTrue);
    CHECK(pol::query_belief(updated,open_query()).truth==pol::Truth::Unknown);
    w.hypothesize("a",h); CHECK(w.view_delta("a",m.cursor()).evidence.empty());
    CHECK(canonical(w.view("b"))==canonical(before_other));
    auto restored=World::restore(w.config(),w.snapshot());
    CHECK(canonical(encode(w.view_delta("a",m.cursor())))==canonical(encode(restored.view_delta("a",m.cursor()))));
}

TEST(PG_T1_unread_envelopes_are_visible_without_content_and_views_are_immutable) {
    auto c=config(); World w(c,state(c));
    do_action(w,cmd("b",1,"A11",Object{{"to","a"},{"channel","digital"},{"expires",80},
        {"terms",Object{{"kind","repair"},{"debtor","b"},{"creditor","a"},{"target","target"},{"due",100}}}}),2);
    pol::ControllerMemory m("a",c.hash,policy_hash); auto before=read_all(w,m);
    CHECK(before.unread_messages().size()==1);
    CHECK(before.message_envelope("b@1/message"));
    CHECK(!before.message_envelope("b@1/message")->read);
    CHECK(!has(encode(*before.message_envelope("b@1/message")),"content"));
    CHECK(before.commitment("b@1/proposal",1)==nullptr);
    do_action(w,cmd("a",1,"A24",Object{{"message","b@1/message"}}));
    auto after=read_all(w,m);
    CHECK(after.unread_messages().empty()); CHECK(after.message_envelope("b@1/message")->read);
    CHECK(!before.message_envelope("b@1/message")->read);
    CHECK(canonical(pol::ControllerMemory::restore(m.snapshot(),c.hash,policy_hash).snapshot())==canonical(m.snapshot()));
}
TEST(PG_T1_evidence_and_inbox_share_one_fair_page_budget) {
    auto c=config(); auto s=state(c); s.actors["a"].beliefs.clear();
    for(int i=0;i<130;++i) s.actors["a"].beliefs.push_back(fact("old-"+std::to_string(i)));
    for(int i=0;i<8;++i) {
        Message message; message.id="mail-"+std::to_string(i); message.author="b";
        message.recipients={"a"}; message.channel="digital"; message.delivered=true;
        message.content=Object{{"type","claim"},{"statement",encode(fact("hidden").statement)}};
        s.messages[message.id]=message; s.actors["a"].inbox.push_back(message.id);
    }
    World w(c,s); pol::ControllerMemory m("a",c.hash,policy_hash);
    auto first=w.view_delta("a",m.cursor(),4);
    CHECK(first.records_examined==4); CHECK(first.evidence.size()==2); CHECK(first.inbox.size()==2);
    auto v=pol::apply_delta(m,first); CHECK(v.unread_messages().size()==2);
    auto restored_world=World::restore(c,w.snapshot());
    auto restored=pol::ControllerMemory::restore(m.snapshot(),c.hash,policy_hash);
    CHECK(canonical(encode(w.view_delta("a",m.cursor(),4)))==canonical(encode(restored_world.view_delta("a",restored.cursor(),4))));
    auto all=read_all(w,m); CHECK(all.evidence_count()==130); CHECK(all.unread_messages(64).size()==8);
    CHECK(m.cursor().inbox_offset==8); CHECK(w.view_delta("a",m.cursor(),4).records_examined==0);
}
TEST(PG_T1_unread_body_changes_neither_cursor_nor_local_memory) {
    auto c=config(); auto s=state(c);
    Message msg; msg.id="m"; msg.author="b"; msg.recipients={"a"}; msg.channel="digital";
    msg.delivered=true; msg.content=Object{{"secret","one"}};
    s.messages[msg.id]=msg; s.actors["a"].inbox={"m"}; auto other=s;
    other.messages["m"].content=Object{{"secret","entirely different hidden terms"}};
    World one(c,s), two(c,other); pol::ControllerMemory a("a",c.hash,policy_hash),b("a",c.hash,policy_hash);
    auto va=read_all(one,a),vb=read_all(two,b);
    CHECK(va.unread_messages().size()==1);
    CHECK(canonical(va.export_json())==canonical(vb.export_json()));
    CHECK(canonical(a.snapshot())==canonical(b.snapshot()));
}
TEST(PG_T1_memory_identity_and_goal_target_family_cannot_be_relabelled) {
    auto w=knowledge_world({fact("e")}); pol::ControllerMemory m("a",w.config().hash,policy_hash); read_all(w,m);
    auto changed=m; changed.edit_state().policy_config_hash=math::sha256("different policy");
    input_error([&]{changed.snapshot();});
    changed=m; changed.edit_state().goals={waiting_goal()};
    changed.edit_state().goals[0].target.kind="resource";
    input_error([&]{changed.snapshot();});
}
