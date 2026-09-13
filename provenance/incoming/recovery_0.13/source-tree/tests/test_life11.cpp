#include "test.hpp"
#include "life/world.hpp"
#include <algorithm>
#include "valuation.hpp"
using namespace life;
TEST("life11",familiarity_forecast_reads_current_owned_relation){
    auto w=World::generate(42,8);w.configure_community();
    auto& a=w.edit_for_test().actors[0];
    CHECK(!a.mind.relations.empty());const auto id=a.mind.relations.front().person;
    a.action.method=Method::Talk;a.action.phase=Phase::Running;a.action.partner=id;
    a.mind.social.methods[std::size_t(Interaction::RomanticTouch)].mastery=1;
    a.mind.social.norms.fill(0);a.mind.social.public_romance_disapproval=0;
    a.mind.social.community.kin_romance=0;a.mind.social.community.stranger_romance=.6;
    auto* r=a.mind.relation(id);r->familiarity=0;
    auto low=evaluate_social(w.personal_view(1).social,Interaction::RomanticTouch,id,0);
    r->familiarity=1;
    auto high=evaluate_social(w.personal_view(1).social,Interaction::RomanticTouch,id,0);
    CHECK(high.moral<low.moral);
}
TEST("life11",new_profile_contains_known_procedures_and_work_contracts){auto w=World::generate(42,8);w.configure_life_projects();CHECK(w.state().life.enabled);CHECK(w.state().community.enabled);for(const auto& a:w.state().actors){CHECK(a.mind.projects.enabled);CHECK(a.mind.projects.procedures.size()>=4);CHECK(a.employment.active);}}
TEST("life11",acquaintance_is_directed_grounded_and_idempotent){Mind a,b;CHECK(a.experience_contact(2,10,1000,300,.5,2,true));CHECK(a.relation(2));CHECK(a.relation(2)->introduced);CHECK(!b.relation(1));const auto f=a.relation(2)->familiarity;CHECK(!a.experience_contact(2,10,1001,300,.5,2,true));NEAR(a.relation(2)->familiarity,f,0);CHECK(a.experience_contact(2,11,2000,300,-.5,2));CHECK(a.relation(2)->familiarity>f);}
TEST("life11",search_uses_known_places_without_target_telepathy){auto w=World::generate(42,8);w.configure_life_projects();auto& a=w.edit_for_test().actors[0];GoalCandidate g;g.basis=123;g.kind=GoalKind::FindCompany;auto id=a.mind.projects.propose(g,1,0);CHECK(id);auto v=w.personal_view(1);auto choices=project_options(v);CHECK(!choices.empty());for(const auto& x:choices){CHECK(x.partner==0);CHECK(std::any_of(v.places.begin(),v.places.end(),[&](const auto& p){return p.id==x.place;}));}}
TEST("life11",conversation_keeps_the_work_action_and_its_progress){auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();s.now=10*3600000LL;s.next_physical=s.now;for(auto& x:s.actors){x.physical_at=s.now;x.place=x.home;}s.autonomy=false;auto& a=s.actors[0];auto& b=s.actors[1];a.place=b.place=5;a.social=b.social=.1;w.command_for_test(1,Method::Work,5);w.command_for_test(2,Method::Work,5);auto id=a.action.id;CHECK(w.conversation_for_test(1,2));CHECK(a.action.id==id&&a.action.method==Method::Work);CHECK(a.conversation.id&&a.conversation.id==b.conversation.id);w.run_seconds(60);CHECK(a.economy.work_today>0);CHECK(a.secondary_social_ms>0);CHECK(a.action.effort_seconds<60);}
TEST("life11",sleeping_person_cannot_be_talked_into_a_parallel_task){auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();auto& a=s.actors[0];auto& b=s.actors[1];a.place=b.place=2;b.action.phase=Phase::Asleep;b.action.method=Method::Sleep;CHECK(!w.conversation_for_test(1,2));CHECK(b.action.phase==Phase::Asleep);}
TEST("life11",critical_thirst_is_not_displaced_by_a_long_term_work_project){
 auto w=World::generate(42,8);w.configure_life_projects();auto& a=w.edit_for_test().actors[0];
 GoalCandidate g;g.kind=GoalKind::WorkShift;g.basis=123;g.place=5;g.importance=1;g.expected_gain=1;g.success=1;g.deadline=17*3600000LL;
 CHECK(a.mind.projects.propose(g,4,0));auto v=w.personal_view(1);v.now=10*3600000LL;v.need.fill(0);v.need[1]=1;v.focus_metric=1;
 auto options=Planner::ideas(v,w.state().random);CHECK(!options.empty());for(const auto& p:options)CHECK(p.method==Method::Drink);
}
TEST("life11",search_procedure_can_attend_and_recognize_a_real_person){
 auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();
 for(auto& a:s.actors){a.place=2;a.body.energy=a.body.water=.95;a.body.sleep=.1;a.social=.15;a.money=400;a.mind.believed_money=400;}
 s.ledger.initial_money=3200;
 auto& a=s.actors[0];GoalCandidate g;g.kind=GoalKind::FindCompany;g.importance=.9;g.basis=321;
 auto id=a.mind.projects.propose(g,1,0);a.mind.projects.observe(id,StepOutcome::Success,555,0);
 w.run_seconds(180);CHECK(std::any_of(a.cog.percepts.begin(),a.cog.percepts.end(),[](const auto& p){return p.recognized;}));
}
TEST("life11",a_missed_shift_does_not_count_as_a_completed_goal){
 auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();s.autonomy=false;
 auto& a=s.actors[0];GoalCandidate g;g.kind=GoalKind::WorkShift;g.place=5;g.basis=123;g.deadline=17*3600000LL;auto id=a.mind.projects.propose(g,4,0);
 s.now=17*3600000LL-1000;s.next_physical=s.now;for(auto& x:s.actors)x.physical_at=s.now;
 w.run_seconds(2);CHECK(a.mind.projects.find(id)->status!=ProjectStatus::Completed);CHECK(a.economy.earned==0);
}
namespace {
World adult_pair(){
 auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();
 for(auto& a:s.actors){a.place=a.home;a.mind.projects.procedures.clear();a.body.energy=a.body.water=.95;a.body.sleep=.1;}
 auto& a=s.actors[0];auto& b=s.actors[1];a.place=b.place=a.home;a.social=b.social=.1;
 for(Actor* who:{&a,&b}){
  who->desire.deficit=.9;who->desire.value=.8;who->desire.refractory=0;who->mind.social.methods.fill(MethodBelief{});
  for(auto k:{Interaction::Introduce,Interaction::InviteMeeting,Interaction::PartnerIntimacy,Interaction::RequestWork})who->mind.social.methods[std::size_t(k)]={.9,.8,.9,.8,999};
  who->mind.social.norms.fill(0);who->mind.social.community.stranger_romance=0;who->mind.social.community.kin_romance=0;who->mind.social.public_romance_disapproval=0;who->mind.social.expected_attraction={0,1,1};
 }
 return w;
}
void await_acceptance(World& w,Interaction kind){
 for(int i=0;i<120&&w.state().social.accepted[std::size_t(kind)]==0;++i)w.run_ms(500);
 CHECK(w.state().social.accepted[std::size_t(kind)]>0);
}
}
TEST("life11",partner_intimacy_needs_consent_and_applies_once_only_after_completion){
 auto w=adult_pair();CHECK(w.conversation_for_test(1,2));w.propose_social_for_test(1,Interaction::PartnerIntimacy,2);await_acceptance(w,Interaction::PartnerIntimacy);
 CHECK(w.state().life.intimacies==0);const auto id=w.state().social.events.back().id;
 w.edit_for_test().autonomy=false;w.run_seconds(1801);CHECK(w.state().life.intimacies==1);
 for(int i=0;i<2;++i){const auto& a=w.state().actors[i];CHECK(std::count(a.desire.applied_results.begin(),a.desire.applied_results.end(),id)==1);}
 w.run_seconds(1);CHECK(w.state().life.intimacies==1);
}
TEST("life11",withdrawal_does_not_grant_a_full_intimate_result){
 auto w=adult_pair();CHECK(w.conversation_for_test(1,2));w.propose_social_for_test(1,Interaction::PartnerIntimacy,2);await_acceptance(w,Interaction::PartnerIntimacy);
 w.run_seconds(10);w.withdraw_social_for_test(2);w.edit_for_test().autonomy=false;w.run_seconds(1800);CHECK(w.state().life.intimacies==0);CHECK(w.state().actors[0].desire.applied_results.empty());
}
TEST("life11",public_place_is_not_a_valid_private_intimacy_location){auto w=adult_pair();w.edit_for_test().actors[0].place=w.edit_for_test().actors[1].place=2;CHECK(w.conversation_for_test(1,2));w.propose_social_for_test(1,Interaction::PartnerIntimacy,2);CHECK(w.state().social.events.empty());}
TEST("life11",invitation_communicates_an_address_but_not_sexual_consent){
 auto w=adult_pair();auto& s=w.edit_for_test();auto& b=s.actors[1];b.mind.projects.procedures.push_back(familiar_procedure(GoalKind::MeetPerson,123));
 auto private_home=s.actors[0].home;CHECK(std::none_of(b.mind.places.begin(),b.mind.places.end(),[&](const auto& p){return p.id==private_home;}));
 CHECK(w.conversation_for_test(1,2));MeetingProposal m;m.host=1;m.place=private_home;m.at=3600000;m.until=7200000;m.private_visit=true;m.route_from=1;m.route_seconds=120;
 w.invitation_for_test(1,2,m);w.run_seconds(180);CHECK(w.state().life.meetings_agreed==1);CHECK(!b.appointments.empty());CHECK(std::any_of(b.mind.places.begin(),b.mind.places.end(),[&](const auto& p){return p.id==private_home;}));CHECK(w.state().life.intimacies==0);
}
TEST("life11",current_refusal_prevents_intimacy_despite_close_relationship){auto w=adult_pair();auto& b=w.edit_for_test().actors[1];b.mind.social.methods[std::size_t(Interaction::PartnerIntimacy)].expected_pleasure=-1;b.mind.social.norms.fill(1);CHECK(w.conversation_for_test(1,2));w.propose_social_for_test(1,Interaction::PartnerIntimacy,2);w.run_seconds(90);CHECK(w.state().social.declined[std::size_t(Interaction::PartnerIntimacy)]==1);CHECK(w.state().life.intimacies==0);}
TEST("life11",private_intimacy_cannot_overlap_work){auto w=adult_pair();auto& s=w.edit_for_test();s.actors[0].action.method=Method::Work;s.actors[0].action.phase=Phase::Running;s.actors[0].action.id=s.next_id++;s.actors[0].action.destination=s.actors[0].place;s.actors[0].action.end=3600000;s.actors[0].action.next_window=300000;CHECK(w.conversation_for_test(1,2));w.propose_social_for_test(1,Interaction::PartnerIntimacy,2);w.run_seconds(90);CHECK(w.state().social.accepted[std::size_t(Interaction::PartnerIntimacy)]==0);}
TEST("life11",waiting_projects_and_parallel_channels_survive_restore){auto a=adult_pair();CHECK(a.conversation_for_test(1,2));a.edit_for_test().autonomy=false;a.run_seconds(10);const std::string path="/tmp/life11-resume.save";a.save(path);auto b=World::load(path);a.run_seconds(30);b.run_seconds(30);CHECK(a.hash()==b.hash());}
TEST("life11",supervisor_request_does_not_directly_change_the_employees_action){auto w=adult_pair();auto& a=w.edit_for_test().actors[0];auto& b=w.edit_for_test().actors[1];CHECK(w.conversation_for_test(1,2));auto before=b.action.id;auto method=b.action.method;w.propose_social_for_test(a.id,Interaction::RequestWork,b.id);CHECK(b.action.id==before&&b.action.method==method);w.run_seconds(90);CHECK(b.employment.last_request!=0);}
TEST("life11",hidden_partner_mind_is_not_in_the_project_snapshot){auto a=World::generate(42,8);a.configure_life_projects();auto b=a;b.edit_for_test().actors[1].mind.social.expected_attraction={0,0,0};b.edit_for_test().actors[1].desire.value=.99;const auto x=a.personal_view(1),y=b.personal_view(1);auto px=project_options(x),py=project_options(y);CHECK(px.size()==py.size());for(std::size_t i=0;i<px.size();++i){auto dx=Planner::forecast(x,px[i]),dy=Planner::forecast(y,py[i]);CHECK(dx.score==dy.score&&dx.partner==dy.partner);}}
TEST("life11",parallelism_does_not_add_extra_hours_to_the_primary_day){auto w=adult_pair();CHECK(w.conversation_for_test(1,2));w.edit_for_test().autonomy=false;w.run_seconds(60);double total=0;for(auto t:w.state().actors[0].economy.time_ms)total+=t;NEAR(total,60000,1e-6);CHECK(w.state().actors[0].secondary_social_ms>0);}
TEST("life11",worker_snapshot_contains_unique_bounded_project_records){auto w=World::generate(42,8);w.configure_life_projects();w.run_seconds(7300);for(const auto& a:w.state().actors){std::vector<std::uint64_t> ids;for(const auto& p:a.cog.snapshot.life.projects)ids.push_back(p.id);std::sort(ids.begin(),ids.end());CHECK(std::adjacent_find(ids.begin(),ids.end())==ids.end());CHECK(ids.size()<=8);}}
TEST("projects",only_a_real_completed_procedure_updates_its_success_count){ProjectMemory m;m.enabled=true;m.procedures={familiar_procedure(GoalKind::LearnSkill,1)};GoalCandidate g;g.kind=GoalKind::LearnSkill;g.basis=2;const auto id=m.propose(g,5,0);auto successes=m.procedures[0].successes;m.activate(id,0);NEAR(m.procedures[0].successes,successes,0);CHECK(m.observe(id,StepOutcome::Success,3,1000));NEAR(m.procedures[0].successes,successes+1,0);CHECK(!m.observe(id,StepOutcome::Success,3,2000));NEAR(m.procedures[0].successes,successes+1,0);}
TEST("life11",an_empty_procedure_memory_cannot_invent_a_social_plan){auto w=adult_pair();w.run_seconds(120);for(const auto& a:w.state().actors){CHECK(a.mind.projects.projects.empty());CHECK(a.mind.projects.procedures.empty());}}
TEST("life11",an_addressed_goal_executes_the_whole_known_social_procedure){
 auto w=adult_pair();auto& s=w.edit_for_test();
 for(auto& a:s.actors)a.employment.active=false;
 auto& a=s.actors[0];auto& b=s.actors[1];
 a.mind.projects.procedures={familiar_procedure(GoalKind::SeekIntimacy,4001)};
 b.mind.projects.procedures={familiar_procedure(GoalKind::MeetPerson,4002)};
 a.mind.experience_contact(2,4003,0,300,.8,a.home,true);b.mind.experience_contact(1,4004,0,300,.8,a.home,true);
 GoalCandidate g;g.kind=GoalKind::SeekIntimacy;g.target=2;g.importance=1;g.expected_gain=1;g.success=1;g.basis=4005;g.deadline=4*3600000LL;
 auto id=a.mind.projects.propose(g,3,0);CHECK(id);CHECK(w.conversation_for_test(1,2));
 w.run_seconds(3*3600);
 CHECK(s.life.meetings_agreed>0);CHECK(s.life.intimacies>0);CHECK(a.mind.projects.find(id)->status==ProjectStatus::Completed);
}
TEST("life11",synchronized_walking_preserves_a_parallel_conversation){auto w=adult_pair();auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=s.actors[1].place=1;CHECK(w.conversation_for_test(1,2));for(int i=0;i<2;++i){auto& a=s.actors[i];a.action.method=Method::Visit;a.action.id=s.next_id++;a.action.phase=Phase::Travel;a.action.path={1,2};a.action.leg=0;a.action.destination=2;a.action.segment_start=0;a.action.end=90000;a.action.started=0;}w.run_seconds(60);CHECK(s.actors[0].conversation.id);CHECK(s.actors[0].secondary_social_ms>=60000);CHECK(s.actors[0].action.phase==Phase::Travel);}
TEST("life11",workers_reference_and_logs_preserve_new_profile_decisions){auto a=World::generate(42,8);a.configure_life_projects();auto b=a;b.set_workers(4,1);b.set_indexed(false);std::uint64_t logs=0;b.set_logger([&](const auto&){++logs;});a.run_seconds(7300);b.run_seconds(7300);CHECK(logs>0);CHECK(a.hash()==b.hash());}
TEST("life11",an_accepted_joint_activity_survives_save_restore){auto a=adult_pair();CHECK(a.conversation_for_test(1,2));a.propose_social_for_test(1,Interaction::PartnerIntimacy,2);await_acceptance(a,Interaction::PartnerIntimacy);a.edit_for_test().autonomy=false;a.run_seconds(300);a.save("/tmp/life11-joint.save");auto b=World::load("/tmp/life11-joint.save");a.run_seconds(1800);b.run_seconds(1800);CHECK(a.hash()==b.hash());CHECK(a.state().life.intimacies==1);}
TEST("life11",payroll_and_goals_survive_mid_shift_restore){auto a=World::generate(7,8);a.configure_life_projects();a.run_seconds(16*3600);a.save("/tmp/life11-payroll.save");auto b=World::load("/tmp/life11-payroll.save");a.run_seconds(2*3600);b.run_seconds(2*3600);CHECK(a.hash()==b.hash());CHECK(a.state().life.organizations[0].payroll.size()==8);}
TEST("life11",pure_snapshots_do_not_consult_hidden_partner_desire){auto a=World::generate(42,8);a.configure_life_projects();auto& actor=a.edit_for_test().actors[0];GoalCandidate g;g.kind=GoalKind::SeekIntimacy;g.target=2;g.importance=.9;g.basis=123;auto id=actor.mind.projects.propose(g,3,0);CHECK(id);auto b=a;b.edit_for_test().actors[1].mind.social.expected_attraction={0,0,0};b.edit_for_test().actors[1].desire.value=.99;auto x=a.personal_view(1),y=b.personal_view(1);x.life.people[0].visible=y.life.people[0].visible=true;for(auto* v:{&x,&y}){RelationshipView p;p.person=2;p.visible=true;p.introduced=true;p.basis=123;v->life.people={p};}auto px=project_options(x),py=project_options(y);CHECK(!px.empty());CHECK(px.size()==py.size());for(std::size_t i=0;i<px.size();++i)NEAR(Planner::forecast(x,px[i]).score,Planner::forecast(y,py[i]).score,0);}
TEST("life11_long",normal_week_keeps_a_progressing_event_queue){auto w=World::generate(42,16);w.configure_life_projects();try{w.run_seconds(7*86400);}catch(const std::exception& e){const auto& s=w.state();std::cerr<<"QUEUE now="<<s.now<<" physical="<<s.next_physical<<" payroll="<<s.life.next_payroll<<" supply="<<s.next_supply<<" "<<e.what()<<'\n';for(const auto& a:s.actors){std::cerr<<a.id<<" action="<<unsigned(a.action.phase)<<" "<<method_name(a.action.method)<<" end="<<a.action.end<<" window="<<a.action.next_window<<" conversation="<<a.conversation.id<<" conversation_end="<<a.conversation.ends<<" cogop="<<unsigned(a.cog.operation)<<" cogdue="<<a.cog.due<<" review="<<a.cog.review_at<<'\n';if(a.mind.social.active_event)for(const auto& ev:s.social.events)if(ev.id==a.mind.social.active_event)std::cerr<<"social="<<ev.id<<" phase="<<unsigned(ev.phase)<<" end="<<ev.ends<<'\n';}throw;}CHECK(w.state().now==7*86400000LL);for(const auto& a:w.state().actors){CHECK(a.alive);CHECK(a.body.damage<.1);}}
TEST("life11",late_arrival_to_work_cannot_schedule_an_event_in_the_past){auto w=World::generate(42,8);w.configure_life_projects();auto& s=w.edit_for_test();s.autonomy=false;s.now=17*3600000LL+1000;s.next_physical=s.now;s.life.next_payroll+=86400000;for(auto& a:s.actors)a.physical_at=s.now;s.actors[0].place=5;w.command_for_test(1,Method::Work,5);CHECK(s.actors[0].action.end>s.now);w.run_seconds(2);CHECK(s.actors[0].failures>0);CHECK(s.actors[0].economy.earned==0);}
TEST("life11",a_known_appointment_overlap_is_not_silently_accepted){auto w=adult_pair();auto& b=w.edit_for_test().actors[1];b.appointments.push_back({1234,2,3,2,3600000,7200000,false});CHECK(w.conversation_for_test(1,2));MeetingProposal p;p.host=1;p.place=2;p.at=4000000;p.until=7600000;w.invitation_for_test(1,2,p);w.run_seconds(90);CHECK(w.state().social.declined[std::size_t(Interaction::InviteMeeting)]==1);CHECK(w.state().life.meetings_agreed==0);}
TEST("life11",several_critical_body_signals_do_not_hide_water_behind_fear){auto w=World::generate(7,16);w.configure_life_projects();auto v=w.personal_view(12);v.need.fill(0);v.need[0]=.9;v.need[1]=.95;v.need[2]=1;v.need[8]=1;v.focus_metric=8;auto choices=Planner::ideas(v,w.state().random);CHECK(std::any_of(choices.begin(),choices.end(),[](const auto& x){return x.method==Method::Drink;}));CHECK(std::none_of(choices.begin(),choices.end(),[](const auto& x){return x.method==Method::Work;}));}

TEST("life11",project_terminal_forecast_is_not_discounted_twice){auto w=World::generate(42,8);w.configure_life_projects();auto& a=w.edit_for_test().actors[0];GoalCandidate g;g.kind=GoalKind::FindCompany;g.importance=1;g.success=1;g.expected_gain=1;g.basis=123;g.delay=5*3600000LL;auto id=a.mind.projects.propose(g,1,0);CHECK(id);auto v=w.personal_view(1);v.need.fill(0);v.norms.fill(0);v.risks.fill(0);for(auto& q:v.forecasts)q.fill(0);PlanOption o;o.method=Method::Visit;o.place=2;o.project=id;o.project_step=StepKind::Visit;auto d=Planner::forecast(v,o);CHECK(!d.path.empty());const double hours=d.time_cost*24/.1;cog04::Ledger ledger;ledger.impulse(1,0,0,project_forecast(v,o));ledger.flow(5,0,0,hours,-.1/24);NEAR(d.score,cog04::squash(ledger.value(std::max(24.,hours),cog04::cfg::discount_per_hour)),1e-12);}
