#include "mld/world.hpp"
#include <chrono>
#include <numeric>
namespace mld {
    const std::string& catalog_hash(){static const auto hash=sha256(catalog_text);
        return hash;
    }
    const char* action_name(Act a){static const char* names[]={"wait","move","inspect","exchange","take","eat","drink","sleep","rest","play","talk","private_relief","work","give"};
        return std::size_t(a)<action_count?names[std::size_t(a)]:"invalid";
    }

    State generate(Config c){
        if(c.profile!=model_version||c.population<8||c.population>1024||c.population%2||(c.scarce&&c.temptation)||!std::isfinite(c.theft_norm_override)||c.theft_norm_override < -1||c.theft_norm_override>1)throw std::invalid_argument("unsupported profile/population/override");
        State s;
        s.config=c;
        auto draw=[&](const char* stage,std::uint64_t entity,std::uint64_t k,std::uint64_t n){return uniform(c.seed,catalog_hash(),stage,entity,k,n);
        };
        auto real=[&](const char* stage,Id i,unsigned k){return double(draw(stage,i,k,10001))/10000;
        };
        const Id count=c.population;
        const auto today=std::chrono::sys_days(std::chrono::year(2026)/9/11);
        s.npcs.resize(count);
        s.persons.resize(count);
        s.places.resize(5+count/2);
        for(Id p=0;p<s.places.size();++p){auto& l=s.places[p];
            l.id=p;
            l.kind=std::uint8_t(p<5?p:5);
            l.food=p==1?(c.scarce?count/8:count*12):(p>=5&&!c.scarce&&!c.temptation?2:0);
            l.water=p==2?count*16:0;
            s.counters.initial_food+=l.food;
            s.counters.initial_water+=l.water;
        }
        for(Id i=0;i<count;++i){
            auto& n=s.npcs[i];
            n.id=i;
            n.home=5+i/2;
            n.place=n.home;
            const int years=23+int(draw("genealogy",i/2,0,40))+int(draw("genealogy",i,1,4));
            auto birth=std::chrono::sys_days(std::chrono::year(2026-years)/9/11)-std::chrono::days(draw("genealogy",i/2,2,100));
            s.persons[i]={i,none,std::int32_t(birth.time_since_epoch().count()),true};
            auto& b=n.biology;
            b.energy_use=.85+.3*real("biology",i,0);
            b.water_use=.85+.3*real("biology",i,1);
            b.sleep_need=.85+.3*real("biology",i,2);
            b.social_need=.7+.6*real("biology",i,3);
            b.leisure_need=.7+.6*real("biology",i,4);
            b.learning=.35+.4*real("biology",i,5);
            b.plasticity=.3+.4*real("biology",i,6);
            for(unsigned j=0;j<8;++j)b.intellect[j]=.35+.4*real("biology",i,10+j);
            n.body.energy=c.temptation?.2:.7+.2*real("state",i,0);
            n.body.water=.7+.2*real("state",i,1);
            n.body.sleep=.1+.2*real("state",i,2);
            n.desire.deficit=.1+.6*real("state",i,3);
            n.desire.background=.04+.18*real("biology",i,20);
            n.desire.sensitivity=.25+.5*real("biology",i,21);
            n.desire.rate*=.5+real("biology",i,22);
            n.innate_trigger=-.1+.5*real("biology",i,23);
            n.next_review=1000*draw("review",i,0,30);
            n.money=c.temptation?0:8;
            n.food=c.scarce||c.temptation?0:1;
            n.memory.property_norm=c.theft_norm_override<0?.05+.95*real("norm",i,0):c.theft_norm_override;
            n.memory.privacy_norm=.2+.7*real("norm",i,1);
            n.memory.ways.fill(true);
            n.memory.ways[std::size_t(Act::Give)]=false;
            // reserved, not implemented in lab1
            for(Id p:{Id(0),Id(1),Id(2),Id(3),Id(4),n.home})n.memory.places.push_back({p,p==n.home&&c.temptation?Truth::False:(p==1||p==n.home?Truth::True:Truth::Unknown),p==n.home,0,0,1});
            if(c.temptation)for(auto& known:n.memory.places)if(known.place==n.home)known.retry=std::numeric_limits<Time>::max();
            auto prior=[&](Act a,int m,double mean){auto& q=n.memory.expectation[std::size_t(a)][m];
                q.mean=mean;
                q.count=0;
            };
            prior(Act::Eat,0,.65);
            prior(Act::Exchange,0,.65);
            prior(Act::Take,0,.65);
            prior(Act::Drink,1,.5);
            prior(Act::Sleep,2,.85);
            prior(Act::Sleep,3,.4);
            prior(Act::Rest,3,.4);
            prior(Act::Play,4,.35);
            prior(Act::Talk,4,.2);
            prior(Act::Talk,5,.4);
            prior(Act::Relief,6,.6);
            prior(Act::Work,7,.4);
            s.counters.initial_food+=n.food;
            s.counters.initial_money+=n.money;
        }
        // Active siblings for half of the villagers; external parents for every household.
        for(Id i=0;i<count;++i){if(i%2==1&&i<count/2){s.persons[i].parent=s.persons[i-1].parent;
                continue;
            }
            auto dob=std::chrono::year_month_day(std::chrono::sys_days(std::chrono::days(s.persons[i].birth_day)));
            auto pb=std::chrono::sys_days((dob.year()-std::chrono::years(25))/dob.month()/dob.day());
            Id parent=Id(s.persons.size());
            s.persons[i].parent=parent;
            s.persons.push_back({parent,none,std::int32_t(pb.time_since_epoch().count()),false});
        }
        std::vector<std::set<Id>> edges(count);
        std::vector<Id> target(count);
        for(Id i=0;i<count;++i)target[i]=std::min<Id>(count-2,8+Id(draw("social-target",i,0,9)));
        auto add=[&](Id a,Id b){if(a!=b&&edges[a].size()<target[a]&&edges[b].size()<target[b]){edges[a].insert(b);
                edges[b].insert(a);
            }};
        // Neighbourhood ring ensures connectedness without a complete graph.
        for(Id i=0;i<count;++i){add(i,(i+1)%count);
            add(i,(i+2)%count);
            add(i,i^1U);
        }
        for(Id round=0;round<16;++round)for(Id i=0;i<count;++i){
            if(edges[i].size()>=target[i])continue;
            for(Id k=0;k<64&&edges[i].size()<target[i];++k){Id candidate;
                if(k<32){Id base=(i/8)*8;
                    candidate=(base+Id(draw("work-contact",i,round*64+k,8)))%count;
                }
                else candidate=Id(draw("cross-contact",i,round*64+k,count));
                add(i,candidate);
            }
        }
        std::size_t edge_count=0;
        for(auto& e:edges)edge_count+=e.size();
        edge_count/=2;
        // Serial historical appointments also reserve travel buffers; no overlapping participation.
        Time when=-Time(edge_count+count*8+48)*3600000;
        std::uint64_t ep=0;
        for(Id a=0;a<count;++a)for(Id b:edges[a])if(a<b){
            ++ep;
            s.history.push_back({ep,a,b,3,when,when+300000,0});
            when+=3600000;
            for(auto [id,other]:{std::pair{a,b},std::pair{b,a}}){
                auto& n=s.npcs[id];
                Relation r;
                r.person=other;
                r.known_relative=s.persons[id].parent==s.persons[other].parent;
                r.care=.05+.25*real("projection",id,other);
                r.source=ep;
                for(unsigned j=0;j<4;++j){r.reliability[j].mean=.35+.55*real("trust",id,other*4+j);
                    r.reliability[j].count=1;
                }
                n.memory.people.push_back(r);
                std::array<std::optional<double>,6> u{};
                u[0]=.2+.25*real("projection",id,other+count);
                n.reactions.learn(ep,{{3,1},{1000+other,.5}},u,n.biology.plasticity,1,0,n.memory.people.size());
            }
        }
        for(auto& n:s.npcs){std::sort(n.memory.people.begin(),n.memory.people.end(),[](auto a,auto b){return a.person<b.person;});
            const auto extra=(n.memory.people.size()<6?8-n.memory.people.size():2);
            for(std::size_t k=0;k<extra;++k){++ep;
                s.history.push_back({ep,n.id,none,n.home,when,when+300000,std::uint8_t(k==0?1:2)});
                when+=3600000;
                std::array<std::optional<double>,6> u{};
                if(k==0)u[5]=n.innate_trigger;
                else u[0]=.25;
                n.reactions.learn(ep,{{3,1}},u,n.biology.plasticity,1,0,n.memory.people.size());
            }
            n.memory.version=1;
        }
        s.sequence=ep;
        validate(s);
        (void)today;
        return s;
    }

    void validate(const State& s){
        const auto n=s.npcs.size();
        if(s.config.profile!=model_version||n!=s.config.population||n<8||n>1024||s.places.size()!=5+n/2||s.now<0||s.now%1000)throw std::invalid_argument("invalid state profile/dimensions/time");
        if(n%2||s.persons.size()<n||s.persons.size()>3*n||s.sequence==0)throw std::invalid_argument("invalid population metadata");
        require(s.config.theft_norm_override,-1,1);
        for(const auto& person:s.persons)if(person.parent!=none&&person.parent>=s.persons.size())throw std::invalid_argument("parent id");
        std::size_t degree=0,kin=0;
        std::vector<bool> visited(n);
        std::vector<Id> stack{0};
        for(std::size_t i=0;i<n;++i){const auto& a=s.npcs[i];
            if(a.id!=i||a.home>=s.places.size()||a.place>=s.places.size()||a.food<0||a.unowned_food<0||a.money<0||a.action.type>=Act::Count)throw std::invalid_argument("invalid actor state");
            if(a.memory.people.size()<3||a.memory.people.size()>std::min<std::size_t>(32,n-2)||a.reactions.weights.size()+a.memory.people.size()>512)throw std::invalid_argument("social/learning bound");
            if(a.focus>=motives||a.action.start<0||a.action.start>s.now||a.action.end<0||a.action.id>s.sequence||a.next_review<s.now)throw std::invalid_argument("invalid action time/id");
            if(a.action.end>0){
                if(a.action.end<s.now||a.action.target==none){if(a.action.type!=Act::Wait)throw std::invalid_argument("invalid pending action");
                }
                if(a.action.type==Act::Talk){if(a.action.target>=n)throw std::invalid_argument("invalid contact target");
                }
                else if(a.action.type!=Act::Wait&&a.action.target>=s.places.size())throw std::invalid_argument("invalid place target");
            }
            if(a.intent.active&&(a.intent.procedure>=Act::Count||a.intent.destination>=s.places.size()||a.intent.motive>=motives))throw std::invalid_argument("invalid intention");
            require(a.memory.sanction);
            require(a.memory.privacy_norm);
            for(double x:a.biology.intellect)require(x);
            for(double x:{a.biology.learning,a.biology.plasticity,a.habituation,a.desire.background,a.desire.sensitivity,a.desire.rate})require(x);
            require(a.pleasant,-1,1);
            require(a.trigger,-.5,.5);
            for(double x:a.sensed)require(x);
            for(double x:a.emotions)require(x);
            for(double x:a.targets)require(x);
            for(const auto& row:a.memory.expectation)for(const auto& stat:row){require(stat.mean,-1,1);
                require(stat.variance,0,4);
                require(stat.count,0,1e15);
            }
            std::pair<Id,std::uint8_t> previous_weight{};
            bool first_weight=true;
            for(auto w:a.reactions.weights){require(w.value,-1,1);
                auto key=std::pair{w.feature,w.channel};
                if(w.channel>=6||w.updated<0||w.updated>s.now||(!first_weight&&key<=previous_weight))throw std::invalid_argument("invalid reaction state");
                previous_weight=key;
                first_weight=false;
            }
            bool relative=false;
            Id prev=none;
            for(auto r:a.memory.people){require(r.care);
                if(r.person>=n||r.person==i||(prev!=none&&r.person<=prev))throw std::invalid_argument("invalid relationship");
                prev=r.person;
                relative|=r.known_relative;
            }
            degree+=a.memory.people.size();
            kin+=relative;
            for(double x:{a.body.energy,a.body.water,a.body.sleep,a.body.fatigue,a.body.damage,a.body.pain,a.body.oxygen,a.body.activation,a.leisure,a.social,a.desire.deficit,a.desire.satiation,a.desire.value,a.memory.property_norm})require(x);
            require(a.body.temp,-1,1);
            for(auto p:a.memory.places)if(p.place>=s.places.size()||p.food>Truth::Conflict)throw std::invalid_argument("invalid personal place");
        }
        while(!stack.empty()){auto id=stack.back();
            stack.pop_back();
            if(visited[id])continue;
            visited[id]=true;
            for(auto r:s.npcs[id].memory.people)stack.push_back(r.person);
        }
        if(std::find(visited.begin(),visited.end(),false)!=visited.end())throw std::invalid_argument("disconnected social graph");
        if(n>=64&&(double(degree)/n<8||double(degree)/n>16||double(degree)/(n*(n-1))>.25||double(kin)/n<.25||double(kin)/n>.65))throw std::invalid_argument("social distribution invalid");
        for(std::size_t i=0;i<s.persons.size();++i){auto p=s.persons[i];
            if(p.id!=i)throw std::invalid_argument("person id");
            if(p.parent!=none){if(p.parent>=s.persons.size())throw std::invalid_argument("parent id");
                auto delta=p.birth_day-s.persons[p.parent].birth_day;
                if(delta<18*365||delta>51*366)throw std::invalid_argument("parent age");
                Id k=p.parent;
                std::size_t depth=0;
                while(k!=none){if(k==p.id||++depth>s.persons.size())throw std::invalid_argument("genealogy cycle");
                    k=s.persons[k].parent;
                }
            }
        }
        std::vector<Time> last(n,std::numeric_limits<Time>::min());
        std::vector<int> episodes(n);
        std::uint64_t previous=0;
        for(auto h:s.history){if(h.id<=previous||h.a>=n||h.place>=s.places.size()||h.end<=h.start||h.end>0)throw std::invalid_argument("history bounds");
            previous=h.id;
            for(auto id:{h.a,h.b})if(id!=none){if(id>=n||h.start<last[id])throw std::invalid_argument("history overlap");
                last[id]=h.end+600000;
                ++episodes[id];
            }}
        for(auto e:episodes)if(e<8||e>20)throw std::invalid_argument("historical episode count");
        std::int64_t food=s.counters.eaten,water=s.counters.drunk,money=0;
        for(auto p:s.places){if(p.food<0||p.water<0||p.money<0)throw std::invalid_argument("negative source");
            food+=p.food;
            water+=p.water;
            money+=p.money;
        }
        for(auto& p:s.npcs){food+=p.food+p.unowned_food;
            money+=p.money;
        }
        if(food!=s.counters.initial_food+s.counters.supplied_food||water!=s.counters.initial_water+s.counters.supplied_water||money!=s.counters.initial_money+s.counters.wages)throw std::invalid_argument("resource conservation");
    }
}
