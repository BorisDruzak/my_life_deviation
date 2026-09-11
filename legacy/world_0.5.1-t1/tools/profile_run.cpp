#include "npc/controller.hpp"
#include <chrono>
#include <iostream>
using namespace npc;
int main(int argc,char**argv){auto c=Config::load("game/rules");auto s=read_json("game/scenarios/household.json");World w=World::from_scenario(c,at(s,"world"));double view=0,decision=0,advance=0,checks=0;using C=std::chrono::steady_clock;auto elapsed=[](auto start){return std::chrono::duration<double>(C::now()-start).count();};int count=argc>1?std::stoi(argv[1]):1440;for(int i=0;i<count;++i){for(auto&kv:at(s,"controllers").as_object()){auto id=std::string(kv.key());if(!w.can_decide(id))continue;auto t=C::now();auto v=w.view(id);view+=elapsed(t);t=C::now();auto d=decide(c,v,kv.value());decision+=elapsed(t);if(d.command)w.submit(*d.command);}auto t=C::now();w.advance();advance+=elapsed(t);if(i%60==0){t=C::now();w.check_invariants();checks+=elapsed(t);}}std::cout<<"view="<<view<<" decision="<<decision<<" advance="<<advance<<" checks="<<checks<<" events="<<w.debug_state().ledger.size()<<'\n';}
