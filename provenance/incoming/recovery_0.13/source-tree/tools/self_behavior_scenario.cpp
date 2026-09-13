#include "life/world.hpp"
#include "valuation.hpp"
#include "attention.hpp"
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace life;
SelfModel history(double own) {
 SelfModel m; Cognitive c;
 for(unsigned i=1;i<=20;++i){InterpretedEpisode e;e.id=e.source_event=e.source_action=i;e.actual=e.intentional=true;
  e.domain=SelfDomain::Social;e.success=0;e.importance=e.perception_quality=1;e.adversity=.8;
  e.attribution={.95,0,0,.05,.25,1};m.integrate(e,c,.5);}
 if(own>=0)for(unsigned i=21;i<=40;++i){InterpretedEpisode e;e.id=e.source_event=e.source_action=i;e.actual=e.intentional=true;
  e.domain=SelfDomain::Social;e.success=1;e.importance=e.perception_quality=1;
  e.attribution={own,0,0,1-own,.15+.8*own,1};m.integrate(e,c,.5);}
 return m;
}
int main(){
 try{
  auto w=World::generate(42,8);auto v=w.personal_view(1);v.self_enabled=true;v.life.enabled=false;v.social.enabled=false;v.economy.enabled=false;
  v.place=2;v.map.clear();v.places.clear();KnownPlace place;place.id=2;place.services=service(Method::Talk)|service(Method::Leisure);v.places.push_back(place);
  v.perceived_people={2};v.known.fill(false);v.known[std::size_t(Method::Talk)]=v.known[std::size_t(Method::Leisure)]=true;
  v.norms.fill(0);v.risks.fill(0);v.capability.gate=1;v.capability.alternatives=4;v.capability.operations=16;v.capability.context=4;
  for(auto& f:v.forecasts)f.fill(0);v.forecasts[std::size_t(Method::Talk)][5]=.6;v.forecasts[std::size_t(Method::Leisure)][4]=.45;
  std::array<SelfModel,3> models{history(-1),history(.95),history(.01)};std::array<int,3> initiatives{},declines{},operations{};
  std::array<double,3> times{};int improved_pairs=0,worsened_pairs=0;
  for(unsigned i=0;i<100;++i){v.episode=i;v.need.fill(0);v.need[5]=.2+.8*w.state().random.uniform("self-opportunity-social",1,i);v.need[4]=.15+.65*w.state().random.uniform("self-opportunity-leisure",1,i);
   std::array<bool,3> did{};
   for(unsigned k=0;k<3;++k){for(unsigned j=0;j<self_domain_count;++j)v.self_predictions[j]=models[k].predict(SelfDomain(j));
    const auto d=Planner::choose(v,w.state().random);did[k]=d.method==Method::Talk;initiatives[k]+=did[k];declines[k]+=!did[k];operations[k]+=d.operations;
    times[k]+=d.operations/cog04::operation_rate(v.capability.current[4],true);
   }
   improved_pairs+=!did[0]&&did[1];worsened_pairs+=did[0]&&!did[1];
  }
  std::cout<<std::setprecision(17)<<"{\"opportunities\":100,\"same_external_inputs\":true,\"variants\":[";
  for(unsigned k=0;k<3;++k){if(k)std::cout<<',';auto p=models[k].predict(SelfDomain::Social);
   std::cout<<"{\"variant\":\""<<(k==0?"before":k==1?"self_attributed_success":"luck_attributed_success")<<"\",\"initiatives\":"<<initiatives[k]<<",\"non_social_choices\":"<<declines[k]<<",\"mean_operations\":"<<operations[k]/100.<<",\"charged_operation_seconds\":"<<times[k]/100.<<",\"efficacy\":"<<p.efficacy<<",\"control\":"<<p.control<<'}';}
  std::cout<<"],\"discordant_improved\":"<<improved_pairs<<",\"discordant_worsened\":"<<worsened_pairs<<"}\n";
  if(initiatives[1]<=initiatives[0]||initiatives[1]<=initiatives[2]||improved_pairs<8||worsened_pairs)throw std::runtime_error("matched social choice acceptance failed");
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
