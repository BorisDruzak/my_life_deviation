#include "test.hpp"
#include "life/projects.hpp"
#include "life/archive.hpp"
#include <limits>
using namespace life;
namespace {
ProjectMemory seeded(){ProjectMemory m;m.enabled=true;m.procedures.push_back(familiar_procedure(GoalKind::MeetPerson,100));return m;}
GoalCandidate meeting(){GoalCandidate g;g.kind=GoalKind::MeetPerson;g.target=2;g.place=2;g.not_before=100;g.deadline=86400000;g.basis=77;return g;}
}
TEST("projects",proposal_is_persistent_not_an_atomic_action){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(id);CHECK(m.find(id));CHECK(m.find(id)->status==ProjectStatus::Candidate);CHECK(m.created==1);CHECK(m.completed==0);}
TEST("projects",repeated_thought_does_not_duplicate_goal){auto m=seeded();auto a=m.propose(meeting(),2,0);auto b=m.propose(meeting(),2,1);CHECK(a&&a==b);CHECK(m.projects.size()==1);}
TEST("projects",unknown_procedure_is_not_invented){ProjectMemory m;m.enabled=true;CHECK(m.propose(meeting(),99,0)==0);CHECK(m.projects.empty());}
TEST("projects",not_before_is_respected){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(id);CHECK(!m.activate(id,99));CHECK(m.activate(id,100));}
TEST("projects",paused_project_resumes_without_restarting){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(m.activate(id,100));CHECK(m.observe(id,StepOutcome::Success,1000,200));auto step=m.find(id)->step;CHECK(m.pause(id,201,500));CHECK(!m.activate(id,499));CHECK(m.activate(id,500));CHECK(m.find(id)->step==step);CHECK(m.pauses==1&&m.resumes==1);}
TEST("projects",a_replayed_result_is_not_a_second_step){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(m.activate(id,100));CHECK(m.observe(id,StepOutcome::Success,1000,200));auto step=m.find(id)->step;CHECK(!m.observe(id,StepOutcome::Success,1000,201));CHECK(m.find(id)->step==step);}
TEST("projects",refusal_delays_instead_of_forcing_another_try){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(m.activate(id,100));CHECK(m.observe(id,StepOutcome::Refused,1000,200));CHECK(m.find(id)->status==ProjectStatus::Waiting);CHECK(m.find(id)->retry_at>200);CHECK(!m.activate(id,201));}
TEST("projects",expired_goal_cannot_resume){auto m=seeded();auto g=meeting();g.deadline=200;auto id=m.propose(g,2,0);m.review(201);CHECK(id);CHECK(m.find(id)->status==ProjectStatus::Expired);CHECK(!m.activate(id,202));}
TEST("projects",abandoned_goal_cannot_resume){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(m.abandon(id,10));CHECK(!m.activate(id,100));}
TEST("projects",unknown_person_is_a_valid_search_goal){ProjectMemory m;m.enabled=true;m.procedures.push_back(familiar_procedure(GoalKind::FindCompany,9));GoalCandidate g;g.basis=8;auto id=m.propose(g,1,0);CHECK(id);CHECK(m.find(id)->goal.target==0);}
TEST("projects",the_address_is_required_for_a_specific_meeting){auto m=seeded();auto g=meeting();g.target=0;THROWS(m.propose(g,2,0));}
TEST("projects",delayed_reward_can_outweigh_an_immediate_one){GoalCandidate g;g.importance=1;g.expected_gain=.9;g.success=.9;g.remaining_cost=.1;g.delay=5*3600000;CHECK(continuation_value(g,.03)>.2);}
TEST("projects",discount_changes_future_not_current_reward){GoalCandidate g;g.importance=1;g.expected_gain=.9;g.success=1;g.remaining_cost=0;g.delay=36000000;CHECK(continuation_value(g,.5)<continuation_value(g,.01));g.delay=0;NEAR(continuation_value(g,.5),continuation_value(g,.01),1e-12);}
TEST("projects",bad_numbers_cannot_create_a_goal){auto m=seeded();auto g=meeting();g.importance=std::nan("");THROWS(m.propose(g,2,0));NeedThreshold n;n.release=.5;THROWS(n.validate());}
TEST("projects",individual_thresholds_produce_different_activation){NeedThreshold a{.2,.15,.6},b{.5,.45,.8};CHECK(a.activation(.3)>0);CHECK(b.activation(.3)==0);}
TEST("projects",hysteresis_prevents_threshold_chatter){NeedThreshold n{.3,.2,.7};CHECK(n.activation(.25,false)==0);CHECK(n.activation(.25,true)>0);CHECK(n.activation(.19,true)==0);}
TEST("projects",satiation_lowers_predicted_gain_without_moral_penalty){CHECK(predicted_satiated_gain(.6,.8)<predicted_satiated_gain(.6,.1));NEAR(predicted_satiated_gain(.6,0),.6,1e-12);}
TEST("projects",serialized_project_keeps_deadline_cursor_and_delay){auto m=seeded();auto id=m.propose(meeting(),2,0);CHECK(id);CHECK(m.activate(id,100));CHECK(m.observe(id,StepOutcome::Success,1000,200));CHECK(m.pause(id,201,500));Writer w;w(m);ProjectMemory copy;Reader r(w.data);r(copy);CHECK(r.finished());CHECK(copy.find(id));CHECK(copy.find(id)->step==m.find(id)->step);CHECK(copy.find(id)->retry_at==500);CHECK(copy.activate(id,500));copy.validate(500);}
