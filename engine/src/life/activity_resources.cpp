#include "life/activity_resources.hpp"
#include "life/mind.hpp"
#include <algorithm>
namespace life {
ActivityResources activity_resources(Method m,bool travel,bool asleep){
    if(asleep||m==Method::Sleep||m==Method::PrivateIntimacy)return {{0,0,0,0,0,1},true};
    if(travel)return {{.85,.05,.45,.1,0,.25},false};
    switch(m){
    case Method::Work:return {{.1,.65,.65,.1,.05,.70},false};
    case Method::Study:return {{0,.1,.6,.2,0,.85},false};
    case Method::InquireJob:case Method::ApplyJob:return {{0,.1,.2,.6,.85,.6},false};
    case Method::Talk:return {{0,0,.15,.65,.85,.45},false};
    case Method::Eat:return {{0,.4,.15,.05,.1,.15},false};
    case Method::Drink:return {{0,.2,.1,0,.1,.1},false};
    case Method::Leisure:return {{.1,.2,.4,.2,0,.4},false};
    case Method::AcquireFood:case Method::TakeFood:return {{.1,.3,.3,.2,.25,.35},false};
    default:return {{0,0,.05,.05,0,.1},false};
    }
}
ParallelFit combine_resources(const ActivityResources& a,const ActivityResources& b){
    for(double x:a.load)require_range(x,0,1);
    for(double x:b.load)require_range(x,0,1);
    if(a.exclusive||b.exclusive)return {};
    double worst=1;
    for(std::size_t i=0;i<a.load.size();++i){
        const double total=a.load[i]+b.load[i];
        // Hands, locomotion and speech cannot be oversubscribed. Sensory and
        // cognitive load may share time up to a bounded interference limit.
        const bool hard=i==0||i==1||i==4;
        if(total>(hard?1.:1.5))return {};
        if(!hard)worst=std::max(worst,total);
    }
    return {true,1/worst};
}
}
