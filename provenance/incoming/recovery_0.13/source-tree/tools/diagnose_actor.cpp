#include "life/world.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
using namespace life;
int main(int argc,char**argv){try{
 if(argc!=5)throw std::runtime_error("seed actor hours output-prefix");
 auto seed=std::stoull(argv[1]);auto who=std::stoul(argv[2]);auto hours=std::stoul(argv[3]);std::string prefix=argv[4];
 auto w=World::generate(seed,16);w.configure_life_projects();w.configure_self_model();
 std::ofstream snapshots(prefix+".snapshots.jsonl"),decisions(prefix+".decisions.jsonl"),events(prefix+".events.jsonl");
 w.set_thought_logger([&](const Thought& t){if(t.actor!=who)return;
  if(t.kind==ThoughtKind::Intent||t.kind==ThoughtKind::Forecast||t.kind==ThoughtKind::Interpret)decisions<<thought_json(t)<<'\n';});
 w.set_logger([&](const EventLog& e){if(e.actor==who)events<<"{\"ms\":"<<e.time<<",\"method\":\""<<method_name(e.method)<<"\",\"kind\":\""<<e.kind<<"\",\"result\":\""<<e.result<<"\"}\n";});
 for(unsigned i=0;i<hours;++i){w.run_seconds(3600);const auto& a=w.state().actors.at(who-1);
  snapshots<<std::setprecision(17)<<"{\"hour\":"<<i+1<<",\"energy\":"<<a.body.energy<<",\"water\":"<<a.body.water<<",\"sleep\":"<<a.body.sleep<<",\"food\":"<<a.food<<",\"believed_food\":"<<a.mind.believed_food<<",\"money\":"<<a.money<<",\"method\":\""<<method_name(a.action.method)<<"\",\"phase\":"<<unsigned(a.action.phase)<<",\"place\":"<<a.place<<",\"focus\":"<<unsigned(a.cog.focus_metric)<<",\"budget\":"<<a.cog.budget<<",\"gate\":"<<capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep).gate<<",\"known_food\":[";
  bool first=true;for(const auto& p:a.mind.places)if(p.services&service(Method::AcquireFood)){if(!first)snapshots<<',';first=false;snapshots<<"{\"place\":"<<p.id<<",\"truth\":"<<unsigned(p.food)<<",\"retry\":"<<p.retry_method[std::size_t(Method::AcquireFood)]<<'}';}
  snapshots<<"]}\n";snapshots.flush();if(!a.alive)break;
 }
 std::cout<<w.self_report_json()<<'\n';
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
