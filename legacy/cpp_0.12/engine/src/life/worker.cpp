#include "life/cognition.hpp"
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <stdexcept>

namespace life {
struct ForecastWorkers::Impl {
 std::mutex mutex;std::condition_variable wake;bool stop=false;
 std::deque<std::packaged_task<Decision()>> tasks;std::vector<std::thread> threads;
 explicit Impl(unsigned count){if(count<1||count>32)throw std::invalid_argument("workers must be 1..32");
  if(count==1)return;
  for(unsigned i=0;i<count;++i)threads.emplace_back([this]{for(;;){std::packaged_task<Decision()> f;{std::unique_lock lock(mutex);wake.wait(lock,[&]{return stop||!tasks.empty();});if(stop&&tasks.empty())return;f=std::move(tasks.front());tasks.pop_front();}f();}});
 }
 ~Impl(){ {std::lock_guard lock(mutex);stop=true;}wake.notify_all();for(auto& t:threads)t.join();}
};
ForecastWorkers::ForecastWorkers(unsigned count):impl_(std::make_unique<Impl>(count)){}
ForecastWorkers::~ForecastWorkers()=default;
std::vector<Decision> ForecastWorkers::evaluate(const std::vector<std::pair<PersonalView,PlanOption>>& jobs,std::size_t chunk){
 if(!chunk||chunk>100000)throw std::invalid_argument("invalid work chunk");
 std::vector<Decision> out;out.reserve(jobs.size());
 for(std::size_t start=0;start<jobs.size();start+=chunk){const auto end=std::min(jobs.size(),start+chunk);
  if(impl_->threads.empty()){for(auto i=start;i<end;++i)out.push_back(Planner::forecast(jobs[i].first,jobs[i].second));}
  else {std::vector<std::future<Decision>> results;for(auto i=start;i<end;++i){const auto copy=jobs[i];std::packaged_task<Decision()> f([copy]{return Planner::forecast(copy.first,copy.second);});results.push_back(f.get_future());{std::lock_guard lock(impl_->mutex);impl_->tasks.push_back(std::move(f));}impl_->wake.notify_one();}for(auto& result:results)out.push_back(result.get());}
 }
 return out; // Publication is performed later, in stable order, by the main simulation thread.
}
}
