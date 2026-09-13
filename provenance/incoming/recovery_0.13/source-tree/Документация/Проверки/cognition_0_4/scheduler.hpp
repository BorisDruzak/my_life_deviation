#pragma once
#include "core.hpp"
#include <future>
#include <limits>
#include <set>
namespace cog04 {
// A deliberately small executable concurrency contract, not a replacement World.
// The integer payload stands for a pure cognitive operation on a captured personal view.
struct Receipt {
  Ms at{};Id actor{},operation{};bool accepted{};int value{};
  bool operator==(const Receipt&)const=default;
};
struct CapturedJob {Ms start{},due{};Id actor{},operation{},revision{};int input{},operand{};};
struct PersonalObservation {Ms at{};Id actor{},sequence{};int value{};};
class BarrierScheduler {
  Ms now_{};Id next_operation_{1},next_observation_{1};
  std::vector<int> values_;std::vector<Id> revisions_;
  std::vector<CapturedJob> jobs_,batch_;
  std::vector<PersonalObservation> observations_;
  std::vector<Receipt> receipts_;
  std::vector<std::optional<int>> computed_;
  std::set<Id> busy_;
  bool barrier_{};
  static int pure(const CapturedJob&j){
    auto x=std::int64_t(j.input)+j.operand;
    if(x<std::numeric_limits<int>::min()||x>std::numeric_limits<int>::max())throw std::overflow_error("test payload overflow");
    return int(x);
  }
  void enter_next_barrier(){
    Ms t=std::numeric_limits<Ms>::max();
    for(auto j:jobs_)t=std::min(t,j.due);
    for(auto o:observations_)t=std::min(t,o.at);
    if(t==std::numeric_limits<Ms>::max())return;
    now_=t;
    std::sort(observations_.begin(),observations_.end(),[](auto a,auto b){return std::tie(a.at,a.actor,a.sequence)<std::tie(b.at,b.actor,b.sequence);});
    // Phase: already available observation precedes completion of a cognitive operation.
    for(auto o:observations_)if(o.at==t){values_.at(o.actor)=o.value;++revisions_.at(o.actor);}
    std::erase_if(observations_,[&](auto o){return o.at==t;});
    for(auto j:jobs_)if(j.due==t)batch_.push_back(j);
    std::erase_if(jobs_,[&](auto j){return j.due==t;});
    std::sort(batch_.begin(),batch_.end(),[](auto a,auto b){return std::tie(a.actor,a.operation)<std::tie(b.actor,b.operation);});
    computed_.resize(batch_.size());barrier_=!batch_.empty();
  }
public:
  explicit BarrierScheduler(std::size_t actors):values_(actors,0),revisions_(actors,0){if(!actors)throw std::invalid_argument("no actors");}
  Ms now()const{return now_;}
  const std::vector<int>& values()const{return values_;}
  const std::vector<Receipt>& receipts()const{return receipts_;}
  void enqueue(Ms start,Ms duration,Id actor,int operand){
    if(barrier_||start!=now_||duration<=0||actor>=values_.size()||busy_.contains(actor))throw std::invalid_argument("invalid operation start");
    if(duration>std::numeric_limits<Ms>::max()-start)throw std::overflow_error("logical time overflow");
    busy_.insert(actor);jobs_.push_back({start,start+duration,actor,next_operation_++,revisions_[actor],values_[actor],operand});
  }
  // Only already projected personal input. A hidden World event MUST NOT call observe().
  void observe(Ms at,Id actor,int value){
    if(barrier_||at<now_||actor>=values_.size())throw std::invalid_argument("invalid observation");
    observations_.push_back({at,actor,next_observation_++,value});
  }
  bool done()const{return !barrier_&&jobs_.empty()&&observations_.empty();}
  bool pump(std::size_t cpu_quota,std::size_t workers,bool reverse_completion){
    if(!cpu_quota||!workers)throw std::invalid_argument("zero physical processing budget");
    while(!barrier_&&!done())enter_next_barrier();
    if(done())return true;
    std::vector<std::size_t> todo;
    for(std::size_t i=0;i<computed_.size();++i)if(!computed_[i])todo.push_back(i);
    if(reverse_completion)std::reverse(todo.begin(),todo.end());
    if(todo.size()>cpu_quota)todo.resize(cpu_quota);
    for(std::size_t begin=0;begin<todo.size();begin+=workers){
      const auto end=std::min(todo.size(),begin+workers);
      if(workers==1){computed_[todo[begin]]=pure(batch_[todo[begin]]);continue;}
      std::vector<std::future<int>> futures;
      for(std::size_t k=begin;k<end;++k){auto captured=batch_[todo[k]];futures.emplace_back(std::async(std::launch::async,[captured]{return pure(captured);}));}
      for(std::size_t k=begin;k<end;++k)computed_[todo[k]]=futures[k-begin].get();
    }
    if(std::any_of(computed_.begin(),computed_.end(),[](auto&x){return !x.has_value();}))return false;
    // No partial publication. Stable commit order is independent of worker completion.
    for(std::size_t i=0;i<batch_.size();++i){auto j=batch_[i];bool valid=j.revision==revisions_[j.actor];
      if(valid){values_[j.actor]=*computed_[i];++revisions_[j.actor];}
      receipts_.push_back({now_,j.actor,j.operation,valid,valid?*computed_[i]:values_[j.actor]});busy_.erase(j.actor);
    }
    batch_.clear();computed_.clear();barrier_=false;return done();
  }
  void run(std::size_t cpu_quota,std::size_t workers,bool reverse_completion){
    if(!cpu_quota||!workers)throw std::invalid_argument("zero physical processing budget");
    while(!pump(cpu_quota,workers,reverse_completion)){}
  }
};
}
