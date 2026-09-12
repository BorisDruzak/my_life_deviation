#include "subjective.hpp"
#include "attention.hpp"
#include "valuation.hpp"
#include "scheduler.hpp"
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <string>
using namespace cog04;
int passed=0,failed=0;
void require(bool x) { if(!x) throw std::runtime_error("assertion failed"); }
void near(double a,double b,double eps=1e-12) {require(std::abs(a-b)<=eps);}
template<class F> void check(const std::string& name,F f) {
  try{f();++passed;std::cout<<"PASS "<<name<<'\n';}
  catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}
}
template<class F> void rejects(F f) { bool yes=false; try{f();}catch(const std::exception&){yes=true;}require(yes); }
int main(){
 check("S01_preserved_body_signal",[]{near(body_signal(.4,1),.48);});
 check("S02_focused_sensation",[]{near(sensation(.48,1,.2),.4992);});
 check("S03_peripheral_sensation",[]{near(sensation(.48,0,.2),.17472);});
 check("S04_zero_signal_no_phantom_pain",[]{near(sensation(body_signal(0,1.5),1,1),0);});
 check("S05_fusion_example",[]{auto b=fuse(Reading{.4992,.8,1},Reading{.2,.6,2});near(b.mean,.4176);require(b.status==Status::Supported);});
 check("S06_missing_is_unknown",[]{require(fuse(std::nullopt,std::nullopt).status==Status::Unknown);});
 check("S07_prior_is_not_observation",[]{auto b=fuse(std::nullopt,Reading{.7,.8,2});near(b.mean,.7);require(!b.fresh_observation);});
 check("S08_observed_zero_is_known",[]{auto b=fuse(Reading{0,.8,1},std::nullopt);require(b.status==Status::Supported);near(b.mean,0);});
 check("S09_conflicting_reliable_evidence",[]{require(fuse(Reading{.9,.9,1},Reading{.1,.9,2}).status==Status::Conflicting);});
 check("S10_same_source_not_double_weight",[]{auto b=fuse(Reading{.8,.7,1},Reading{.8,.7,1});near(b.confidence,.7);});
 check("S11_once_only_interpretation",[]{BeliefRegister r;require(r.update(4,1,Reading{.4,.8,1},std::nullopt));require(!r.update(4,1,Reading{.4,.8,1},std::nullopt));require(r.update(4,2,Reading{.4,.8,1},Reading{.3,.5,2}));require(r.updates()==2);});
 check("S19_same_evidence_id_inconsistent_payload_rejected",[]{rejects([]{fuse(Reading{.2,.8,1},Reading{.9,.8,1});});});
 check("A17_same_candidate_revision_inconsistent_payload_rejected",[]{CandidateQueue q;q.upsert({1,.5,2,1000,false});rejects([&]{q.upsert({1,.9,2,1000,false});});});
 check("S12_nonfinite_rejected",[]{rejects([]{body_signal(std::numeric_limits<double>::quiet_NaN(),1);});});
 check("S13_semantic_hysteresis",[]{require(band(.54,Band::High)==Band::High);require(band(.49,Band::High)==Band::Medium);});
 check("S14_rules_unknown_not_false",[]{std::array<Status,2> p{Status::Supported,Status::Unknown};require(conjunction(p)==Status::Unknown);});
 check("S15_rules_conflict_preserved",[]{std::array<Status,2> p{Status::Supported,Status::Conflicting};require(conjunction(p)==Status::Conflicting);});
 check("S16_known_refutation_blocks_rule",[]{std::array<Status,2> p{Status::Refuted,Status::Supported};require(conjunction(p)==Status::Refuted);});
 check("S17_no_learning_unobserved",[]{near(learn(0,{},.4),0);});
 check("S18_negative_from_zero_learned",[]{near(learn(0,-.8,.25),-.2);});
 check("A01_priority_existing_coefficients",[]{near(priority(.8,.5,.2,.9,.3,.6),.57);});
 check("A02_capacity_not_split_between_modules",[]{require(capacity(.5,.5,true)==7);require(capacity(1,1,true)==12);require(capacity(1,1,false)==0);});
 check("A03_focus_minimum_hold",[]{Focus f{1,.2,0,0};require(!may_switch(f,Candidate{2,.9,0,10000,false},1000,.5,.5));require(may_switch(f,Candidate{2,.9,0,10000,false},5000,.5,.5));});
 check("A04_equal_margin_not_switch",[]{Focus f{1,.2,0,0};auto m=.06+.18*.5+.12*.5;require(!may_switch(f,Candidate{2,.2+m,0,10000,false},5000,.5,.5));});
 check("A05_detected_emergency_bypasses_hold",[]{Focus f{1,.9,0,0};require(may_switch(f,Candidate{2,.1,0,10000,true},1,.5,.5));});
 check("A06_same_emergency_not_restarts_switch",[]{Focus f{1,.9,0,5000};require(!may_switch(f,Candidate{1,1,0,10000,true},1,.5,.5));});
 check("A07_normal_candidate_waits_rebuild",[]{Focus f{1,.1,0,7000};require(!may_switch(f,Candidate{2,1,0,10000,false},5000,.5,.5));});
 check("A08_switch_duration",[]{require(switch_ms(.5,.5,1)==2000);});
 check("A09_operation_budget_and_time",[]{require(operation_budget(.5,true)==18);near(operation_rate(.5,true),5);Work w;w.advance(5,.1);near(w.done,.5);require(!w.complete());w.advance(5,.1);require(w.complete());});
 check("A10_no_idle_or_sleep_credit",[]{Work w;w.active=false;w.advance(8,100);near(w.done,0);w.active=true;w.advance(operation_rate(1,false),10);near(w.done,0);});
 check("A11_feature_work",[]{near(feature_rate(.8,.5,.5,true),1.25);Work w;w.advance(1.25,.8);require(w.complete());});
 check("A12_exposure_split_same_threshold",[]{Exposure a{0,1.5},b{0,1.5};a.advance(.5,2);b.advance(.5,1);b.advance(.5,1);near(a.accum,b.accum);near(a.seconds_to_detection(.5),1);});
 check("A13_queue_order_independent",[]{CandidateQueue a,b;for(int i=1;i<=40;++i)a.upsert(Candidate{Id(i),i/40.,0,10000,false});for(int i=40;i>=1;--i)b.upsert(Candidate{Id(i),i/40.,0,10000,false});auto x=a.select(0,1),y=b.select(0,1);require(x==y&&x.size()==32);require(std::find(x.begin(),x.end(),1)!=x.end());});
 check("A14_expired_removed",[]{CandidateQueue q;q.upsert({1,.9,0,100,false});require(q.select(100,0).empty());});
 check("A15_duplicate_revision_not_inflates_queue",[]{CandidateQueue q;q.upsert({1,.5,0,1000,false});q.upsert({1,.5,0,1000,false});require(q.select(1,0).size()==1);});
 check("A16_superseded_candidate_version_ignored",[]{CandidateQueue q;q.upsert({1,.8,2,1000,false});q.upsert({1,.1,1,1000,false});near(q.score(1),.8);});
 check("A18_emergency_overflow_prefers_urgency",[]{CandidateQueue q;for(int i=1;i<=40;++i)q.upsert({Id(i),1.-i/100.,0,10000,true,i/40.});auto x=q.select(0,1);require(x.size()==32);require(x.front()==40);require(std::find(x.begin(),x.end(),1)==x.end());});
 check("N01_shame_example",[]{auto x=norm_response(.9,.95,.8,0,.7,.65,.9);near(x.resistance,.684);near(x.shame_target,.455);near(x.guilt_target,.43092);});
 check("N02_private_norm_without_audience",[]{auto x=norm_response(1,1,1,0,.7,0,1);near(x.shame_target,0);near(x.resistance,1);require(x.guilt_target>0);});
 check("N03_exception_removes_own_conflict",[]{auto x=norm_response(1,1,1,1,.7,0,1);near(x.resistance,0);});
 check("N04_observer_disapproval_without_own_acceptance",[]{auto x=norm_response(0,1,1,0,.7,.65,1);near(x.resistance,0);near(x.shame_target,.455);});
 check("N05_shame_evolves_in_logical_time",[]{near(approach(0,.455,8,8),.455*(1-std::exp(-1.)));});
 check("N06_subjective_sanction",[]{near(sanction(.4,.8,.5,.6),.096);});
 check("N07_duplicate_norm_group_max",[]{std::vector<NormCost> x{{1,.6},{1,.7},{2,.1}};near(moral_cost(x),.8);});
 check("V01_negative_expected_effect_preserved",[]{Ledger l;l.impulse(1,0,0,-.8);near(l.value(24,.0),-.8);});
 check("V02_duplicate_reward_rejected",[]{Ledger l;l.impulse(1,0,0,1);rejects([&]{l.impulse(1,0,0,1);});});
 check("V03_reward_delay",[]{Ledger l;l.impulse(1,0,0,-2);l.impulse(2,0,720,7.2);near(l.value(720,.01/24),-2+7.2*std::exp(-.3));});
 check("V04_future_not_current_need",[]{near(projected_relief(.1,.8,.1,0),1.015);near(projected_relief(.1,.8,.1,1),0);});
 check("V05_flow_partition_invariant",[]{Ledger a,b;a.flow(1,0,0,24,-.02);b.flow(1,0,0,12,-.02);b.flow(1,0,12,24,-.02);near(a.value(24,.01),b.value(24,.01));});
 check("V06_zero_discount_flow",[]{Ledger a;a.flow(1,0,0,24,-.02);near(a.value(24,0),-.48);});
 check("V07_tail_beyond_horizon_not_silent",[]{Ledger a;a.impulse(1,0,720,1);rejects([&]{a.value(24,.01);});});
 check("V08_probability_branches_complete",[]{std::vector<Branch> b{{.9,4},{.1,-2}};near(expected(b),3.4);rejects([]{expected(std::vector<Branch>{{.9,1},{.2,1}});});});
 check("V09_pause_can_beat_continue",[]{std::array<double,4> v{-.2,.45,.1,0};require(best_plan(v)==1);});
 check("V10_goal_satisfied_abandon_can_win",[]{std::array<double,4> v{-.4,-.1,-.2,0};require(best_plan(v)==3);});
 check("V11_unknown_not_free_reward",[]{Ledger a;rejects([&]{a.impulse(1,0,0,std::numeric_limits<double>::infinity());});});
 check("V12_overlapping_same_cost_rejected",[]{Ledger a;a.flow(1,0,0,10,-.2);rejects([&]{a.flow(1,0,9,11,-.2);});});
 check("V13_sign_squash",[]{near(squash(-3),-.75);near(squash(3),.75);});
 check("V14_per_time_discount_not_per_step",[]{near(std::exp(-.01*24),std::exp(-.01*12)*std::exp(-.01*12));});
 auto make=[](){BarrierScheduler s(4);for(Id a=0;a<4;++a)s.enqueue(0,200,a,int(a+1));return s;};
 check("Q01_wall_quota_does_not_change_result",[&]{auto a=make(),b=make();a.run(1,1,false);b.run(17,1,true);require(a.receipts()==b.receipts());require(a.values()==b.values());});
 check("Q02_one_two_four_workers_same",[&]{auto a=make(),b=make(),c=make();a.run(1,1,false);b.run(2,2,true);c.run(17,4,false);require(a.receipts()==b.receipts()&&a.receipts()==c.receipts());});
 check("Q03_barrier_not_publish_partial_results",[&]{auto a=make();require(!a.pump(1,1,false));for(auto x:a.values())require(x==0);require(a.now()==200);a.run(1,1,false);for(auto r:a.receipts())require(r.at==200);});
 check("Q04_no_early_completion",[&]{auto a=make();a.run(4,1,false);for(auto r:a.receipts())require(r.at==200);});
 check("Q05_stale_personal_snapshot_discarded",[]{BarrierScheduler s(2);s.enqueue(0,200,0,4);s.observe(100,0,99);s.run(1,1,false);require(!s.receipts().at(0).accepted);require(s.values().at(0)==99);});
 check("Q06_other_actors_private_update_no_telepathy",[]{BarrierScheduler s(2);s.enqueue(0,200,0,4);s.observe(100,1,99);s.run(1,1,false);require(s.receipts().at(0).accepted);require(s.values().at(0)==4);});
 check("Q07_same_timestamp_observation_precedes_commit",[]{BarrierScheduler s(1);s.enqueue(0,200,0,4);s.observe(200,0,99);s.run(1,1,false);require(!s.receipts().at(0).accepted);});
 check("Q08_one_active_operation_per_actor",[]{BarrierScheduler s(1);s.enqueue(0,200,0,4);rejects([&]{s.enqueue(0,300,0,2);});});
 check("Q09_invalid_wall_budget_rejected",[&]{auto s=make();rejects([&]{s.run(0,1,false);});});
 check("Q10_snapshot_copy_resume_at_barrier",[&]{auto a=make();a.pump(1,1,true);auto b=a;a.run(1,1,true);b.run(17,4,false);require(a.receipts()==b.receipts());});
 check("F01_random_bounds_2000_inputs",[]{std::mt19937_64 g(42);std::uniform_real_distribution<double>d(0,1);for(int i=0;i<2000;++i){auto n=norm_response(d(g),d(g),d(g),d(g),d(g),d(g),d(g));require(n.resistance>=0&&n.resistance<=1&&n.shame_target>=0&&n.shame_target<=1);auto s=sensation(body_signal(d(g),.5+d(g)),d(g),d(g));require(s>=0&&s<=1);}});
 std::cout<<"TOTAL "<<passed<<" passed; "<<failed<<" failed\n";return failed?1:0;
}
