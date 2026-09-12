#include "life/mind.hpp"
#include "valuation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace life {
namespace {
constexpr std::array<MethodSpec,method_count> specs{{
 {30,0,0},{90,.02,.12},{30,.02,.05},{28800,0,0},{900,0,.05},
 {1800,.02,.42},{1200,.02,.35},{1800,.02,.25},{60,.02,0},{60,.02,0},
 {1800,.25,.05},{15,0,0},{600,0,.05},{5,0,0},{60,.02,.1},{1800,.02,.1}}};
constexpr std::array<std::size_t,method_count> motive{4,0,1,2,3,4,5,6,0,0,7,0,8,5,4,7};
constexpr const char* names[]={"idle","eat","drink","sleep","rest","leisure","talk","private_intimacy","acquire_food","take_food","work","inspect","shelter","social","use_object","study"};
bool remember(std::vector<std::uint64_t>& ids,std::uint64_t id){
    auto it=std::lower_bound(ids.begin(),ids.end(),id);
    if(it!=ids.end()&&*it==id)return false;
    ids.insert(it,id);return true;
}
std::vector<Feature> selected(std::vector<Feature> f){
    for(const auto& x:f){require_range(x.exposure,0,1);require_range(x.salience,0,1);}
    std::sort(f.begin(),f.end(),[](const auto& a,const auto& b){return std::tie(a.id,a.source)<std::tie(b.id,b.source);});
    std::vector<Feature> unique;
    for(const auto& x:f){
        if(!unique.empty()&&unique.back().id==x.id){unique.back().exposure=std::max(unique.back().exposure,x.exposure);unique.back().salience=std::max(unique.back().salience,x.salience);}
        else unique.push_back(x);
    }
    std::stable_sort(unique.begin(),unique.end(),[](const auto& a,const auto& b){if(a.salience!=b.salience)return a.salience>b.salience;return a.id<b.id;});
    if(unique.size()>16)unique.resize(16);
    return unique;
}
double decayed(const Weight& w,Tick now){
    if(now<w.updated)throw std::invalid_argument("weight read before its timestamp");
    return w.value*std::exp(-std::log(2.0)*double(now-w.updated)/(1000*day*365));
}
using Key=std::pair<Id,std::uint8_t>;
auto locate(std::vector<Weight>& weights,Key key){return std::lower_bound(weights.begin(),weights.end(),key,[](const Weight& w,Key k){return Key{w.feature,w.channel}<k;});}
auto locate(const std::vector<Weight>& weights,Key key){return std::lower_bound(weights.begin(),weights.end(),key,[](const Weight& w,Key k){return Key{w.feature,w.channel}<k;});}
std::vector<Id> route(Id from,Id goal,const std::vector<KnownEdge>& edges,double& duration){
    duration=0;if(from==goal)return {from};
    std::map<Id,double> distance;std::map<Id,Id> parent;std::set<Id> closed;
    distance[from]=0;
    while(true){
        Id current=0;double best=std::numeric_limits<double>::infinity();
        for(auto [id,d]:distance)if(!closed.contains(id)&&d<best){current=id;best=d;}
        if(current==0)return {};
        if(current==goal){std::vector<Id> path{goal};while(path.back()!=from)path.push_back(parent.at(path.back()));std::reverse(path.begin(),path.end());duration=best;return path;}
        closed.insert(current);
        for(const auto& e:edges)if(e.open!=Truth::Refuted&&e.open!=Truth::Conflicting&&(e.from==current||e.to==current)){
            Id next=e.from==current?e.to:e.from;double d=best+e.seconds;
            auto it=distance.find(next);if(it==distance.end()||d<it->second){distance[next]=d;parent[next]=current;}
        }
    }
}
}
const char* method_name(Method m){auto i=std::size_t(m);if(i>=method_count)throw std::invalid_argument("unknown method");return names[i];}
const MethodSpec& method_spec(Method m){auto i=std::size_t(m);if(i>=method_count)throw std::invalid_argument("unknown method");return specs[i];}
std::string catalogue_hash(){
    std::string bytes="LIFE-0.9.0;BEHAVIOR-0.3;COG-0.4;balance-cpp-0.9.0-social1;graph-seconds;expected-sanctions;";
    for(std::size_t i=0;i<method_count;++i){bytes+=names[i];bytes+=':';bytes+=std::to_string(specs[i].seconds);bytes+=':';bytes+=std::to_string(std::bit_cast<std::uint64_t>(specs[i].load));bytes+=':';bytes+=std::to_string(std::bit_cast<std::uint64_t>(specs[i].pleasure));bytes+=';';}
    return hex_sha256(bytes);
}
void Estimate::observe(double y,double learning,double quality,double dose,double memory,bool conscious){
    require_range(y,-1,1);for(double x:{learning,quality,dose,memory})require_range(x,0,1);
    const double alpha=.25*learning*quality*dose*(.6+.4*memory);
    if(!conscious||alpha==0)return;
    const double error=y-mean;variance=(1-alpha)*(variance+alpha*error*error);mean+=alpha*error;count+=quality*dose;
}
double Estimate::confidence(double similarity,double multiplier)const{
    require_range(similarity,0,1);require_range(multiplier,.5,1.5);return unit(count/(count+4)*(1-unit(variance))*similarity*multiplier);
}
bool Learning::observe(const Episode& e,const Cognitive& c,double memory){
    if(e.id==0||std::size_t(e.method)>=method_count||e.primary_observed>63)throw std::invalid_argument("invalid learning episode");
    require_range(e.quality,0,1);require_range(e.dose,0,1);require_range(memory,0,1);
    for(std::size_t j=0;j<metric_count;++j)if(e.observed&(1u<<j))require_range(e.outcome[j],-1,1);
    for(std::size_t j=0;j<6;++j)if(e.primary_observed&(1u<<j))require_range(e.primary[j],-1,1);
    auto f=selected(e.features);
    if(!remember(processed,e.id))return false;
    last_considered=last_created=0;
    if(!e.conscious||e.quality==0||e.dose==0)return false;
    ++episodes;
    for(std::size_t j=0;j<metric_count;++j)if(e.observed&(1u<<j))q[std::size_t(e.method)][j].observe(e.outcome[j],c.learnability,e.quality,e.dose,memory,true);
    std::vector<std::uint8_t> channels;
    for(std::uint8_t j=0;j<6;++j)if((e.primary_observed&(1u<<j))&&std::abs(e.primary[j])>=.10)channels.push_back(j);
    std::stable_sort(channels.begin(),channels.end(),[&](auto a,auto b){if(std::abs(e.primary[a])!=std::abs(e.primary[b]))return std::abs(e.primary[a])>std::abs(e.primary[b]);return a<b;});
    int features_created=0;
    if(e.quality>=.20&&c.plasticity>0){
        for(const auto& x:f){
            if(x.exposure<.25||features_created==2)continue;
            int created_here=0;
            for(auto j:channels){
                if(created_here==2||last_created==4)break;
                auto it=locate(weights,{x.id,j});if(it!=weights.end()&&it->feature==x.id&&it->channel==j)continue;
                if(weights.size()+address_slots>=512){
                    auto victim=weights.end();
                    for(auto w=weights.begin();w!=weights.end();++w)if(e.time-w->activated>=Tick(30*day*1000)&&std::abs(decayed(*w,e.time))<.02&&(victim==weights.end()||std::tie(w->activated,w->feature,w->channel)<std::tie(victim->activated,victim->feature,victim->channel)))victim=w;
                    if(victim==weights.end())continue;
                    weights.erase(victim);it=locate(weights,{x.id,j});
                }
                weights.insert(it,{x.id,j,0,e.time,e.time});++created_here;++last_created;
            }
            if(created_here>0)++features_created;
        }
    }
    for(std::uint8_t j=0;j<6;++j)if(e.primary_observed&(1u<<j)){
        std::vector<std::pair<std::size_t,double>> active;double predicted=0,z=0;
        for(const auto& x:f){auto it=locate(weights,{x.id,j});if(it!=weights.end()&&it->feature==x.id&&it->channel==j){it->value=decayed(*it,e.time);it->updated=e.time;it->activated=e.time;predicted+=x.exposure*it->value;z+=x.exposure*x.exposure;active.emplace_back(std::size_t(it-weights.begin()),x.exposure);}}
        last_considered+=std::uint32_t(active.size());
        const double delta=e.primary[j]-signed_unit(predicted),eta=.02*c.plasticity*e.quality*e.dose;
        for(auto [i,x]:active)weights[i].value=signed_unit(weights[i].value+eta*x/std::max(1.0,z)*delta);
    }
    return true;
}
Channels Learning::effects(const std::vector<Feature>& features,Tick now)const{
    auto f=selected(features);Channels result{};
    for(const auto& x:f)for(std::uint8_t j=0;j<6;++j){auto it=locate(weights,{x.id,j});if(it!=weights.end()&&it->feature==x.id&&it->channel==j)result[j]+=x.exposure*decayed(*it,now);}
    for(auto& x:result)x=signed_unit(x);
    return result;
}
PreparedEffects Learning::prepare(const std::vector<Feature>& features)const{
    PreparedEffects p;auto f=selected(features);
    for(const auto& x:f)for(std::uint8_t j=0;j<6;++j){auto it=locate(weights,{x.id,j});if(it!=weights.end()&&it->feature==x.id&&it->channel==j)p.channels[j].push_back({std::size_t(it-weights.begin()),x.id,x.exposure});}
    return p;
}
Channels Learning::prepared_effects(const PreparedEffects& plan,Tick now)const{
    Channels result{};
    for(std::size_t j=0;j<6;++j){for(const auto& link:plan.channels[j]){
        const auto& w=weights.at(link.index);if(w.feature!=link.feature||w.channel!=j)throw std::logic_error("stale prepared effects");
        result[j]+=link.exposure*decayed(w,now);
    }result[j]=signed_unit(result[j]);}
    return result;
}
bool Budget::pay(int n){if(n<0)throw std::invalid_argument("negative operation count");if(used+n>operations)return false;used+=n;return true;}
bool Budget::hold(int n){if(n<0)throw std::invalid_argument("negative slot count");if(held+n>slots)return false;held+=n;peak=std::max(peak,held);return true;}
void Budget::release(int n){if(n<0||n>held)throw std::invalid_argument("invalid slot release");held-=n;}
bool CausalHypothesis::compare(const CausalSample& yes,const CausalSample& no,double learning,double causal,int depth,Budget& budget){
    if(!budget.hold(3))return false;
    const bool paid=budget.pay();budget.release(3);if(!paid)return false;
    if(!yes.result_observed||!no.result_observed||!yes.factor_observed||!no.factor_observed||!yes.present||no.present||!yes.ordered||!no.ordered||yes.id==no.id)return false;
    if(std::binary_search(used.begin(),used.end(),yes.id)||std::binary_search(used.begin(),used.end(),no.id))return false;
    std::set<Id> a(yes.other_conditions.begin(),yes.other_conditions.end()),b(no.other_conditions.begin(),no.other_conditions.end());
    std::vector<Id> common,united;std::set_intersection(a.begin(),a.end(),b.begin(),b.end(),std::back_inserter(common));std::set_union(a.begin(),a.end(),b.begin(),b.end(),std::back_inserter(united));
    if(united.empty())return false;
    double similarity=double(common.size())/double(united.size());if(similarity<.6)return false;
    require_range(learning,0,1);require_range(causal,0,1);require_range(double(depth),0,4);require_range(yes.quality,0,1);require_range(no.quality,0,1);
    const double quality=std::min(yes.quality,no.quality)*similarity,alpha=.20*learning*quality*causal*(.2+.8*depth/4.0);
    if(alpha==0)return false;
    const double error=signed_unit(yes.result-no.result)-effect;variance=(1-alpha)*(variance+alpha*error*error);effect+=alpha*error;weight+=quality;
    remember(used,yes.id);remember(used,no.id);return true;
}
double CausalHypothesis::justification()const{return weight/(weight+4)*(1-unit(variance));}
double Knowledge::get(Id id)const{for(auto [key,g]:mastery)if(key==id)return g;return 0;}
bool Knowledge::usable(const Rule& r,const std::vector<Rule>& cat)const{
    std::set<Id> visiting;
    std::function<bool(const Rule&)> check=[&](const Rule& rule){if(get(rule.id)<.60||visiting.contains(rule.id))return false;visiting.insert(rule.id);for(Id id:rule.prerequisites){auto it=std::find_if(cat.begin(),cat.end(),[&](const auto& x){return x.id==id;});if(it==cat.end()||!check(*it))return false;}visiting.erase(rule.id);return true;};
    return check(r);
}
void Knowledge::learn(const Rule& r,const std::vector<Rule>& cat,double learning,double causal,double quality,double dose){
    for(double x:{learning,causal,quality,dose})require_range(x,0,1);
    double old=get(r.id),alpha=.25*learning*quality*dose*(.4+.6*causal);if(alpha==0)return;
    bool prerequisites=true;for(Id id:r.prerequisites){auto it=std::find_if(cat.begin(),cat.end(),[&](const auto& x){return x.id==id;});if(it==cat.end()||!usable(*it,cat))prerequisites=false;}
    double next=old+alpha*(1-old);if(!prerequisites)next=std::max(old,std::min(.59,next));
    auto it=std::find_if(mastery.begin(),mastery.end(),[&](auto x){return x.first==r.id;});if(it==mastery.end())mastery.emplace_back(r.id,next);else it->second=next;
    std::sort(mastery.begin(),mastery.end());
}
int Knowledge::depth(Id topic,const std::vector<Rule>& cat)const{
    int result=0;for(int level=1;level<=4;++level){bool exists=false,valid=true;for(const auto& r:cat)if(r.topic==topic&&r.level<=level){if(r.level==level)exists=true;if(!usable(r,cat))valid=false;}if(!exists||!valid)break;result=level;}return result;
}
bool Trust::receive(Evidence e){
    if(e.delivery==0)throw std::invalid_argument("zero evidence delivery");
    require_range(e.result,0,1);require_range(e.quality,0,1);
    if(std::any_of(evidence.begin(),evidence.end(),[&](const auto& old){return old.delivery==e.delivery;}))return false;
    evidence.push_back(e);return true;
}
void Trust::revise(std::uint64_t root,double result,double quality){
    if(root==0)throw std::invalid_argument("cannot revise an unknown origin");
    require_range(result,0,1);require_range(quality,0,1);
    for(auto& e:evidence)if(e.known_root==root){e.result=result;e.quality=quality;}
}
double Trust::expectation()const{
    std::map<std::uint64_t,Evidence> groups;
    for(const auto& e:evidence){auto it=groups.find(e.known_root);if(it==groups.end()||e.quality>it->second.quality)groups[e.known_root]=e;}
    double support=0,refutation=0;for(const auto& [id,e]:groups){(void)id;support+=e.quality*e.result;refutation+=e.quality*(1-e.result);}
    return (strength*prior+support)/(strength+support+refutation);
}
Dependency dependency(const std::vector<GoalDependency>& goals){
    double all=0,covered=0,numerator=0;
    for(const auto& g:goals){require_range(g.importance,0,1);all+=g.importance;if(g.contribution&&g.replacement){require_range(*g.contribution,0,1);require_range(*g.replacement,0,1);covered+=g.importance;numerator+=g.importance**g.contribution*(1-*g.replacement);}}
    if(all==0)return {0.0,0};
    if(covered==0)return {std::nullopt,0};
    return {numerator/covered,covered/all};
}
double sanction(double d,double q,double r,double severity){for(double x:{d,q,r,severity})require_range(x,0,1);return d*q*r*severity;}
double revise_norm(double old,double target,double p,double q,double dose){for(double x:{old,target,p,q,dose})require_range(x,0,1);return unit(old+(1-std::exp(-.02*p*q*dose))*(target-old));}
double moral_cost(const std::vector<Aspect>& aspects){std::map<Id,double> groups;for(auto x:aspects){require_range(x.weight,0,1);require_range(x.violation,0,1);groups[x.group]=std::max(groups[x.group],x.weight*x.violation);}double result=0;for(auto [id,value]:groups){(void)id;result+=value;}return unit(result);}
Decision Planner::choose(const PersonalView& v,const Seed& random){
    Decision best;best.place=v.place;
    if(v.capability.gate==0)return best;
    Budget budget(v.capability);if(!budget.hold(1))return best;
    struct Idea{Method method;KnownPlace place;double urgency;};std::vector<Idea> ideas;
    for(std::size_t i=1;i<method_count;++i){
        if(!v.known[i]||i==std::size_t(Method::Social)||i==std::size_t(Method::UseObject))continue;
        auto m=Method(i);const double need=v.need[motive[i]];if(need<=.001)continue;
        if(m==Method::Eat&&v.food<=0)continue;
        if((m==Method::AcquireFood||m==Method::TakeFood)&&v.food>0)continue;
        for(const auto& place:v.places){
            if(!(place.services&service(m))||v.now<place.retry_method[i])continue;
            if(m==Method::AcquireFood&&v.money<place.price)continue;
            if(v.economy.enabled){
                if(m==Method::Work&&(place.job_level<0||v.economy.mastery[unsigned(place.job_level)]<.6))continue;
                if(m==Method::Leisure&&place.price>std::max(0.,v.money-v.economy.protected_cash))continue;
            }
            if((m==Method::AcquireFood||m==Method::TakeFood)&&place.food==Truth::Refuted)continue;
            if(m==Method::Inspect&&(place.food!=Truth::Refuted||v.food>0||v.now<place.retry_at))continue;
            const double urgency=(m==Method::Inspect?.25*need:need)+random.uniform("idea",v.self,v.episode*32+i)*.002
              +(v.economy.enabled&&m==Method::Work?.25*place.wage/42.5:0);
            ideas.push_back({m,place,urgency});
        }
    }
    std::stable_sort(ideas.begin(),ideas.end(),[](const auto& a,const auto& b){if(a.urgency!=b.urgency)return a.urgency>b.urgency;return std::tie(a.method,a.place.id)<std::tie(b.method,b.place.id);});
    if(ideas.size()>8)ideas.resize(8);
    Outcomes importance{};double norm=0;
    for(std::size_t j=0;j<metric_count;++j){importance[j]=v.need[j]>0?.05+.50*v.need[j]:0;norm+=importance[j];}
    for(double& x:importance)x/=std::max(1.0,norm);
    int considered=0;
    for(const auto& idea:ideas){
        if(considered>=v.capability.alternatives||!budget.hold(1))break;
        double travel=0;auto path=route(v.place,idea.place.id,v.map,travel);
        if(path.empty()){budget.release();if(!budget.pay())break;continue;}
        if(!budget.pay(2+int(path.size()))){budget.release();break;}
        ++considered;const auto i=std::size_t(idea.method);double score=0;
        for(std::size_t j=0;j<metric_count;++j)score+=importance[j]*(v.forecasts[i][j]>=0?std::min(v.need[j],v.forecasts[i][j]):v.forecasts[i][j]);
        if(idea.method==Method::Inspect)score=importance[0]*.08; // Value of a known information-gathering procedure, not experienced nutrition.
        const double price=idea.method==Method::AcquireFood?idea.place.price/25:0;
        const double time=(travel+method_spec(idea.method).seconds)/86400*.10;
        score=signed_unit(score-price-time-v.norms[i]-v.risks[i]);
        if(score>best.score){best.method=idea.method;best.place=idea.place.id;best.path=std::move(path);best.score=score;best.moral=v.norms[i];best.risk=v.risks[i];best.resource_cost=price;best.time_cost=time;best.partner=(idea.method==Method::Talk&&!v.perceived_people.empty())?v.perceived_people.front():0;}
        // The candidate itself stays in context; transient route/forecast arguments are not extra free thoughts.
    }
    best.operations=budget.used;best.peak_slots=budget.peak;best.alternatives=considered;return best;
}

std::vector<PlanOption> Planner::ideas(const PersonalView& v,const Seed& random){
    std::vector<PlanOption> ideas;
    if(v.capability.gate==0)return ideas;
    for(std::size_t i=1;i<method_count;++i){
        if(!v.known[i]||i==std::size_t(Method::Social)||i==std::size_t(Method::UseObject))continue;
        Method m=Method(i);double need=v.need[motive[i]];
        if(v.economy.enabled&&m==Method::Talk&&v.social.in_conversation)continue;
        if(m==Method::Study){
            if(!v.economy.enabled||v.economy.mastery[3]>=.6||v.economy.study_budget<4||v.economy.study_today>=2)continue;
            need=.25+.45*v.social.memory.community.ambition;
        }
        if(m==Method::Work&&v.economy.enabled){
            const auto hour=(v.now%86400000)/3600000;
            if((v.now/86400000)%7>=5||hour<9||hour>=17||v.economy.work_today>=8)continue;
            need=std::max(need,.12);
        }
        // A familiar prerequisite: income can supply food in the known exchange procedure.
        if(m==Method::Work && !v.food && v.money<(v.economy.enabled?12:1))need=std::max(need,v.need[0]);
        if(need<=.001)continue;
        if(m==Method::Eat&&v.food<=0)continue;
        if((m==Method::AcquireFood||m==Method::TakeFood)&&v.food>0)continue;
        for(const auto& place:v.places){
            if(!(place.services&service(m))||v.now<place.retry_method[i])continue;
            if(m==Method::AcquireFood&&v.money<place.price)continue;
            if(v.economy.enabled){
                if(m==Method::Work&&(place.job_level<0||v.economy.mastery[unsigned(place.job_level)]<.6))continue;
                if(m==Method::Leisure&&place.price>std::max(0.,v.money-v.economy.protected_cash))continue;
            }
            if((m==Method::AcquireFood||m==Method::TakeFood)&&place.food==Truth::Refuted)continue;
            if(m==Method::Inspect&&(place.food!=Truth::Refuted||v.food>0||v.now<place.retry_at))continue;
            const double urgency=(m==Method::Inspect?.25*need:need)+random.uniform("idea",v.self,v.episode*32+i)*.002
              +(v.economy.enabled&&m==Method::Work?.25*place.wage/42.5:0);
            if(m==Method::Talk&&place.id==v.place&&!v.perceived_people.empty()){
                for(Id person:v.perceived_people)ideas.push_back({m,place.id,person,urgency});
            }else ideas.push_back({m,place.id,0,urgency});
        }
    }
    if(v.social.enabled){
        for(const auto& x:social_choices(v.social)){
            Method m=x.kind==Interaction::UseItem?Method::UseObject:Method::Social;
            if(!v.known[std::size_t(m)])continue;
            PlanOption p{m,v.place,x.other,x.priority};p.interaction=x.kind;p.object=x.object;ideas.push_back(p);
        }
        // An instrumental conversation can be initiated even without social hunger.
        if(!v.social.in_conversation&&v.social.memory.goal_object&&v.known[std::size_t(Method::Talk)]){
            const auto* item=v.social.memory.item(v.social.memory.goal_object);
            for(const auto& person:v.social.perceived){
                if(item&&item->owner_status==Truth::Confirmed&&person.id!=item->owner)continue;
                if(item&&item->holder_status==Truth::Confirmed&&item->holder==v.self&&!v.social.memory.goal_used)continue;
                if(v.social.memory.goal_used&&(!item||item->owner==v.self))continue;
                ideas.push_back({Method::Talk,v.place,person.id,(item&&item->owner_status==Truth::Confirmed)?.98:.8+.18*person.competence});
            }
        }
    }
    if(v.economy.enabled&&v.focus_metric>=0){
        // Attention has already selected the question. Only known links receive
        // retrieval priority; no true inventory/other actor state is consulted.
        for(auto& idea:ideas){const auto m=idea.method;bool relevant=int(motive[std::size_t(m)])==v.focus_metric;
            if(v.focus_metric==0&&m==Method::Work&&!v.food&&v.money<12)relevant=true;
            if(v.focus_metric==5&&m==Method::Social)relevant=true;
            if(relevant)idea.salience+=.5;
        }
    }
    std::stable_sort(ideas.begin(),ideas.end(),[](const auto&a,const auto&b){return a.salience!=b.salience?a.salience>b.salience:std::tie(a.method,a.place,a.partner,a.interaction,a.object)<std::tie(b.method,b.place,b.partner,b.interaction,b.object);});
    if(v.economy.enabled){
        std::vector<PlanOption> picked;std::set<std::pair<Method,Interaction>> represented;
        for(const auto& x:ideas)if(represented.insert({x.method,x.method==Method::Social?x.interaction:Interaction::Count}).second){picked.push_back(x);if(picked.size()==8)break;}
        for(const auto& x:ideas){if(picked.size()==8)break;if(std::none_of(picked.begin(),picked.end(),[&](const auto& p){return p.method==x.method&&p.place==x.place&&p.partner==x.partner&&p.object==x.object&&p.interaction==x.interaction;}))picked.push_back(x);}
        ideas=std::move(picked);
    }
    if(ideas.size()>8)ideas.resize(8);
    return ideas;
}
Decision Planner::forecast(const PersonalView& v,const PlanOption& idea){
    Decision d;d.method=idea.method;d.place=idea.place;d.partner=idea.partner;d.score=-1;
    d.interaction=idea.interaction;d.object=idea.object;
    if(idea.method==Method::Social||idea.method==Method::UseObject){
        if(!v.known[std::size_t(idea.method)]||v.capability.gate==0)return d;
        const auto e=evaluate_social(v.social,idea.interaction,idea.partner,idea.object);
        d.social_evaluation=e;d.score=e.score;d.moral=e.moral;d.risk=e.devaluation*.35;
        if(e.known){d.path={v.place};}
        return d;
    }
    if(std::size_t(idea.method)>=method_count||!v.known[std::size_t(idea.method)]||v.capability.gate==0)return d;
    auto place=std::find_if(v.places.begin(),v.places.end(),[&](auto& p){return p.id==idea.place;});
    if(place==v.places.end())return d;
    double travel=0;d.path=route(v.place,idea.place,v.map,travel);if(d.path.empty())return d;
    const auto i=std::size_t(idea.method);Outcomes importance{};double norm=0;
    for(std::size_t j=0;j<metric_count;++j){importance[j]=v.need[j]>0?.05+.5*v.need[j]:0;norm+=importance[j];}
    for(double& x:importance)x/=std::max(1.,norm);
    double expected=0;for(std::size_t j=0;j<metric_count;++j){const double q=v.forecasts[i][j];expected+=importance[j]*(q>=0?std::min(v.need[j],q):q);}
    if(idea.method==Method::Work&&!v.food&&v.money<(v.economy.enabled?12:1)){
        expected=std::max(expected,importance[0]*.35); // Known compressed work -> exchange -> eat, not observed calories.
    }
    if(v.economy.enabled){
        if(idea.method==Method::Work){
            if(place->job_level<0||v.economy.mastery[unsigned(place->job_level)]<.6){d.path.clear();return d;}
            expected=std::max(expected,(.15+.7*v.need[7])*(place->wage/42.5));
        }
        if(idea.method==Method::Study){
            const double ambition=v.social.memory.community.ambition;
            expected=.08+.22*ambition; // known investment in a 4-level curriculum, not instant promotion
        }
        if(idea.method==Method::Leisure&&place->recreation>=0)expected*=v.economy.interests[unsigned(place->recreation)];
    }
    if(idea.method==Method::Inspect)expected=importance[0]*.08;
    if(idea.method==Method::Talk&&v.social.enabled&&v.social.memory.goal_object){
        const auto* item=v.social.memory.item(v.social.memory.goal_object);
        if((!item||item->owner_status!=Truth::Confirmed||item->owner==idea.partner)&&
           (!v.social.memory.goal_used||(item&&item->owner!=v.self)))expected=std::max(expected,.48);
    }

    const double hours=(travel+method_spec(idea.method).seconds)/3600.;
    d.moral=cog04::norm_response(v.norms[i],1,1,0,1,0,1).resistance;
    d.risk=v.risks[i];d.resource_cost=idea.method==Method::AcquireFood?place->price/(v.economy.enabled?100.:25.):0;
    if(v.economy.enabled&&idea.method==Method::Leisure)d.resource_cost=place->price/100.;
    if(v.economy.enabled&&idea.method==Method::Study)d.resource_cost=4./100.;
    d.time_cost=hours/24*.10;
    cog04::Ledger ledger;
    ledger.impulse(1,0,hours,expected);
    ledger.impulse(2,0,0,-d.resource_cost);
    ledger.impulse(3,0,0,-d.moral);
    ledger.impulse(4,0,0,-d.risk);
    if(hours>0)ledger.flow(5,0,0,hours,-.10/24);
    d.score=cog04::squash(ledger.value(std::max(24.,hours),cog04::cfg::discount_per_hour));
    return d;
}
Relation* Mind::relation(Id id){auto it=std::lower_bound(relations.begin(),relations.end(),id,[](const Relation& r,Id key){return r.person<key;});return it!=relations.end()&&it->person==id?&*it:nullptr;}
const Relation* Mind::relation(Id id)const{auto it=std::lower_bound(relations.begin(),relations.end(),id,[](const Relation& r,Id key){return r.person<key;});return it!=relations.end()&&it->person==id?&*it:nullptr;}
PersonalView Mind::view(Id self,Id place,Id home,Tick now,const Capability& c,const Outcomes& need,const std::vector<Id>& visible)const{
    PersonalView v;v.self=self;v.place=place;v.home=home;v.now=now;v.episode=decisions;v.capability=c;v.need=need;v.food=believed_food;v.money=believed_money;v.known=known;v.norms=norms;v.risks=risks;v.places=places;v.map=map;v.perceived_people=visible;
    if(v.perceived_people.size()>4)v.perceived_people.resize(4);
    v.forecasts=priors;
    for(std::size_t i=0;i<method_count;++i)for(std::size_t j=0;j<metric_count;++j){const auto& q=learning.q[i][j];if(q.count>0){double cQ=q.confidence();v.forecasts[i][j]=(1-cQ)*priors[i][j]+cQ*q.mean;}}
    return v;
}
}
