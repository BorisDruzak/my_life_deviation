#include "test.hpp"
#include "life/world.hpp"
#include <algorithm>
using namespace life;
TEST("adaptive",talk_requires_an_observed_local_addressee) {
 auto w=World::generate(42,8);w.configure_life_projects();auto v=w.personal_view(1);
 v.need.fill(0);v.need[5]=1;v.perceived_people.clear();v.social.enabled=false;v.life.enabled=false;
 const auto options=Planner::ideas(v,w.state().random);
 CHECK(std::none_of(options.begin(),options.end(),[](const auto& o){return o.method==Method::Talk&&o.partner==0;}));
}
TEST("adaptive",known_buy_eat_chain_keeps_future_food_value) {
 auto w=World::generate(42,8);w.configure_life_projects();auto v=w.personal_view(1);
 v.food=0;v.money=20;v.need.fill(.6);v.need[0]=.99;v.forecasts[std::size_t(Method::AcquireFood)].fill(0);
 auto d=Planner::forecast(v,{Method::AcquireFood,3,0,1});
 CHECK(!d.path.empty());CHECK(d.score>0);
 CHECK(d.resource_cost>.1);
}
#if __has_include("life/deliberation.hpp")
#include "life/deliberation.hpp"
TEST("adaptive",money_urgency_does_not_allow_worse_work_interrupt) {
 Outcomes needs{};needs[7]=1;
 auto r=assess_interruption(Method::Work,.33,Method::Leisure,.05,needs,.06);
 CHECK(!r.change);CHECK(r.reason==SwitchReason::InsufficientGain);
}
TEST("adaptive",emergency_interrupt_must_address_that_emergency) {
 Outcomes needs{};needs[1]=.95;
 CHECK(assess_interruption(Method::Work,.8,Method::Drink,.1,needs,.06).change);
 CHECK(!assess_interruption(Method::Work,.8,Method::Leisure,.1,needs,.06).change);
}
TEST("adaptive",new_better_information_can_change_intention) {
 Outcomes needs{};
 CHECK(assess_interruption(Method::Leisure,.1,Method::Work,.4,needs,.06).change);
}
#else
TEST("adaptive",reasoned_interruption_policy_must_exist){CHECK(false && "missing interruption policy");}
#endif
TEST("adaptive",known_sleep_prerequisite_resolves_hunger_before_retrying_sleep) {
 auto w=World::generate(7,8);w.configure_life_projects();auto v=w.personal_view(1);
 v.need.fill(0);v.need[0]=v.need[2]=1;v.food=0;v.money=100;
 const auto opts=Planner::ideas(v,w.state().random);
 CHECK(std::none_of(opts.begin(),opts.end(),[](const auto& o){return o.method==Method::Sleep;}));
 CHECK(std::any_of(opts.begin(),opts.end(),[](const auto& o){return o.method==Method::AcquireFood;}));
}

TEST("adaptive",unaffordable_food_does_not_make_starvation_sleep_possible) {
 auto w=World::generate(7,8);w.configure_life_projects();auto v=w.personal_view(1);
 v.need.fill(0);v.need[0]=v.need[2]=1;v.food=0;v.money=0;
 auto options=Planner::ideas(v,w.state().random);
 CHECK(std::none_of(options.begin(),options.end(),[](const auto& p){return p.method==Method::Sleep;}));
}
TEST("adaptive",completed_search_does_not_reuse_unchanged_location_as_new_lead) {
 auto w=World::generate(42,8);w.configure_life_projects();auto v=w.personal_view(1);
 PersonalProject p;p.id=777;p.procedure=2;p.status=ProjectStatus::Active;p.goal.kind=GoalKind::MeetPerson;p.goal.target=2;p.goal.basis=1;p.searched_place=3;p.searched_at=1000;
 v.now=2000;v.life.projects={p};v.life.people.clear();RelationshipView who;who.person=2;who.last_known_place=3;who.last_seen=500;v.life.people.push_back(who);
 auto options=project_options(v);CHECK(std::none_of(options.begin(),options.end(),[](const auto& p){return p.method==Method::Visit&&p.place==3;}));
 v.life.people[0].last_seen=1500;options=project_options(v);CHECK(std::any_of(options.begin(),options.end(),[](const auto& p){return p.method==Method::Visit&&p.place==3;}));
}

TEST("adaptive",return_obligation_can_direct_paid_recognition_after_item_use) {
 auto w=World::generate(42,8);w.configure_social_scene("borrow");
 unsigned recognitions=0;
 w.set_thought_logger([&](const Thought& t){if(t.actor==1&&t.origin==Origin::Observed&&t.kind==ThoughtKind::Recall&&t.detail=="знакомый_образ_после_наблюдения")++recognitions;});
 w.run_seconds(900);
 CHECK(w.state().social.loans.size()==1);
 CHECK(w.state().social.loans.front().returned);
 CHECK(recognitions>0);
}
