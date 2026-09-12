#include "life/world.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace life {
void World::validate()const{
    const auto& s=state_;validate_social_world();validate_life();validate_self_runtime();validate_career();
    auto ensure=[](bool condition,const std::string& what){if(!condition)throw std::runtime_error("world invariant: "+what);};
    ensure(s.format=="LIFE-0.12.0-self1"&&(s.balance=="balance-cpp-0.9.0-social1"||s.balance=="community-0.10-test2"||s.balance=="life-projects-0.11-experimental1"||s.balance=="self01-adaptive-1"),"unsupported format or balance");
    ensure(s.random.catalog==catalogue_hash(),"catalogue mismatch");
    ensure(s.scenario=="normal"||s.scenario=="scarcity"||s.scenario=="closed-road","unsupported scenario");
    ensure(s.now>=0,"clock must be nonnegative");
    const std::size_t n=s.actors.size();ensure(n>=8&&n<=4096,"population bounds");
    std::set<Id> places;
    std::int64_t food=0,water=0;double money=0;
    for(const auto& p:s.places){ensure(p.id>0&&places.insert(p.id).second,"duplicate place");ensure(p.food>=0&&p.water>=0&&p.reserved>=0,"negative stock/reservation");require_range(p.account,0,1e12);require_range(p.temperature,-1,1);food+=p.food;water+=p.water;money+=p.account;}
    for(const auto& e:s.edges)ensure(places.contains(e.a)&&places.contains(e.b)&&e.a!=e.b&&e.seconds>0,"edge");
    ensure(s.people.size()>=n,"missing genealogical records");
    using namespace std::chrono;
    const auto epoch=sys_days{year{2026}/9/11};
    auto age=[](std::int32_t from,std::int32_t to){year_month_day a{sys_days{days{from}}},b{sys_days{days{to}}};return int(b.year())-int(a.year())-(std::pair{unsigned(b.month()),unsigned(b.day())}<std::pair{unsigned(a.month()),unsigned(a.day())}?1:0);};
    for(std::size_t i=0;i<s.people.size();++i){const auto& p=s.people[i];ensure(p.id==i+1,"person IDs");if(p.active){int years=age(p.birth_day,std::int32_t(epoch.time_since_epoch().count()));ensure(years>=18&&years<=80,"active person is not an adult in profile");}
        for(Id parent:p.parents)if(parent){ensure(parent<=s.people.size()&&parent!=p.id,"invalid parent");const auto& q=s.people[parent-1];int years=age(q.birth_day,p.birth_day);ensure(years>=18&&years<=50,"parent chronology / DAG");}
        ensure(!p.parents[0]||p.parents[0]!=p.parents[1],"same parent twice");
    }
    std::map<Id,std::int64_t> reserved;
    std::size_t with_relative=0,total_edges=0;
    for(std::size_t i=0;i<n;++i){const auto& a=s.actors[i];ensure(a.id==i+1&&places.contains(a.place)&&places.contains(a.home),"actor identity/location");
        validate_cognition(a.cog,s.now);ensure(a.physical_at<=s.now,"future physics");auto copy=a.body;advance_body(copy,a.biology,Input{},Emotions{},0);auto desire=a.desire;desire.advance(0,0,0);require_range(a.leisure,0,1);require_range(a.social,0,1);
        if(s.community.enabled){
            const auto& cm=a.mind.social.community;const auto& e=a.economy;
            ensure(cm.enabled,"missing community state");
            for(double x:{cm.confidentiality,cm.curiosity,cm.ambition,cm.stranger_romance,cm.kin_romance})require_range(x,0,1);
            attraction(cm.appearance,cm.preference);attraction(a.appearance,a.social_traits.preference);
            ensure(e.job<4,"job level");
            for(double x:{e.work_today,e.study_today,e.earned,e.food_spent,e.leisure_spent,e.study_spent,e.rent_paid,e.rent_debt})require_range(x,0,1e12);
            for(double x:e.time_ms)require_range(x,0,1e15);
            ensure(std::is_sorted(cm.delivered.begin(),cm.delivered.end())&&std::adjacent_find(cm.delivered.begin(),cm.delivered.end())==cm.delivered.end(),"community delivery index");
            std::uint64_t last=0;
            for(const auto& m:cm.entries){ensure(m.content.id>last&&m.first_delivery>0&&m.encoded_at<=s.now,"memory key/time");last=m.content.id;
                ensure(unsigned(m.content.kind)<unsigned(NewsKind::Count)&&unsigned(m.content.disclosure)<=2&&unsigned(m.content.origin)<=2,"information enums");
                for(double x:{m.content.confidence,m.content.importance,m.content.sensitivity,m.content.material})require_range(x,0,1);
                require_range(m.content.valence,-1,1);m.gist.at(s.now);m.place.at(s.now);m.time.at(s.now);m.source.at(s.now);
            }
        }
        require_range(a.money,0,1e12);ensure(a.food>=0,"negative actor food");food+=a.food;money+=a.money;
        ensure(std::size_t(a.action.method)<method_count&&std::uint8_t(a.action.phase)<=std::uint8_t(Phase::Waiting),"action enum");
        if(a.action.phase!=Phase::Idle){ensure(a.action.id>0&&a.action.id<s.next_id,"action ID");ensure(a.action.end>=s.now,"past unfinished action");ensure(places.contains(a.action.destination),"action destination");}
        if(a.action.phase==Phase::Travel){ensure(a.action.path.size()>=2&&a.action.leg+1<a.action.path.size(),"travel cursor");for(Id p:a.action.path)ensure(places.contains(p),"unknown path node");}
        if(a.action.reserved)++reserved[a.action.destination];
        ensure(a.mind.learning.weights.size()+a.mind.learning.address_slots<=512,"acquired-state cap");
        for(const auto& w:a.mind.learning.weights){ensure(w.channel<6,"W channel");require_range(w.value,-1,1);}
        ensure(std::is_sorted(a.mind.learning.processed.begin(),a.mind.learning.processed.end()),"processed-episode index");
        bool relative=false;Id previous=0;
        for(const auto& r:a.mind.relations){ensure(r.person>previous&&r.person<=n&&r.person!=a.id,"social index / duplicate");previous=r.person;
            if(r.role==1){const auto& p=s.people[a.id-1];const auto& q=s.people[r.person-1];ensure(p.parents[0]&&p.parents==q.parents,"claimed sibling without genealogy");relative=true;}
            require_range(r.care,0,1);for(const auto& t:r.trust)require_range(t.expectation(),0,1);
        }
        if(relative)++with_relative;
        total_edges+=a.mind.relations.size();
        if(s.now==0){ensure(a.mind.relations.size()>=3&&a.mind.relations.size()<=std::min(std::size_t(32),n-2),"initial social degree");}
    }
    for(const auto& p:s.places)ensure(p.reserved==reserved[p.id],"reservation conservation");
    ensure(food+s.ledger.eaten_food==s.ledger.initial_food+s.ledger.supplied_food,"food conservation");
    ensure(water+s.ledger.drunk_water==s.ledger.initial_water+s.ledger.supplied_water,"water conservation");
    ensure(std::abs(money-s.ledger.initial_money-s.ledger.wages)<1e-7,"money conservation / phantom fine");
    if(s.now==0){
        ensure(double(with_relative)/n>=.25&&double(with_relative)/n<=.65,"known-relative fraction");
        if(n>=64)ensure(double(total_edges)/(double(n)*(n-1))<=.25,"social density");
        if(n==128)ensure(double(total_edges)/n>=8&&double(total_edges)/n<=16,"average social degree");
        std::set<Id> visited{1};std::vector<Id> queue{1};
        for(std::size_t j=0;j<queue.size();++j)for(const auto& r:s.actors[queue[j]-1].mind.relations)if(visited.insert(r.person).second)queue.push_back(r.person);
        ensure(visited.size()==n,"disconnected social graph");
    }
    std::vector<Tick> last(n,std::numeric_limits<Tick>::min());std::vector<int> counts(n,0);std::set<std::uint64_t> roots;
    for(const auto& e:s.history){ensure(e.id>0&&roots.insert(e.id).second&&e.end>e.start&&e.end<0,"history identity/interval");ensure(places.contains(e.place),"history location");for(Id id:{e.a,e.b})if(id){ensure(id<=n,"history participant");ensure(e.start>=last[id-1],"history overlaps");last[id-1]=e.end;++counts[id-1];}}
    for(int count:counts)ensure(count>=8&&count<=20,"history episode count: "+std::to_string(count));
}
std::string World::summary_json()const{
    const auto& s=state_;std::array<std::uint64_t,method_count> actions{};
    std::uint64_t failures=0,violations=0,alive=0,weights=0,episodes=0;
    double critical=0,energy=0,water=0,sleep=0,leisure=0,social=0,desire=0,max_damage=0,min_energy=1,min_water=1;
    std::size_t min_degree=4096,max_degree=0,degree_sum=0;
    for(const auto& a:s.actors){for(std::size_t i=0;i<method_count;++i)actions[i]+=a.completed[i];failures+=a.failures;violations+=a.violations;critical+=a.critical_seconds;alive+=a.alive?1:0;weights+=a.mind.learning.weights.size();episodes+=a.mind.learning.episodes;
        energy+=a.body.energy;water+=a.body.water;sleep+=a.body.sleep;leisure+=a.leisure;social+=a.social;desire+=a.desire.value;max_damage=std::max(max_damage,a.max_damage);min_energy=std::min(min_energy,a.min_energy);min_water=std::min(min_water,a.min_water);
        min_degree=std::min(min_degree,a.mind.relations.size());max_degree=std::max(max_degree,a.mind.relations.size());degree_sum+=a.mind.relations.size();
    }
    const double n=double(s.actors.size());std::ostringstream out;out.precision(12);
    out<<"{\"version\":\"0.12.0-self01\",\"balance\":\""<<s.balance<<"\",\"catalogue\":\""<<s.random.catalog<<"\",\"seed\":"<<s.random.value<<",\"scenario\":\""<<s.scenario<<"\",\"seconds\":"<<s.now/1000<<",\"population\":"<<s.actors.size()<<",\"alive\":"<<alive<<",\"hash\":\""<<hash()<<"\",\"actions\":{";
    for(std::size_t i=0;i<method_count;++i){if(i)out<<',';out<<'\"'<<method_name(Method(i))<<"\":"<<actions[i];}
    out<<"},\"failures\":"<<failures<<",\"unpermitted_takes\":"<<violations<<",\"critical_actor_seconds\":"<<critical<<",\"min_energy\":"<<min_energy<<",\"min_water\":"<<min_water<<",\"max_damage\":"<<max_damage;
    out<<",\"means\":{\"energy\":"<<energy/n<<",\"water\":"<<water/n<<",\"sleep_pressure\":"<<sleep/n<<",\"leisure\":"<<leisure/n<<",\"social\":"<<social/n<<",\"desire\":"<<desire/n<<"},\"acquired_pairs\":"<<weights<<",\"learning_episodes\":"<<episodes;
    out<<",\"social_degree\":{\"min\":"<<min_degree<<",\"mean\":"<<degree_sum/n<<",\"max\":"<<max_degree<<"},\"body_steps\":"<<metrics_.body_steps<<",\"perception_candidates\":"<<metrics_.perception_candidates<<",\"decision_count\":"<<metrics_.decisions<<",\"institutional_fines\":0}";
    auto text=out.str();text.pop_back();std::uint64_t thoughts=0,started=0,stale=0,switches=0,completed=0;int peak=0;std::array<std::uint64_t,std::size_t(ThoughtKind::Count)> kinds{};
    for(const auto& a:s.actors){for(std::size_t j=0;j<kinds.size();++j){kinds[j]+=a.cog.counts[j];thoughts+=a.cog.counts[j];}started+=a.cog.operation_starts;stale+=a.cog.stale_ops;switches+=a.cog.focus_switches;completed+=a.cog.completed_ops;peak=std::max(peak,a.cog.peak_slots);}
    std::ostringstream more;more<<",\"cognition\":{\"thoughts\":"<<thoughts<<",\"started_ops\":"<<started<<",\"completed_ops\":"<<completed<<",\"stale_ops\":"<<stale<<",\"focus_switches\":"<<switches<<",\"max_slots\":"<<peak<<",\"kinds\": {";
    for(std::size_t j=0;j<kinds.size();++j){if(j)more<<',';more<<'"'<<thought_kind_name(ThoughtKind(j))<<"\":"<<kinds[j];}more<<"}},\"social_events\":{\"completed\":{";
    for(std::size_t k=0;k<interaction_count;++k){if(k)more<<',';more<<'"'<<interaction_name(Interaction(k))<<"\":"<<s.social.completed[k];}
    more<<"},\"declined\":{";
    for(std::size_t k=0;k<interaction_count;++k){if(k)more<<',';more<<'"'<<interaction_name(Interaction(k))<<"\":"<<s.social.declined[k];}
    std::uint64_t known=0,learned=0;for(const auto& a:s.actors){known+=a.mind.social.items.size();learned+=a.mind.social.learned_outcomes;}
    more<<"},\"cancelled\":"<<s.social.cancelled<<",\"loans\":"<<s.social.loans.size()<<",\"uses\":"<<s.social.uses<<",\"item_memories\":"<<known<<",\"learned_outcomes\":"<<learned<<"}}";return text+more.str();
}
}
