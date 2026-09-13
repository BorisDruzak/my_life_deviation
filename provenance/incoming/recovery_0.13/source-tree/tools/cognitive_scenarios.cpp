// Reproducible authored situations. Only initial conditions and explicit requests
// are supplied; later thoughts and actions come from the real integrated World.
#include "life/world.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
using namespace life;
namespace {
std::ofstream checked(const std::filesystem::path& p) {
 std::ofstream out(p,std::ios::binary);out.exceptions(std::ios::failbit|std::ios::badbit);return out;
}
void busy(const std::filesystem::path& dir,double bias,const std::string& label){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();s.autonomy=false;
 s.actors[0].place=s.actors[1].place=5;s.places[4].services|=service(Method::Talk);
 s.actors[0].social=.1;s.actors[0].cog.rejection_bias=bias;
 auto log=checked(dir/(label+".jsonl"));
 w.set_thought_logger([&](const auto& t){if(t.actor==1)log<<thought_json(t)<<'\n';});
 w.command_for_test(2,Method::Work,5);w.command_for_test(1,Method::Talk,5);
 s.autonomy=true;w.run_seconds(10);w.validate();
 auto summary=checked(dir/(label+"-world.json"));summary<<w.summary_json()<<'\n';
 if(w.state().actors[0].cog.contacts.empty())throw std::runtime_error("reply was not interpreted");
 std::cout<<label<<" bias="<<bias<<" stored_unpleasantness="
  <<std::setprecision(15)<<w.state().actors[0].cog.contacts.front().unpleasantness<<'\n';
}
void speed(const std::filesystem::path& dir,double value,const std::string& label){
 auto w=World::generate(42,8);w.edit_for_test().actors[0].mind.cognition.base[4]=value;
 auto log=checked(dir/(label+".jsonl"));Tick first=0;
 w.set_thought_logger([&](const Thought& t){if(t.actor!=1)return;log<<thought_json(t)<<'\n';if(t.kind==ThoughtKind::Intent&&!first)first=t.time;});
 w.run_seconds(20);std::cout<<label<<" speed="<<value<<" first_intent_ms="<<first<<'\n';
}
void income(const std::filesystem::path& dir){
 auto w=World::generate(42,8);auto& s=w.edit_for_test();auto& a=s.actors[0];
 s.ledger.initial_food-=a.food;s.ledger.initial_money-=a.money;
 a.food=0;a.money=0;a.mind.believed_money=0;a.body.energy=.3;
 a.mind.norms[std::size_t(Method::TakeFood)]=1;
 auto thoughts=checked(dir/"income-thoughts.jsonl");
 w.set_thought_logger([&](const auto& t){if(t.actor==1)thoughts<<thought_json(t)<<'\n';});
 auto events=checked(dir/"income-actions.jsonl");
 w.set_logger([&](const auto& e){if(e.actor==1)events<<"{\"ms\":"<<e.time<<",\"event\":"<<e.event<<",\"method\":\""<<method_name(e.method)<<"\",\"kind\":\""<<e.kind<<"\",\"result\":\""<<e.result<<"\"}\n";});
 w.run_seconds(10800);w.validate();auto summary=checked(dir/"income-world.json");summary<<w.summary_json()<<'\n';
 const auto& final=w.state().actors[0];
 std::cout<<"income actor1: work="<<final.completed[std::size_t(Method::Work)]<<" food="<<final.completed[std::size_t(Method::Eat)]<<" purchases="<<final.completed[std::size_t(Method::AcquireFood)]<<" alive="<<final.alive<<'\n';
}
}
int main(int argc,char**argv){try{const auto out=std::filesystem::path(argc>1?argv[1]:"cognitive_scenarios");std::filesystem::create_directories(out);busy(out,.1,"busy-low-bias");busy(out,.9,"busy-high-bias");income(out);speed(out,.1,"slow-processing");speed(out,.9,"fast-processing");return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
