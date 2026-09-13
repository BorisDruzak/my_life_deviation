#include "test.hpp"
#include "life/world.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
using namespace life;
TEST("world", same_seed_same_initial_world){auto a=World::generate(42,16),b=World::generate(42,16);CHECK(a.state().actors.size()==16);CHECK(a.hash()==b.hash());a.validate();}
TEST("world", hundred_seeds_social_constraints){for(std::uint64_t seed=0;seed<100;++seed){auto w=World::generate(seed,128);w.validate();for(const auto& a:w.state().actors){CHECK(a.mind.relations.size()>=3);CHECK(a.mind.relations.size()<=32);}}}
TEST("world", invalid_population_rejected){THROWS(World::generate(1,1));THROWS(World::generate(1,7));THROWS(World::generate(1,5000));}
TEST("world", full_institutions_not_silently_enabled){THROWS(World::generate(1,16,"full-institutions"));}
TEST("world", reference_and_indexed_visibility){auto w=World::generate(7,16);w.edit_for_test().actors[0].place=2;w.edit_for_test().actors[1].place=2;w.set_indexed(false);auto a=w.visible_for_test(1);w.set_indexed(true);auto b=w.visible_for_test(1);CHECK(a==b);}
TEST("world", hidden_stock_does_not_change_personal_decision){auto a=World::generate(42,16),b=a;b.edit_for_test().places[2].food=0;auto av=a.personal_view(1),bv=b.personal_view(1);auto x=Planner::choose(av,a.state().random),y=Planner::choose(bv,b.state().random);CHECK(x.method==y.method&&x.place==y.place&&x.score==y.score);}
TEST("world", consumption_does_not_charge_money){auto w=World::generate(42,8);w.edit_for_test().autonomy=false;auto money=w.state().actors[0].money;w.command_for_test(1,Method::Eat,w.state().actors[0].home);w.run_seconds(100);CHECK(w.state().actors[0].money==money);CHECK(w.state().actors[0].food==0);CHECK(w.state().ledger.eaten_food==1);w.validate();}
TEST("world", competition_for_last_unit){auto w=World::generate(2,8);auto& s=w.edit_for_test();s.autonomy=false;auto initial=s.places[2].food;s.places[2].food=1;s.ledger.initial_food-=initial-1;s.actors[0].place=s.actors[1].place=3;auto a=s.actors[0].food,b=s.actors[1].food;w.command_for_test(1,Method::AcquireFood,3);w.command_for_test(2,Method::AcquireFood,3);w.run_seconds(65);CHECK(w.state().actors[0].food+w.state().actors[1].food==a+b+1);CHECK(w.state().places[2].food==0);w.validate();}
TEST("world", desire_satisfaction_requires_completed_action){auto w=World::generate(3,8);w.edit_for_test().autonomy=false;double d=w.state().actors[0].desire.deficit;w.command_for_test(1,Method::PrivateIntimacy,w.state().actors[0].home);w.run_seconds(1799);CHECK(w.state().actors[0].desire.deficit>=d);w.run_seconds(2);CHECK(w.state().actors[0].desire.deficit<d);}
TEST("world", split_advance_same_state){auto a=World::generate(9,16),b=a;a.run_seconds(3600);b.run_seconds(1700);b.run_seconds(1900);CHECK(a.hash()==b.hash());}
TEST("world", indexed_and_full_scan_same_simulation){auto a=World::generate(9,16),b=a;b.set_indexed(false);a.run_seconds(3600);b.run_seconds(3600);CHECK(a.hash()==b.hash());}
TEST("world", save_resume_exact){auto a=World::generate(4,16);a.run_seconds(777);auto file=(std::filesystem::temp_directory_path()/"life-test-state.save").string();a.save(file);auto b=World::load(file);CHECK(a.hash()==b.hash());a.run_seconds(1234);b.run_seconds(1234);CHECK(a.hash()==b.hash());std::filesystem::remove(file);}
TEST("world", damaged_save_rejected){auto a=World::generate(4,8);auto file=(std::filesystem::temp_directory_path()/"life-test-damaged.save").string();a.save(file);{std::fstream f(file,std::ios::in|std::ios::out|std::ios::binary);f.seekp(90);f.put('x');}THROWS(World::load(file));std::filesystem::remove(file);}
TEST("world", day_has_actual_actions){auto w=World::generate(42,16);w.run_seconds(86400);w.validate();std::uint64_t n=0;for(const auto& a:w.state().actors){CHECK(a.alive);n+=a.completed[1]+a.completed[2];}CHECK(n>16);}
TEST("world", waiting_person_can_accept_contact){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();s.autonomy=false;
 s.actors[0].place=2;s.actors[0].social=s.actors[1].social=.3;
 w.command_for_test(1,Method::Talk,2);s.actors[1].place=2;w.command_for_test(2,Method::Talk,2);w.run_seconds(40);
 CHECK(w.state().actors[0].action.phase==Phase::Running);CHECK(w.state().actors[0].action.partner==2);CHECK(w.state().actors[0].action.id==w.state().actors[1].action.id);
}
TEST("world", refusal_does_not_immediately_repeat_same_method){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=2;
 w.command_for_test(1,Method::Talk,2);w.run_seconds(31);
 auto v=w.personal_view(1);v.need.fill(0);v.need[5]=1;
 auto d=Planner::choose(v,w.state().random);CHECK(d.method!=Method::Talk);
}
TEST("world", rest_learning_uses_experienced_result){
 auto w=World::generate(3,8);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].body.fatigue=0;
 w.command_for_test(1,Method::Rest,s.actors[0].home);w.run_seconds(901);
 const auto& q=w.state().actors[0].mind.learning.q[4][3];CHECK(q.count>0);NEAR(q.mean,0,1e-12);
}
TEST("world", interrupted_talk_stops_both_participants){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=s.actors[1].place=2;
 s.actors[0].social=s.actors[1].social=.3;w.command_for_test(1,Method::Talk,2);w.run_seconds(10);
 w.command_for_test(1,Method::Leisure,2);CHECK(w.state().actors[1].action.phase==Phase::Idle);
}
TEST("world", acquiring_food_is_not_nutritional_experience){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=3;
 const auto count=s.actors[0].mind.learning.q[8][0].count;
 w.command_for_test(1,Method::AcquireFood,3);w.run_seconds(61);
 CHECK(w.state().actors[0].mind.learning.q[8][0].count==count);
}
TEST("world", trace_does_not_change_state){auto a=World::generate(42,16),b=a;std::uint64_t logs=0;b.set_logger([&](const EventLog&){++logs;});a.run_seconds(3600);b.run_seconds(3600);CHECK(logs>0);CHECK(a.hash()==b.hash());}
TEST("world", cancelled_desire_action_does_not_satisfy){auto w=World::generate(3,8);auto& s=w.edit_for_test();s.autonomy=false;double d=s.actors[0].desire.deficit;w.command_for_test(1,Method::PrivateIntimacy,s.actors[0].home);w.run_seconds(100);w.command_for_test(1,Method::Rest,s.actors[0].home);CHECK(w.state().actors[0].desire.deficit>=d);CHECK(w.state().actors[0].desire.applied_results.empty());}
TEST("world", finite_norm_changes_take_decision){auto w=World::generate(42,8);auto v=w.personal_view(1);v.food=0;v.need.fill(0);v.need[0]=1;v.risks.fill(0);v.norms.fill(0);v.money=0;v.known.fill(false);v.known[9]=true;auto d=Planner::choose(v,w.state().random);CHECK(d.method==Method::TakeFood);v.norms[9]=1;CHECK(Planner::choose(v,w.state().random).method!=Method::TakeFood);}
TEST("world", expired_retry_does_not_turn_false_fact_true){
 auto w=World::generate(42,8);auto v=w.personal_view(1);v.food=0;v.need.fill(0);v.need[0]=1;v.known.fill(false);v.known[8]=v.known[9]=true;v.norms.fill(0);v.risks.fill(0);
 for(auto& p:v.places)if(p.id==3){p.food=Truth::Refuted;p.retry_at=0;}
 CHECK(Planner::choose(v,w.state().random).method!=Method::AcquireFood);CHECK(Planner::choose(v,w.state().random).method!=Method::TakeFood);
}
TEST("world", unavailable_food_does_not_prevent_accessible_water){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();auto& a=s.actors[0];s.ledger.initial_food-=a.food+s.places[2].food;a.food=0;a.mind.believed_food=0;s.places[2].food=0;
 a.body.energy=.02;a.body.water=.05;for(auto& p:a.mind.places)if(p.id==3)p.food=Truth::Refuted;
 w.run_seconds(900);CHECK(w.state().actors[0].consumed_water>0);
}
