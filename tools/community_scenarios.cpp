#include "life/world.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace life;
namespace {
World room(bool group){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.autonomy=false;
 for(auto& a:s.actors){auto& m=a.mind.social;m.enabled=a.id<=(group?4u:2u);m.methods.fill(MethodBelief{});m.community.entries.clear();m.community.shared.clear();a.social=.15;a.leisure=.9;
 m.methods[std::size_t(Interaction::ShareNews)]={.9,.3,.95,.9,s.next_id++};if(m.enabled){a.place=2;a.mind.known.fill(false);a.mind.known[std::size_t(Method::Talk)]=a.mind.known[std::size_t(Method::Social)]=true;}}
 w.command_for_test(1,Method::Talk,2);
 if(group){w.command_for_test(3,Method::Talk,2);for(unsigned n=2;n<4;++n){Percept p;p.token=1;p.since=p.last=0;p.exposure_id=10000+n;p.noticed=p.recognized=true;p.feature_work=1;s.actors[n].cog.percepts.push_back(p);}}
 s.autonomy=true;return w;}
Id put(World& w,Disclosure d){auto& s=w.edit_for_test();Information f;f.id=s.next_id++;f.subject=8;f.kind=NewsKind::Employment;f.other=5;f.material=.8;f.importance=.8;f.cited_source=1;f.disclosure=d;s.actors[0].mind.social.community.receive(f,0,s.next_id++,0);return Id(f.id);}
void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p);f.exceptions(std::ios::failbit|std::ios::badbit);f<<s<<'\n';}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::invalid_argument("community_scenarios OUTPUT_DIRECTORY");auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);
 for(const std::string name:{"public-group","secret-low","secret-high"}){auto out=root/name;std::filesystem::create_directories(out);auto w=room(name=="public-group");auto id=put(w,name=="public-group"?Disclosure::Open:Disclosure::Entrusted);w.edit_for_test().actors[0].mind.social.community.confidentiality=name=="secret-high"?1:0;
  std::ofstream events(out/"events.jsonl"),thoughts(out/"thoughts.jsonl");events.precision(15);
  w.set_thought_logger([&](const Thought& t){thoughts<<thought_json(t)<<'\n';});
  w.set_logger([&](const EventLog& e){events<<"{\"ms\":"<<e.time<<",\"actor\":"<<e.actor<<",\"event\":"<<e.event<<",\"kind\":\""<<e.kind<<"\",\"result\":\""<<e.result<<"\",\"partner\":"<<e.partner<<",\"interaction\":\""<<interaction_name(e.interaction)<<"\"}\n";});
  write(out/"initial.json",w.community_report_json());w.run_seconds(600);write(out/"summary.json",w.summary_json());write(out/"community.json",w.community_report_json());write(out/"fixture.json","{\"authored_initial_conversation\":true,\"autonomous_utterance_choice\":true,\"claim_id\":"+std::to_string(id)+"}");std::cout<<name<<" "<<w.hash()<<'\n';
 }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
