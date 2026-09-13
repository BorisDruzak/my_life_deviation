#pragma once
#include "core.hpp"
#include <map>
namespace cog04 {
inline double priority(double meaning,double urgency,double novelty,double goal,double emotion,double signal,double trait=0){
  for(auto v:{meaning,urgency,novelty,goal,emotion,signal})unit(v);
  finite(trait);if(std::abs(trait)>.2)throw std::invalid_argument("trait correction");
  return clip(.25*meaning+.20*urgency+.15*novelty+.15*goal+.15*emotion+.10*signal+trait);
}
inline int capacity(double memory,double attention,bool conscious){
  unit(memory);unit(attention);return conscious?std::clamp(int(std::floor(2+6*memory+4*attention)),2,12):0;
}
struct Candidate {
  Id id{};double score{};Id revision{};Ms expires{};bool emergency{};double urgency{};
};
struct Focus {Id id{};double score{};Ms since{},rebuild_until{};};
inline bool may_switch(Focus f,Candidate c,Ms now,double attention,double commitment){
  unit(f.score);unit(c.score);unit(attention);unit(commitment);
  if(now<0)throw std::invalid_argument("negative time");
  if(c.expires<=now||!c.id||c.id==f.id)return false;
  if(c.emergency)return true;
  if(!f.id)return true;
  if(now<f.rebuild_until||now<f.since)return false;
  const double margin=.06+.18*attention+.12*commitment;
  // Compare scores in the same arithmetic form. Equality never qualifies.
  return now-f.since>=Ms(std::ceil(1000*(2+4*attention)))&&c.score>f.score+margin;
}
inline Ms switch_ms(double flexibility,double attention,double topic_difference){
  unit(flexibility);unit(attention);unit(topic_difference);
  return Ms(std::ceil(1000*(.25+1.25*(1-flexibility)+.75*(1-attention)+.75*topic_difference)));
}
inline int operation_budget(double speed,bool conscious){unit(speed);return conscious?4+int(std::floor(28*speed)):0;}
inline double operation_rate(double speed,bool conscious){unit(speed);return conscious?2+6*speed:0;}
inline double feature_rate(double channel,double observation,double attention,bool conscious){
  unit(channel);unit(observation);unit(attention);return conscious?4*channel*(.25+.75*observation)*(.25+.75*attention):0;
}
struct Work {
  double done{};bool active{true};
  void advance(double rate,double seconds){nonnegative(rate);nonnegative(seconds);if(active)done=std::min(1.,done+rate*seconds);}
  bool complete()const{return done>=1.;}
};
struct Exposure {
  double accum{},threshold{};
  void advance(double rate,double seconds){nonnegative(rate);nonnegative(seconds);nonnegative(threshold);accum+=rate*seconds;}
  double seconds_to_detection(double rate)const{
    nonnegative(rate);if(accum>=threshold)return 0;
    if(!rate)throw std::invalid_argument("no detection scheduled at zero rate");
    return (threshold-accum)/rate;
  }
};
// Slow reference intake: retains the input batch to compare a bounded production top-K.
// The normative per-NPC candidate queue is the output of select(), NOT this intake map.
class CandidateQueue {
  std::map<Id,Candidate> intake_;
public:
  void upsert(Candidate c){unit(c.score);unit(c.urgency);if(!c.id)throw std::invalid_argument("zero local candidate id");
    auto it=intake_.find(c.id);
    if(it!=intake_.end()&&c.revision==it->second.revision&&
       (c.score!=it->second.score||c.expires!=it->second.expires||c.emergency!=it->second.emergency||c.urgency!=it->second.urgency))
      throw std::invalid_argument("same candidate revision has inconsistent payload");
    if(it==intake_.end()||c.revision>it->second.revision)intake_[c.id]=c;
  }
  double score(Id id)const{return intake_.at(id).score;}
  std::vector<Id> select(Ms now,Id active_focus)const{
    std::vector<Candidate>x;for(const auto& [id,c]:intake_){(void)id;if(c.expires>now)x.push_back(c);}
    std::sort(x.begin(),x.end(),[&](const Candidate&a,const Candidate&b){
      return std::tuple{!a.emergency,a.emergency?-a.urgency:0.,a.id!=active_focus,-a.score,a.id}<
             std::tuple{!b.emergency,b.emergency?-b.urgency:0.,b.id!=active_focus,-b.score,b.id};
    });
    if(x.size()>32)x.resize(32);
    std::vector<Id> out;for(auto c:x)out.push_back(c.id);return out;
  }
};
}
