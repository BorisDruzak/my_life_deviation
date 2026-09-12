#include "life/numeric.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace life {
namespace {
void finite(double x){if(!std::isfinite(x))throw std::invalid_argument("non-finite scalar");}
void time_ok(double seconds){finite(seconds);if(seconds<0)throw std::invalid_argument("negative interval");}
constexpr std::array<double,10> rise{1,10,3,20,5,.5,2,3,8,20};
constexpr std::array<double,10> fall{30,300,180,1200,180,3,60,90,600,1800};
constexpr double cognitive_weights[8][7]={
 {.35,.10,.15,.10,.25,.20,.20},{.60,.15,.20,.10,.50,.25,.45},
 {.50,.10,.20,.10,.35,.20,.40},{.30,.10,.15,.05,.25,.15,.25},
 {.40,.20,.20,.10,.30,.20,.30},{.45,.15,.15,.10,.30,.20,.45},
 {.40,.10,.15,.10,.25,.15,.50},{.45,.15,.15,.10,.35,.20,.35}};
std::array<double,7> deficits(const Body& b){return {unit((b.sleep-.30)/.70),b.fatigue,unit((.50-b.water)/.50),unit((.35-b.energy)/.35),b.pain,std::abs(b.temperature),unit((b.activation-.65)/.35)};}
}
void require_range(double x,double lo,double hi){finite(x);if(x<lo||x>hi)throw std::invalid_argument("scalar out of range");}
double unit(double x){finite(x);return std::clamp(x,0.0,1.0);}
double signed_unit(double x){finite(x);return std::clamp(x,-1.0,1.0);}
double approach(double x,double target,double seconds,double tau){
 finite(x);finite(target);time_ok(seconds);finite(tau);if(tau<=0)throw std::invalid_argument("non-positive time constant");
 if(seconds==0)return x;
 return target+(x-target)*std::exp(-seconds/tau);
}
double saturation(double x,double a,double b,double leak,double hours){
 require_range(x,0,1);require_range(a,0,2);require_range(b,0,2);require_range(leak,0,2);time_ok(hours);
 if(hours==0)return x;
 if(a+b==0)return unit(x-leak*hours);
 return unit(approach(x,(a-leak)/(a+b),hours,1/(a+b)));
}
void advance_body(Body& b,const Biology& p,const Input& in,const Emotions& e,double seconds){
 time_ok(seconds);
 for(double x:{b.energy,b.water,b.sleep,b.fatigue,b.damage,b.pain,b.oxygen,b.activation})require_range(x,0,1);
 require_range(b.temperature,-1,1);require_range(in.temperature,-1,1);require_range(in.activation_effect,-.3,.3);
 for(double x:{in.load,in.oxygen,in.pain,in.disease,in.safety})require_range(x,0,1);
 for(double x:e)require_range(x,0,1);
 for(double x:{p.metabolism,p.water_rate,p.endurance,p.recovery})require_range(x,.5,1.5);
 require_range(p.sleep_need,.75,1.5);require_range(p.pain_sensitivity,0,1.5);
 if(seconds==0)return;
 const Body old=b;const double h=seconds/hour,n=in.load,s=in.asleep?1.0:0.0;
 b.energy=unit(old.energy-h*.035*p.metabolism*(1+n+.4*std::max(0.0,-old.temperature)+.15*old.activation));
 b.water=unit(old.water-h*.045*p.water_rate*(1+1.5*n+2*std::max(0.0,old.temperature)+.15*old.activation));
 const double quality=(1-.5*old.pain)*(1-.5*std::abs(old.temperature))*(1-.3*old.activation);
 b.sleep=unit(old.sleep+h*(.045*p.sleep_need*(1-s)-.09*p.recovery*quality*s));
 b.fatigue=unit(old.fatigue+h*(.4*n*n*(1+.5*old.oxygen+.3*old.damage)/p.endurance-.25*p.recovery*(1-n)*(.4+.6*s)*old.energy*old.water));
 b.oxygen=unit(approach(old.oxygen,in.oxygen,seconds,in.oxygen>old.oxygen?20:10));
 const double target_pain=unit(p.pain_sensitivity*in.pain);
 b.pain=unit(approach(old.pain,target_pain,seconds,target_pain>old.pain?2:30));
 b.temperature=signed_unit(approach(old.temperature,in.temperature,seconds,1800));
 const double rE=unit((.1-old.energy)/.1),rW=unit((.1-old.water)/.1),rT=unit((std::abs(old.temperature)-.7)/.3),rO=unit((old.oxygen-.7)/.3);
 b.damage=unit(old.damage+h*(in.disease+.03*rE*rE+.04*rW*rW+.04*rT*rT+12*rO*rO-.005*p.recovery*old.energy*old.water*(1+.5*s)));
 const double target=unit((1-.5*s)*(.1+.5*n+.65*std::max(e[0],e[2])+.3*e[1]+.2*e[4]+.15*e[7]+.4*old.oxygen-.1*in.safety)+in.activation_effect);
 b.activation=unit(approach(old.activation,target,seconds,target>old.activation?5:45));
}
std::array<double,8> signals(const Body& b,const Biology& p){
 std::array<double,8> d{unit((.65-b.energy)/.65),unit((.70-b.water)/.70),unit((b.sleep-.20)/.80),b.fatigue,b.pain,unit((-b.temperature-.2)/.8),unit((b.temperature-.2)/.8),b.oxygen};
 for(std::size_t i=0;i<d.size();++i){require_range(p.sensitivity[i],0,1.5);d[i]=unit(p.sensitivity[i]*d[i]*(1+.5*d[i]));}return d;
}
double sensation(double s,double share,double anxiety){require_range(s,0,1);require_range(share,0,1);require_range(anxiety,0,1);return unit(s*(.35+.65*share)*(1+.2*anxiety));}
Capability capability(const Body& b,const Cognitive& c,bool asleep,bool conscious){
 Capability r;r.gate=asleep||!conscious?0:unit((.95-b.oxygen)/.35);auto d=deficits(b);
 for(std::size_t j=0;j<8;++j){require_range(c.base[j],0,1);r.current[j]=c.base[j]*r.gate;for(std::size_t i=0;i<7;++i)r.current[j]*=1-cognitive_weights[j][i]*d[i];}
 if(r.gate==0)return r;
 r.context=std::clamp(int(std::floor(2+6*r.current[2]+4*r.current[1])),2,12);
 r.operations=4+int(std::floor(28*r.current[4]));r.depth=1+int(std::floor(3*r.current[5]));r.alternatives=1+int(std::floor(5*r.current[5]));return r;
}
Emotions appraisal_targets(const Appraisal& a,const Body& b){
 const double z=a.significance,v=a.harm,near=a.immediacy,k=a.control;
 for(double x:{z,v,near,k,a.uncertainty,a.obstacle,a.blame,a.loss,a.progress,a.positive_error,a.discrepancy,a.novelty,a.comprehension,a.rejection,a.disapproval,a.violation,a.responsibility,a.question_usefulness})require_range(x,0,1);
 require_range(a.pleasantness,-1,1);const double good=std::max(0.0,a.pleasantness);
 Emotions r{z*v*near*(.35+.65*(1-k)), z*v*(1-.8*near)*(.35+.65*a.uncertainty)*(1-.5*k),
 z*std::max(a.obstacle,v*near)*a.blame*(.4+.6*k),z*a.loss*(.4+.6*(1-k)),
 z*unit(.5*good+.3*a.progress+.2*a.positive_error),z*a.discrepancy,z*a.rejection,
 z*unit(.55*a.novelty*(.3+.7*a.comprehension)+.30*good+.15*a.question_usefulness)*(1-.7*v*near),z*a.disapproval,z*a.violation*a.responsibility};
 auto d=deficits(b);const double reactivity=std::clamp(1+.4*d[0]+.3*d[4]+.2*d[1],1.0,1.6);
 for(std::size_t i:{0u,1u,2u,3u,8u,9u})r[i]=unit(r[i]*reactivity);
 // v0.2 replaces joy/interest and removes their old second discomfort penalty.
 return r;
}
void Affect::set(std::uint64_t id,Tick expires,const Emotions& target){
 for(double x:target)require_range(x,0,1);
 auto it=std::find_if(causes.begin(),causes.end(),[&](const auto& c){return c.id==id;});
 if(it==causes.end()){causes.push_back({id,expires,target,{}});}else{it->expires=expires;it->target=target;}
 for(std::size_t j=0;j<emotion_count;++j){
  std::vector<std::size_t> active;
  for(std::size_t i=0;i<causes.size();++i)if(causes[i].value[j]>0||causes[i].target[j]>0)active.push_back(i);
  if(active.size()>16){std::stable_sort(active.begin(),active.end(),[&](auto x,auto y){const auto& a=causes[x];const auto& b=causes[y];if(a.value[j]!=b.value[j])return a.value[j]>b.value[j];if(a.expires!=b.expires)return a.expires>b.expires;return a.id<b.id;});for(std::size_t k=16;k<active.size();++k){causes[active[k]].value[j]=0;causes[active[k]].target[j]=0;}}
 }
}
void Affect::advance(Tick now,Tick delta){
 if(delta<0)throw std::invalid_argument("negative affect interval");
 if(delta==0)return;
 total.fill(0);
 for(auto& c:causes){
  const Tick active=std::clamp(c.expires-now,Tick{0},delta);
  for(std::size_t j=0;j<emotion_count;++j){
   if(active>0)c.value[j]=approach(c.value[j],c.target[j],double(active)/1000,c.target[j]>c.value[j]?rise[j]:fall[j]);
   if(active<delta)c.value[j]=approach(c.value[j],0,double(delta-active)/1000,fall[j]);
   if(now+delta>=c.expires&&c.value[j]<.001)c.value[j]=0;
   total[j]=std::max(total[j],c.value[j]);
  }
 }
 std::erase_if(causes,[&](const auto& c){return now+delta>=c.expires&&std::all_of(c.value.begin(),c.value.end(),[](double v){return v==0;});});
}
void Desire::advance(double seconds,double trigger,double inhibition){
 time_ok(seconds);for(double x:{deficit,refractory,value,inhibition})require_range(x,0,1);
 require_range(baseline,0,.4);require_range(accumulation,0,.1);require_range(sensitivity,0,.8);require_range(trigger,-.5,.5);require_range(decay,0,10);
 if(seconds==0)return;
 // Simultaneous snapshot: do not feed end-of-step D/R back into V at the start.
 const double target=unit((baseline+sensitivity*deficit+trigger)*(1-refractory)-inhibition);
 value=unit(approach(value,target,seconds,target>value?30:300));
 deficit=unit(1-(1-deficit)*std::exp(-accumulation*seconds/hour));
 refractory*=std::exp(-decay*seconds/hour);
}
bool Desire::satisfy(std::uint64_t id,double amount){
 require_range(amount,0,1);if(id==0)throw std::invalid_argument("zero result ID");
 auto it=std::lower_bound(applied_results.begin(),applied_results.end(),id);if(it!=applied_results.end()&&*it==id)return false;
 applied_results.insert(it,id);deficit*=1-amount;refractory+=amount*(1-refractory);return true;
}
double habituation(double old,double engagement,double hours){
 require_range(old,0,1);require_range(engagement,0,1);time_ok(hours);if(hours==0)return old;
 const double rate=.35*engagement+.20*(1-engagement);return unit(approach(old,.35*engagement/rate,hours,1/rate));
}
double pleasantness(double base,double innate,double acquired,double relation,double context,double consequences,double engagement,double h,double rho){
 for(double x:{base,innate,acquired}){require_range(x,-.5,.5);}
 for(double x:{relation,context}){require_range(x,-.25,.25);}
 for(double x:{consequences,engagement,h}){require_range(x,0,1);}
 require_range(rho,0,.8);
 const double r=base+innate+acquired+relation+context;
 return engagement*signed_unit((1-rho*h)*std::max(0.0,r)-std::max(0.0,-r)-consequences);
}
bool Attention::select(std::uint32_t candidate,double old_priority,double next_priority,bool emergency,Tick now,const Capability& c,double commitment,double difference){
 if(c.gate==0||candidate==focus)return false;
 if(focus!=0&&!emergency){if(now<ready)return false;if(double(now-since)/1000<2+4*c.current[1])return false;if(next_priority-old_priority<=.06+.18*c.current[1]+.12*commitment)return false;}
 focus=candidate;since=now;ready=now+Tick(std::ceil(1000*(.25+1.25*(1-c.current[6])+.75*(1-c.current[1])+.75*difference)));return true;
}
} // namespace life
