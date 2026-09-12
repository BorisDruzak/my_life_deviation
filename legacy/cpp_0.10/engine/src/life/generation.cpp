#include "life/world.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <set>
#include <tuple>
#include <stdexcept>

namespace life {
namespace {
std::int32_t birth(int year){using namespace std::chrono;return std::int32_t(sys_days{std::chrono::year{year}/3/15}.time_since_epoch().count());}
Tick daytime(Tick value){
    constexpr Tick d=86400000;Tick offset=((value%d)+d)%d;
    if(offset<8*3600000)return value+8*3600000-offset;
    if(offset>20*3600000)return value+d-offset+8*3600000;
    return value;
}
}
World World::generate(std::uint64_t seed,std::size_t population,const std::string& scenario){
    if(population<8||population>4096)throw std::invalid_argument("population must be 8..4096");
    if(scenario!="normal"&&scenario!="scarcity"&&scenario!="closed-road")throw std::invalid_argument("unsupported scenario / institutional profile");
    State s;s.scenario=scenario;s.random={seed,"0.7.0",catalogue_hash()};
    const Id n=Id(population),related=2*(n/4);
    // Public places: square, club, food counter, well, workshop, library. Private homes follow.
    s.places={{1,service(Method::Inspect),0,0,0,0,0,0},
      {2,service(Method::Leisure)|service(Method::Talk),0,0,0,0,0,0},
      {3,service(Method::AcquireFood)|service(Method::TakeFood)|service(Method::Inspect),std::int64_t(n)*8,0,0,0,0,0},
      {4,service(Method::Drink),0,std::int64_t(n)*12,0,0,0,0},
      {5,service(Method::Work),0,0,0,0,0,0},
      {6,service(Method::Leisure)|service(Method::Shelter),0,0,0,0,0,0}};
    s.edges={{1,2,60,true},{1,3,90,true},{1,4,60,true},{1,5,120,true},{1,6,90,true},{2,3,90,true},{2,6,90,true}};
    s.actors.resize(n);
    s.people.reserve(3*n);
    std::vector<int> years(n);
    for(Id i=0;i<n;++i){
        if(i<related&&i%2==1)years[i]=years[i-1]+2;
        else years[i]=2026-20-int(s.random.integer("birth",i+1,0,49));
        s.people.push_back({i+1,birth(years[i]),{},true});
    }
    for(Id i=0;i<n;++i){
        if(i<related&&i%2==1){s.people[i].parents=s.people[i-1].parents;continue;}
        std::array<Id,2> parents{};
        for(int j=0;j<2;++j){Id id=Id(s.people.size()+1);s.people.push_back({id,birth(years[i]-22-j*3),{},false});parents[j]=id;}
        s.people[i].parents=parents;
    }
    std::set<std::pair<Id,Id>> links;
    auto connect=[&](Id a,Id b){if(a!=b)links.emplace(std::min(a,b),std::max(a,b));};
    for(Id i=0;i<related;i+=2){connect(i+1,i+2);s.roles.push_back({i+1,i+2,1,-Tick(365*day*1000),s.next_id++});}
    // A connected sparse backbone is grounded below in scheduled shared contacts, never in free knowledge.
    std::vector<Id> order(n);std::iota(order.begin(),order.end(),Id{1});
    for(Id i=n-1;i>0;--i){Id j=Id(s.random.integer("social-order",i,0,i+1));std::swap(order[i],order[j]);}
    const Id neighbours=n<16?2:4;
    for(Id i=0;i<n;++i)for(Id d=1;d<=neighbours;++d)connect(order[i],order[(i+d)%n]);
    for(Id i=related;i+1<n;i+=4){connect(i+1,i+2);s.roles.push_back({i+1,i+2,2,-Tick(180*day*1000),s.next_id++});}
    std::vector<int> degree(n,0);for(auto [a,b]:links){++degree[a-1];++degree[b-1];}
    const int maximum=std::min(16,int(n)-2);
    for(Id i=0;i<n;++i){
        const int target=n<16?std::min(6,maximum):8+int(s.random.integer("degree",i+1,0,5));
        for(Id attempt=0;attempt<64&&degree[i]<target;++attempt){
            // Bounded candidate retrieval, not an N-by-N acquaintance matrix.
            Id j=Id(s.random.integer(attempt%4?"local-link":"cross-link",i+1,attempt,n));
            if(i==j||degree[j]>=maximum||degree[i]>=maximum)continue;
            auto pair=std::pair{std::min(i+1,j+1),std::max(i+1,j+1)};
            if(links.insert(pair).second){++degree[i];++degree[j];}
        }
    }
    for(Id i=0;i<n;++i){
        Actor& a=s.actors[i];a.id=i+1;a.home=n+100+i; // IDs are opaque; home IDs intentionally differ from vector positions.
        a.household=a.id;
        if(i<related&&i%2==1)a.household=s.actors[i-1].household;
        if(i>=related&&(i-related)%4==1)a.household=s.actors[i-1].household;
        a.place=a.home;a.style=Id(s.random.integer("style",a.id,0,8));
        if(std::none_of(s.places.begin(),s.places.end(),[&](const auto& p){return p.id==a.home;})){
            s.places.push_back({a.home,service(Method::Eat)|service(Method::Sleep)|service(Method::Rest)|service(Method::PrivateIntimacy)|service(Method::Shelter),0,0,0,0,0,a.id});
            s.edges.push_back({1,a.home,60+std::uint32_t(s.random.integer("roads",a.home,0,121)),true});
        }
        a.biology.metabolism=.85+.30*s.random.uniform("biology",a.id,0);
        a.biology.water_rate=.85+.30*s.random.uniform("biology",a.id,1);
        a.biology.sleep_need=.90+.20*s.random.uniform("biology",a.id,2);
        a.biology.social_need=.8+.4*s.random.uniform("biology",a.id,3);
        a.biology.leisure_need=.8+.4*s.random.uniform("biology",a.id,4);
        a.body.sleep=.10+.15*s.random.uniform("state",a.id,0);
        a.desire.deficit=.15+.35*s.random.uniform("state",a.id,1);
        a.desire.baseline=.05+.1*s.random.uniform("biology",a.id,5);
        a.desire.accumulation*=.7+.6*s.random.uniform("biology",a.id,6);
        for(std::size_t j=0;j<8;++j){a.mind.cognition.base[j]=.35+.4*s.random.uniform("cognition",a.id,j);a.innate_trigger[j]=-.3+.9*s.random.uniform("primary-trigger",a.id,j);}
        a.cog.rejection_bias=.1+.7*s.random.uniform("rejection-interpretation",a.id,0);
        a.mind.cognition.learnability=.35+.4*s.random.uniform("cognition",a.id,8);
        a.mind.cognition.plasticity=.3+.4*s.random.uniform("cognition",a.id,9);
        a.mind.known.fill(true);a.mind.known[std::size_t(Method::Inspect)]=true;
        auto& q=a.mind.priors;
        q[1][0]=.54;q[2][1]=.36;q[3][2]=.8;q[4][3]=.3;
        q[5][4]=.30;q[6][5]=.50;q[6][4]=.15;q[7][6]=.6;
        q[8][0]=.54;q[9][0]=.54;q[10][7]=.5;q[12][8]=.8;
        a.mind.norms[9]=s.random.uniform("norm-property",a.id,0);
        a.mind.risks[9]=sanction(.02+.18*s.random.uniform("risk-property",a.id,0),.8,.5,.6);
        a.mind.norms[7]=.01*s.random.uniform("norm-private",a.id,0);
        a.mind.believed_food=int(a.food);a.mind.believed_money=a.money;
        for(const auto& p:s.places)if(p.id<=6||p.id==a.home)a.mind.places.push_back({p.id,p.services,Truth::Unknown,0,1});
        a.mind.knowledge.mastery={{1,.9},{2,.8},{3,.8},{4,.7}};
        for(Id metric=0;metric<9;++metric)a.mind.knowledge.mastery.emplace_back(100+metric,.9);
    }
    // All homes now exist: copy only the public map and the actor's own home edge.
    for(auto& a:s.actors)for(const auto& e:s.edges)if((e.a<=6&&e.b<=6)||e.a==a.home||e.b==a.home)a.mind.map.push_back({e.a,e.b,e.seconds,Truth::Confirmed});
    for(auto [i,j]:links){
        for(auto [a,b]:{std::pair{i,j},std::pair{j,i}}){Relation r;r.person=b;r.care=.02;r.trust[3].prior=.75;
            for(const auto& role:s.roles)if((role.a==a&&role.b==b)||(role.a==b&&role.b==a)){r.role=role.kind;r.care=.2;r.origin=role.origin;}
            s.actors[a-1].mind.relations.push_back(std::move(r));}
    }
    for(auto& a:s.actors){std::sort(a.mind.relations.begin(),a.mind.relations.end(),[](const auto& x,const auto& y){return x.person<y.person;});a.mind.learning.address_slots=std::uint32_t(a.mind.relations.size());}
    std::vector<Tick> free_at(n,-Tick(30*day*1000)+8*3600000);
    auto event=[&](Id a,Id b,std::uint8_t kind,Id place){Tick t=daytime(std::max(free_at[a-1],b?free_at[b-1]:free_at[a-1])+600000);Tick end=t+300000;free_at[a-1]=end+600000;if(b)free_at[b-1]=end+600000;s.history.push_back({s.next_id++,a,b,place,t,end,kind});};
    for(auto [a,b]:links)event(a,b,std::uint8_t(s.random.integer("history-kind",a,std::uint64_t(b),4)),2);
    for(Id i=1;i<=n;++i){for(std::uint8_t j=0;j<4;++j)event(i,0,std::uint8_t(4+j),j<2?s.actors[i-1].home:2);}
    std::sort(s.history.begin(),s.history.end(),[](const auto& a,const auto& b){return std::tie(a.start,a.id)<std::tie(b.start,b.id);});
    for(const auto& h:s.history){
        for(Id id:{h.a,h.b})if(id){auto& a=s.actors[id-1];const Id other=id==h.a?h.b:h.a;
            Episode e;e.id=h.id;e.time=h.end;e.method=other?Method::Talk:Method::Leisure;e.quality=.6+.4*s.random.uniform("projection",id,h.id);e.features={{100+h.place,h.id,1,1}};
            e.primary_observed=1;e.primary[0]=h.kind==3?-.2:.3;e.observed=1u<<10;e.outcome[10]=e.primary[0];
            if(other){e.features.push_back({10000+other,h.id,1,.8});auto* r=a.mind.relation(other);r->familiarity=1-std::exp(-e.quality/5);r->last_contact=h.end;if(!r->origin)r->origin=h.id;r->trust[h.kind%4].receive({h.id,h.id,h.kind==3?0.0:1.0,e.quality});const double k=.03*a.mind.cognition.plasticity*e.quality*.5;r->care+=(1-std::exp(-k))*(1-r->care);}
            if(h.kind>=6){Id f=Id(s.random.integer("specific-feature",id,h.kind,8));e.features.push_back({200+f,h.id,1,.9});e.primary_observed|=32;e.primary[5]=a.innate_trigger[f];}
            a.mind.learning.observe(e,a.mind.cognition,a.mind.cognition.base[3]);
        }
    }
    s.ledger.initial_food=std::int64_t(n)+s.places[2].food;s.ledger.initial_water=s.places[3].water;s.ledger.initial_money=10.0*n;
    if(scenario=="scarcity"){s.ledger.initial_food-=s.places[2].food;s.places[2].food=n/4;s.ledger.initial_food+=s.places[2].food;}
    if(scenario=="closed-road")s.edges[2].open=false; // A physical change not yet observed by any actor.
    World result(std::move(s));result.seed_social();result.validate();return result;
}
}
