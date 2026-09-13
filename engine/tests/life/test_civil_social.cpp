#include "test.hpp"
#include "life/world.hpp"
#include <algorithm>
using namespace life;
namespace {
World pair(){auto w=World::generate(42,8);w.configure_recovery();auto& s=w.edit_for_test();for(auto& a:s.actors){a.place=2;a.action={};a.mind.projects.projects.clear();a.mind.projects.procedures.clear();for(auto& q:a.mind.career.questions){q.stage=CareerStage::Deferred;q.next_review=86400000;}a.mind.career.active_target=0;a.mind.civil.questions.items.clear();a.mind.known.fill(false);a.mind.known[std::size_t(Method::Idle)]=true;a.mind.social.norms.fill(0);a.mind.social.community.confidentiality=0;a.social=.05;a.body.energy=a.body.water=.9;a.body.sleep=.1;}s.actors[0].mind.projects.procedures.push_back(familiar_procedure(GoalKind::FindCompany,s.next_id++));auto& b=s.actors[1];s.ledger.initial_money+=300-b.money;b.money=b.mind.believed_money=300;CHECK(w.conversation_for_test(1,2));return w;}
}
TEST("civil_social", money_help_is_real_conserved_transfer_after_consent){auto w=pair();auto& s=w.edit_for_test();double a=s.actors[0].money,b=s.actors[1].money;w.propose_social_for_test(1,Interaction::AskMoney,2,12);NEAR(s.actors[0].money,a,0);w.run_seconds(90);CHECK(s.civil.gifts==1);NEAR(s.actors[0].money,a+12,1e-8);NEAR(s.actors[1].money,b-12,1e-8);w.validate();}
TEST("civil_social", donor_can_refuse_and_no_money_is_created){auto w=pair();auto& s=w.edit_for_test();s.ledger.initial_money-=s.actors[1].money;s.actors[1].money=s.actors[1].mind.believed_money=0;double money=s.actors[0].money;w.propose_social_for_test(1,Interaction::AskMoney,2,12);w.run_seconds(90);CHECK(s.civil.gifts==0);NEAR(s.actors[0].money,money,0);}
TEST("civil_social", phone_number_is_learned_from_answer){auto w=pair();auto& m=w.edit_for_test().actors[0].mind.civil;m.contacts.clear();w.propose_social_for_test(1,Interaction::AskPhone,2);CHECK(!m.number_for(2));w.run_seconds(65);CHECK(m.number_for(2).has_value());}
TEST("civil_social", procedural_lesson_does_not_grant_unknown_atomic_skill){auto w=pair();auto& s=w.edit_for_test();CHECK(s.actors[1].mind.projects.procedures.empty());w.propose_social_for_test(1,Interaction::ExplainProcedure,2,1);w.run_seconds(155);CHECK(s.actors[1].mind.civil.lessons.received==1);CHECK(s.actors[1].mind.projects.procedures.empty());CHECK(!s.actors[1].mind.known[std::size_t(Method::Talk)]);}
TEST("civil_social", salary_and_leisure_support_fallible_help_hypothesis){auto w=pair();auto& s=w.edit_for_test();auto& m=s.actors[0].mind.social.community;ResourceFact salary{s.next_id++,2,0,2,ResourceKind::IncomeHourly,30,1,0,true};ResourceFact cost{s.next_id++,2,0,2,ResourceKind::LeisureSpending,25,1,0,true};m.resources.receive(salary,2,0);m.resources.receive(cost,2,0);auto v=w.personal_view(1);CHECK(std::find(v.civil.help_candidates.begin(),v.civil.help_candidates.end(),2)!=v.civil.help_candidates.end());auto low=w;low.edit_for_test().ledger.initial_money-=low.state().actors[1].money;low.edit_for_test().actors[1].money=0;CHECK(w.personal_view(1).civil.help_candidates==low.personal_view(1).civil.help_candidates);}
TEST("civil_social", learned_procedure_can_become_an_executable_project){auto w=pair();auto& s=w.edit_for_test();auto& receiver=s.actors[1];receiver.mind.known[std::size_t(Method::Visit)]=receiver.mind.known[std::size_t(Method::Talk)]=true;w.propose_social_for_test(1,Interaction::ExplainProcedure,2,1);w.run_seconds(130);const auto* p=receiver.mind.projects.known(1);CHECK(p);CHECK(p->source!=0);GoalCandidate g;g.kind=GoalKind::FindCompany;g.basis=p->source;auto project=receiver.mind.projects.propose(g,1,s.now);CHECK(project);CHECK(!project_options(w.personal_view(2)).empty());}
TEST("civil_social", interrupted_money_request_does_not_transfer){auto w=pair();auto& s=w.edit_for_test();double cash=s.actors[0].money;w.propose_social_for_test(1,Interaction::AskMoney,2,12);w.run_seconds(10);w.withdraw_social_for_test(2);s.autonomy=false;w.run_seconds(80);CHECK(s.civil.gifts==0);NEAR(s.actors[0].money,cash,0);}
TEST("civil_social", resource_evidence_changes_autonomous_request_not_hidden_wealth){
 auto base=pair();auto& s=base.edit_for_test();auto& a=s.actors[0];a.mind.known[std::size_t(Method::Social)]=true;
 for(auto& method:a.mind.social.methods)method.mastery=0;
 a.mind.social.methods[std::size_t(Interaction::AskMoney)]={1,.3,.95,1,s.next_id++};
 s.ledger.initial_money-=a.money;a.money=a.mind.believed_money=0;
 auto informed=base;auto& si=informed.edit_for_test();auto& resource=si.actors[0].mind.social.community.resources;
 resource.receive({si.next_id++,2,0,2,ResourceKind::IncomeHourly,30,1,si.now,true},2,si.now);
 resource.receive({si.next_id++,2,0,2,ResourceKind::LeisureSpending,25,1,si.now,true},2,si.now);
 informed.run_seconds(180);base.run_seconds(180);
 CHECK(informed.state().civil.gifts>0);CHECK(base.state().civil.gifts==0);
}
TEST("civil_social", seeing_an_expensive_item_is_not_a_successful_social_action){
 auto w=pair();auto& s=w.edit_for_test();auto& a=s.actors[0];auto before=a.mind.self.predict(SelfDomain::Social);const auto outcomes=a.cog.outcome_published;
 Information f;f.id=s.next_id++;f.subject=2;f.kind=NewsKind::Opinion;f.resource_present=true;f.resource={f.id,2,400,0,ResourceKind::VisibleClothing,2,.8,0,false};
 InteractionObservation observation;observation.event=f.id;observation.other=2;observation.kind=Interaction::ShareNews;observation.stage=SocialStage::Completed;observation.information_present=true;observation.information=f;
 w.social_observation_for_test(1,observation);CHECK(a.cog.outcome_published==outcomes);w.run_seconds(30);
 CHECK(a.mind.social.community.resources.assess(2,s.now).clothing_tier);auto after=a.mind.self.predict(SelfDomain::Social);NEAR(before.acceptance,after.acceptance,0);CHECK(!a.mind.social.experience(Interaction::ShareNews,2,0,false));
}
TEST("civil_social", hunger_keeps_known_monetary_help_as_a_food_prerequisite){
 auto w=pair();auto v=w.personal_view(1);v.need.fill(0);v.need[0]=.95;v.need[7]=1;
 v.money=0;v.food=0;v.social.help_people={2};v.social.help_budget=0;v.civil.help_candidates={2};
 v.known[std::size_t(Method::Social)]=true;
 auto options=Planner::ideas(v,w.state().random);
 CHECK(std::any_of(options.begin(),options.end(),[](const auto& p){return p.method==Method::Social&&p.interaction==Interaction::AskMoney&&p.partner==2;}));
 CHECK(std::none_of(options.begin(),options.end(),[](const auto& p){return p.method==Method::Social&&p.interaction==Interaction::ExplainProcedure;}));
}
