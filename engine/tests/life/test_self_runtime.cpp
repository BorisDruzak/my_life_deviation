#include "test.hpp"
#if __has_include("life/experience.hpp")
#include "life/world.hpp"
#include "life/archive.hpp"
#include <filesystem>
#include <algorithm>
#include <limits>
using namespace life;
namespace {
std::string self_bytes(const Actor& a){Writer w;w(a.mind.self);return w.data;}
World self_world(){auto w=World::generate(42,8);w.configure_self_model();return w;}
OutcomeSignal signal(std::uint64_t id){OutcomeSignal s;s.id=s.root=s.action=id;s.method=Method::Talk;s.completed=true;s.feedback_observed=true;s.directness=1;s.intentional=true;s.at=0;s.goal_importance=.8;s.observed_mask=1u<<5;s.observed[5]=.3;s.action_feedback=.9;return s;}
}
TEST("self_runtime",executor_cannot_directly_change_self) {
    auto w=self_world();auto& s=w.edit_for_test();s.autonomy=false;auto before=self_bytes(s.actors[0]);
    w.command_for_test(1,Method::Eat,s.actors[0].home);w.run_seconds(95);
    CHECK(!w.state().actors[0].cog.outcome_inbox.empty());CHECK(self_bytes(w.state().actors[0])==before);
    w.edit_for_test().autonomy=true;w.run_seconds(120);
    CHECK(w.state().actors[0].cog.self_integrations>0);CHECK(self_bytes(w.state().actors[0])!=before);
}
TEST("self_runtime",forecast_never_updates_self_or_history) {
    auto w=self_world();auto v=w.personal_view(1);auto before=w.hash();
    for(unsigned i=0;i<100;++i){v.episode=i;Planner::choose(v,w.state().random);}
    CHECK(w.hash()==before);
}
TEST("self_runtime",hidden_outcome_is_not_self_knowledge) {
    auto a=self_world(),b=self_world();a.edit_for_test().autonomy=b.edit_for_test().autonomy=false;
    b.edit_for_test().actors[3].money+=1;b.edit_for_test().ledger.initial_money+=1;
    a.run_seconds(30);b.run_seconds(30);CHECK(self_bytes(a.state().actors[0])==self_bytes(b.state().actors[0]));
}
TEST("self_runtime",outcome_uses_two_paid_cognitive_operations) {
    auto w=self_world();auto& a=w.edit_for_test().actors[0];const auto ops=a.cog.completed_ops;
    w.observe_outcome_for_test(1,signal(900000));w.run_seconds(120);
    const auto& c=w.state().actors[0].cog;
    CHECK(c.counts[std::size_t(ThoughtKind::InterpretOutcome)]>0);
    CHECK(c.counts[std::size_t(ThoughtKind::Attribute)]>0);
    CHECK(c.completed_ops>=ops+2);CHECK(c.self_integrations>0);
}
TEST("self_runtime",gate_closed_cannot_update_self) {
    auto w=self_world();auto& a=w.edit_for_test().actors[0];a.body.oxygen=1;
    auto before=self_bytes(a);w.observe_outcome_for_test(1,signal(900000));w.run_ms(500);
    CHECK(self_bytes(w.state().actors[0])==before);
}
TEST("self_runtime",inbox_is_bounded_and_prioritizes_important_evidence) {
    auto w=self_world();w.edit_for_test().autonomy=false;
    for(unsigned i=1;i<=80;++i){auto s=signal(900000+i);s.goal_importance=i==1?1:.1;w.observe_outcome_for_test(1,s);}
    const auto& c=w.state().actors[0].cog;CHECK(c.outcome_inbox.size()<=32);CHECK(c.outcome_dropped>0);
    CHECK(std::any_of(c.outcome_inbox.begin(),c.outcome_inbox.end(),[](const auto& x){return x.root==900001;}));
}
TEST("self_runtime",same_visible_result_different_attributions) {
    auto s=signal(1);s.completed=false;s.blocked=true;s.observed[5]=0;s.action_feedback=.1;s.other_decision=.2;
    InterpretationContext a,b;a.causal_clarity=b.causal_clarity=.2;a.rejection_bias=.95;b.rejection_bias=.05;
    a.self.efficacy=.2;b.self.efficacy=.8;
    auto ea=interpret_outcome(s,a),eb=interpret_outcome(s,b);
    ea.attribution=attribute_outcome(ea,s,a);eb.attribution=attribute_outcome(eb,s,b);
    CHECK(ea.attribution.self>eb.attribution.self);
}
TEST("self_runtime",save_restore_mid_interpretation_and_attribution) {
    for(auto stage:{Operation::InterpretOutcome,Operation::AttributeOutcome}){
        auto w=self_world();w.observe_outcome_for_test(1,signal(900000));bool found=false;
        for(unsigned i=0;i<3000;++i){w.run_ms(50);if(w.state().actors[0].cog.operation==stage){found=true;break;}}
        CHECK(found);auto path=std::filesystem::temp_directory_path()/"life-self-stage.bin";w.save(path.string());
        auto restored=World::load(path.string());CHECK(restored.hash()==w.hash());w.run_seconds(90);restored.run_seconds(90);CHECK(restored.hash()==w.hash());std::filesystem::remove(path);
    }
}
TEST("self_runtime",workers_and_diagnostics_cannot_change_result) {
    auto a=self_world(),b=self_world(),c=self_world();a.set_workers(1);b.set_workers(2);c.set_workers(4);
    for(auto* w:{&a,&b,&c})w->observe_outcome_for_test(1,signal(900000));
    std::uint64_t traces=0;c.set_self_logger([&](const auto&){++traces;});
    a.run_seconds(1800);b.run_seconds(1800);c.run_seconds(1800);
    CHECK(a.hash()==b.hash());CHECK(b.hash()==c.hash());CHECK(traces>0);
}
TEST("self_runtime",history_initializes_beliefs_from_evidence_not_random_traits) {
    auto w=self_world();bool changed=false;
    for(const auto& a:w.state().actors){a.mind.self.validate();CHECK(a.mind.self.primary_sources.size()>0);changed|=a.mind.self.predict(SelfDomain::Social).efficacy!=.5;}
    CHECK(changed);
}
TEST("self_runtime",appraisal_links_self_to_body_without_writing_biology) {
    SelfPrediction low,high;low.control=low.coping=.1;high.control=high.coping=.9;
    auto a=self_appraisal(low,.9,.8,.9),b=self_appraisal(high,.9,.8,.9);Body ba,bb;Biology bio;Cognitive cog;
    auto ea=appraisal_targets(a,ba),eb=appraisal_targets(b,bb);CHECK(ea[0]+ea[1]>eb[0]+eb[1]);
    advance_body(ba,bio,Input{},ea,60);advance_body(bb,bio,Input{},eb,60);CHECK(ba.activation>bb.activation);
    CHECK(capability(ba,cog,false).current[1]<=capability(bb,cog,false).current[1]);
}
TEST("self_runtime",external_obstacle_not_hidden_true_cause_drives_attribution) {
    auto s=signal(1);s.completed=false;s.blocked=true;s.observed_obstacle=1;s.action_feedback=.05;
    InterpretationContext c;c.causal_clarity=.9;auto e=interpret_outcome(s,c);auto a=attribute_outcome(e,s,c);
    CHECK(a.environment>a.self);CHECK(a.environment>.5);
}
TEST("self_runtime",pending_low_priority_outcomes_do_not_trigger_per_second_decisions) {
 auto w=self_world();auto& a=w.edit_for_test().actors[0];
 a.body.sleep=.1;a.social=a.leisure=.1;
 for(unsigned i=1;i<=12;++i){auto s=signal(800000+i);s.goal_importance=.01;w.observe_outcome_for_test(1,s);}
 w.run_seconds(900);
 CHECK(w.state().actors[0].cog.episode<180);
}
TEST("self_runtime",delayed_recovery_keeps_original_domain) {
 auto s=signal(1);s.stage=SelfEpisodeStage::Recovery;s.recovery_observed=true;
 s.recovery_domain=SelfDomain::Conflict;s.method=Method::Talk;
 CHECK(interpret_outcome(s,InterpretationContext{}).domain==SelfDomain::Conflict);
}
TEST("self_runtime",completed_sleep_without_restoration_is_not_interpreted_as_success) {
 auto s=signal(1);s.method=Method::Sleep;s.observed.fill(0);s.observed_mask=1u<<2;
 auto e=interpret_outcome(s,InterpretationContext{});CHECK(e.success<.1);
}
TEST("self_runtime",prospective_appraisal_uses_self_during_real_forecast) {
 auto w=self_world();
 for(auto& a:w.edit_for_test().actors){a.place=2;a.social=.02;a.leisure=1;a.body.energy=a.body.water=.9;a.body.sleep=.1;a.body.fatigue=.1;a.mind.known.fill(false);}
 auto& a=w.edit_for_test().actors[0];a.mind.known.fill(false);a.mind.known[std::size_t(Method::Talk)]=true;
 for(Id id=2;id<=8;++id){Percept p;p.token=id;p.exposure_id=10000+id;p.noticed=p.recognized=true;p.threshold=1;p.evidence=2;a.cog.percepts.push_back(p);}
 unsigned forecasts=0;w.set_thought_logger([&](const Thought& t){if(t.actor==1&&t.kind==ThoughtKind::Forecast&&t.method==Method::Talk)++forecasts;});
 w.run_seconds(120);CHECK(forecasts>0);
 const auto& causes=w.state().actors[0].affect.causes;
 CHECK(std::any_of(causes.begin(),causes.end(),[](const auto& c){return c.id==(0x6f00000000000000ull|std::uint64_t(SelfDomain::Social));}));
}

TEST("self_runtime",published_result_root_is_not_old_action_start) {
 auto w=self_world();w.edit_for_test().autonomy=false;
 w.command_for_test(1,Method::Eat,w.state().actors[0].home);
 auto action=w.state().actors[0].action.id;w.run_seconds(95);
 const auto& box=w.state().actors[0].cog.outcome_inbox;
 auto s=std::find_if(box.begin(),box.end(),[&](const auto& x){return x.action==action;});
 CHECK(s!=box.end());CHECK(s->root!=s->action);CHECK(s->root==s->id);
}
TEST("self_runtime",invalid_captured_self_prediction_is_rejected) {
 auto s=signal(1);s.decision.known=true;s.decision.action=1;
 s.decision.self_prediction.coping=std::numeric_limits<double>::quiet_NaN();
 THROWS(validate_outcome(s));
}
TEST("self_runtime",meaningless_completed_action_is_not_observed_functioning) {
 auto w=self_world();auto& a=w.edit_for_test().actors[0];a.mind.known.fill(false);
 auto s=signal(900000);s.method=Method::Sleep;s.observed.fill(0);s.observed_mask=1u<<2;s.goal_importance=1;
 w.observe_outcome_for_test(1,s);w.run_seconds(60);
 CHECK(w.state().actors[0].cog.self_integrations==1);CHECK(w.state().actors[0].cog.observed_functioning==0);
}

TEST("self_runtime",recovery_requires_later_observed_functioning_and_restores_mid_recovery) {
 auto w=self_world();
 for(auto& a:w.edit_for_test().actors){a.mind.known.fill(false);a.body.energy=a.body.water=.9;a.body.sleep=.1;}
 auto bad=signal(7000);bad.completed=false;bad.blocked=true;bad.observed_mask=(1u<<5)|(1u<<10);bad.observed[5]=0;bad.observed[10]=-.6;
 w.observe_outcome_for_test(1,bad);w.run_seconds(120);
 const auto& first=w.state().actors[0];CHECK(first.cog.self_integrations==1);
 CHECK(first.cog.recovery_integrations==0);
 w.run_seconds(1800);CHECK(w.state().actors[0].cog.recovery_integrations==0);
 auto good=signal(7001);good.at=w.state().now;w.observe_outcome_for_test(1,good);
 bool pending=false;
 for(unsigned i=0;i<5000;++i){w.run_ms(100);const auto& c=w.state().actors[0].cog;if(c.operation==Operation::AttributeOutcome&&c.self_input.stage==SelfEpisodeStage::Recovery){pending=true;break;}}
 CHECK(pending);auto path=std::filesystem::temp_directory_path()/"life-recovery-mid.bin";w.save(path.string());auto r=World::load(path.string());
 w.run_seconds(120);r.run_seconds(120);CHECK(w.hash()==r.hash());CHECK(w.state().actors[0].cog.recovery_integrations==1);std::filesystem::remove(path);
}

TEST("self_runtime",redelivery_of_one_social_result_is_one_self_source) {
 auto w=self_world();w.edit_for_test().autonomy=false;
 InteractionObservation o;o.event=777;o.delivery=900000;o.other=2;o.kind=Interaction::Compliment;o.stage=SocialStage::Completed;o.initiated=true;
 w.social_observation_for_test(1,o);o.delivery=900001;w.social_observation_for_test(1,o);
 CHECK(w.state().actors[0].cog.outcome_inbox.size()==1);
}

#else
TEST("self_runtime",self_runtime_pipeline_must_exist){CHECK(false && "Missing self runtime pipeline");}
#endif
