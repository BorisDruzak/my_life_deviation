#include "mld/world.hpp"
#include <sstream>
#include <iomanip>
#include <numeric>
namespace mld {
    World::World(Config c,bool indexed):World(generate(std::move(c)),indexed){}
    World::World(State s,bool indexed):state_(std::move(s)),indexed_(indexed){validate(state_);
        rebuild();
    }
    void World::rebuild(){occupants_.assign(state_.places.size(),{});
        due_={};
        for(auto& n:state_.npcs){if(n.action.type!=Act::Move)occupants_[n.place].push_back(n.id);
            schedule(n.id);
        }}
    void World::schedule(Id id){if(!indexed_)return;
        const auto& n=state_.npcs[id];
        Time due=n.next_review;
        if(n.action.end>0)due=std::min(due,n.action.end);
        due_.push({due,id});
    }
    void World::update_occupancy(Id id,Id old,Id current){if(old!=none){auto& b=occupants_[old];
            b.erase(std::remove(b.begin(),b.end(),id),b.end());
        }if(current!=none){auto& b=occupants_[current];
            b.insert(std::lower_bound(b.begin(),b.end(),id),id);
        }}
    void World::event(std::string_view what,Id actor,Id target,std::int64_t value){
        ++state_.counters.events;
        auto& h=state_.counters.digest;
        auto mix=[&](std::uint64_t x){for(int b=0;b<8;++b){h^=std::uint8_t(x>>(8*b));
                h*=1099511628211ULL;
            }};
        mix(std::uint64_t(state_.now));
        mix(actor);
        mix(target);
        mix(std::uint64_t(value));
        for(unsigned char c:what){h^=c;
            h*=1099511628211ULL;
        }
        if(log_)*log_<<state_.now<<','<<what<<','<<actor<<','<<target<<','<<value<<'\n';
    }

    View World::view(Id id)const{
        const auto& n=state_.npcs.at(id);
        const bool asleep=n.action.type==Act::Sleep&&n.action.asleep;
        View v;
        v.actor=id;
        v.home=n.home;
        v.place=n.action.type==Act::Move?none:n.place;
        v.now=state_.now;
        v.sensed=n.sensed;
        v.cognition=capacities(n.body,n.biology,asleep);
        v.food=n.food+n.unowned_food;
        v.money=n.money;
        v.knowledge=&n.memory;
        v.current=n.action.type;
        v.in_contact=n.action.social_accepted;
        v.intent=n.intent;
        if(asleep||v.place==none)return v;
        // Lab1 uses clear recognition of previously known co-present people at review boundaries.
        // Full exposure integration, ambiguity and recognition errors remain a separate acceptance gap.
        auto visit=[&](Id other){if(other==id)return;
            auto r=std::lower_bound(n.memory.people.begin(),n.memory.people.end(),other,[](auto r,Id x){return r.person<x;});
            if(r!=n.memory.people.end()&&r->person==other&&v.seen_people.size()<4)v.seen_people.push_back(other);
        };
        if(indexed_)for(Id other:occupants_[n.place])visit(other);
        else for(const auto& other:state_.npcs)if(other.place==n.place&&other.action.type!=Act::Move)visit(other.id);
        return v;
    }

    void World::physiology(Npc& n){
        auto type=n.action.type;
        if(type==Act::Sleep&&!n.action.asleep){
            if(n.body.sleep>=.3&&n.body.pain<.7&&n.emotions[0]<.3)n.action.sleep_progress+=1000;
            else n.action.sleep_progress=0;
            if(n.action.sleep_progress>=120000)n.action.asleep=true;
        }
        const bool sleeping=type==Act::Sleep&&n.action.asleep;
        PhysicalInput in;
        in.asleep=sleeping;
        in.load=type==Act::Move?.25:(type==Act::Work?.3:0);
        in.safety=.8;
        const auto old=n.body;
        const auto oldEmotions=n.emotions;
        n.body=advance_body(old,n.biology,in,oldEmotions,1);
        const double engagement=(type==Act::Play||type==Act::Talk||type==Act::Work||type==Act::Relief)?1:0;
        const double positive=std::max(0.,n.pleasant),negative=std::max(0.,-n.pleasant);
        n.leisure=reservoir(n.leisure,.8*positive,.3*negative,.06*n.biology.leisure_need,1./3600);
        double contact=type==Act::Talk&&n.action.social_accepted?.75:0;
        n.social=reservoir(n.social,.6*contact,0,.025*n.biology.social_need,1./3600);
        double rate=.35*engagement+.2*(1-engagement),target=.35*engagement/rate;
        n.habituation=relax(n.habituation,target,1./3600,1/rate);
        auto signals=body_signals(old);
        const double inhibition=clamp(.2*signals[3]+.3*signals[4]+.5*oldEmotions[0]);
        n.desire=advance_desire(n.desire,n.trigger,inhibition,1);
        static constexpr double up[10]={1,10,3,20,5,.5,2,3,8,20},down[10]={30,300,180,1200,180,3,60,90,600,1800};
        for(std::size_t j=0;j<10;++j){double goal=state_.now<n.target_until?n.targets[j]:0;
            n.emotions[j]=relax(oldEmotions[j],goal,1,goal>oldEmotions[j]?up[j]:down[j]);
        }
        auto& critical=state_.counters.critical_seconds;
        critical[0]+=n.body.energy<.1;
        critical[1]+=n.body.water<.1;
        critical[2]+=n.body.sleep>.9;
        critical[3]+=n.body.fatigue>.9;
        critical[4]+=n.leisure<.1;
        critical[5]+=n.social<.1;
        critical[6]+=n.desire.value>.9;
        if(sleeping&&(n.body.sleep<=.1||std::max({signals[0],signals[1],signals[4],signals[7]})>=.85)&&n.action.end>state_.now+1000){n.action.end=state_.now+1000;
            schedule(n.id);
        }
    }

    static void sense(Npc& n,Time now){
        auto signals=body_signals(n.body);
        std::array<double,8> raw={signals[0],signals[1],signals[2],signals[3],1-n.leisure,1-n.social,n.desire.value,clamp((4-double(n.money))/4)};
        auto cap=capacities(n.body,n.biology);
        auto best=std::size_t(std::max_element(raw.begin(),raw.end())-raw.begin());
        // Laboratory focus has hysteresis; full switching latency/limited exposure is not claimed.
        double threshold=.06+.18*cap[1]+.12*.5;
        if(raw[best]-raw[n.focus]>threshold||now==0||((best<4)&&raw[best]>=.85)){n.focus=std::uint8_t(best);
            n.focus_since=now;
        }
        for(std::size_t j=0;j<8;++j)n.sensed[j]=clamp(raw[j]*(.35+.65*(j==n.focus?1:.25))*(1+.2*n.emotions[1]));
    }

    void World::experience(Npc& n,bool complete){
        if(n.action.type==Act::Wait||n.action.type==Act::Move||n.action.type==Act::Inspect)return;
        const bool sleeping=n.action.type==Act::Sleep&&n.action.asleep;
        if(sleeping||n.body.oxygen>=.95)return;
        const double dose=std::min(1.,double(state_.now-n.action.last_window)/300000);
        if(dose<=0)return;
        const auto episode=++state_.sequence;
        std::array<std::optional<double>,6> primary{};
        if(n.action.type==Act::Play||n.action.type==Act::Talk||n.action.type==Act::Work)primary[0]=n.action.type==Act::Work?.15:.45;
        if(n.action.type==Act::Relief)primary[5]=n.innate_trigger;
        n.reactions.learn(episode,{{n.place==n.home?Id(5):n.place,1}},primary,n.biology.plasticity,dose,state_.now,n.memory.people.size());
        if(complete){sense(n,state_.now);
            auto a=std::size_t(n.action.type);
            for(std::size_t j=0;j<motives;++j){if(n.memory.expectation[a][j].mean==0)continue;
                if(n.action.type==Act::Exchange||n.action.type==Act::Take)continue;
                // instrumentality uses known Eat forecast
                double y=clamp(n.action.initial_sensed[j]-n.sensed[j],-1,1);
                if(n.action.type==Act::Work&&j==7)y=.4;
                if(n.action.type==Act::Relief&&j==6)y=.6;
                learn_association(n.memory.expectation[a][j],y,n.biology.learning,1,n.biology.intellect[3],1);
            }
        }
        n.action.last_window=state_.now;
        n.memory.last_learn=episode;
        ++n.memory.version;
    }

    void World::finish(Id id){
        auto& n=state_.npcs[id];
        const auto action=n.action;
        bool ok=true;
        auto stock_result=[&](bool available){auto it=std::find_if(n.memory.places.begin(),n.memory.places.end(),[&](auto p){return p.place==action.target;});
            if(it!=n.memory.places.end()){it->food=available?Truth::True:Truth::False;
                it->checked=state_.now;
                // A known-empty home has no autonomous restocking process in this catalogue.
            // The store's public delivery timetable, not hidden stock, permits a later check.
            it->retry=available?state_.now:(it->free_food?std::numeric_limits<Time>::max():((state_.now/86400000)+1)*86400000);
                it->source=action.id;
                ++n.memory.version;
            }};
        switch(action.type){
            case Act::Move:
            ok=action.target<state_.places.size()&&!state_.places[action.target].closed;
            if(ok)n.place=action.target;
            update_occupancy(id,none,n.place);
            break;
            case Act::Inspect:stock_result(state_.places[n.place].food>0);
            break;
            case Act::Exchange:case Act::Take:{auto& p=state_.places[n.place];
                bool free=n.place==n.home;
                ok=!p.closed&&p.food>0&&(action.type==Act::Take||free||n.money>=1);
                if(ok){--p.food;
                    if(action.type==Act::Take&&!free){++n.unowned_food;
                        ++state_.counters.unauthorized;
                    }else{++n.food;
                        if(!free){--n.money;
                            ++p.money;
                        }++state_.counters.transfers;
                    }stock_result(p.food>0);
                }else stock_result(p.food>0);
                break;
            }
            case Act::Eat:ok=n.food+n.unowned_food>0;
            if(ok){if(n.unowned_food>0)--n.unowned_food;
                else --n.food;
                ++state_.counters.eaten;
                n.body.energy=clamp(n.body.energy+.35);
            }break;
            case Act::Drink:ok=n.place==2&&state_.places[2].water>0;
            if(ok){--state_.places[2].water;
                ++state_.counters.drunk;
                n.body.water=clamp(n.body.water+.25);
            }break;
            case Act::Relief:satisfy(n.desire,.6);
            ++state_.counters.reliefs;
            break;
            case Act::Work:n.money+=2;
            state_.counters.wages+=2;
            break;
            case Act::Talk:
            if(!action.social_accepted){
                bool accepted=action.target<state_.npcs.size()&&state_.npcs[action.target].place==n.place&&state_.npcs[action.target].action.type!=Act::Move;
                if(accepted){sense(state_.npcs[action.target],state_.now);
                    accepted=accept_contact(view(action.target));
                }
                if(accepted){auto& other=state_.npcs[action.target];
                    experience(other,false);
                    n.action.social_accepted=true;
                    n.action.start=state_.now;
                    n.action.end=state_.now+1800000;
                    n.action.last_window=state_.now;
                    other.intent={};
                    n.intent={};
                    other.action=n.action;
                    other.action.target=id;
                    other.action.id=++state_.sequence;
                    other.action.initial_sensed=other.sensed;
                    n.pleasant=.4;
                    other.pleasant=.4;
                    n.next_review=state_.now+30000;
                    other.next_review=state_.now+30000;
                    schedule(id);
                    schedule(other.id);
                    event("contact_accepted",id,other.id);
                    return;
                }
                auto r=std::find_if(n.memory.people.begin(),n.memory.people.end(),[&](auto r){return r.person==action.target;});
                if(r!=n.memory.people.end()){r->retry=state_.now+60000;
                    ++n.memory.version;
                }
                ++state_.counters.failures;
                event("contact_refused",id,action.target);
                n.intent={};
                n.action={};
                n.next_review=state_.now;
                return;
            }
            ++state_.counters.contacts;
            break;
            default:break;
        }
        if(ok)++n.completed[std::size_t(action.type)];
        else ++state_.counters.failures;
        if(ok)experience(n,true);
        event(ok?"complete":"failed",id,action.target,std::int64_t(action.type));
        if(action.moral>0&&ok){Appraisal a;
            a.significance=.7;
            a.violation=action.moral;
            a.responsibility=1;
            n.targets=emotion_targets(a);
            n.target_until=state_.now+5000;
        }
        if(action.type!=Act::Move||!ok||(n.intent.procedure==Act::Move&&n.place==n.intent.destination))n.intent={};
        n.action={};
        n.pleasant=0;
        n.next_review=state_.now;
    }

    void World::start(Id id,const Choice& choice){
        auto& n=state_.npcs[id];
        Act a=choice.type;
        n.intent={choice.procedure,choice.destination,choice.motive,a==Act::Move};
        Id target=choice.target;
        if(n.action.type==Act::Move)return;
        // finish the physical edge before changing route
        if(n.action.type==Act::Talk&&n.action.target<state_.npcs.size()){
            auto& other=state_.npcs[n.action.target];
            if(other.action.type==Act::Talk&&other.action.target==id){experience(other,false);
                other.action={};
                other.pleasant=0;
                other.next_review=state_.now+1000;
                schedule(other.id);
                event("contact_cancel",other.id,id);
            }
        }
        if(n.action.type!=Act::Wait){experience(n,false);
            event("interrupt",id,n.action.target,std::int64_t(n.action.type));
        }
        Time duration=30000;
        switch(a){case Act::Move:duration=120000;
            break;
            case Act::Inspect:duration=10000;
            break;
            case Act::Exchange:case Act::Take:duration=15000;
            break;
            case Act::Eat:duration=600000;
            break;
            case Act::Drink:duration=120000;
            break;
            case Act::Sleep:duration=8*3600000;
            break;
            case Act::Rest:case Act::Talk:case Act::Relief:case Act::Work:duration=1800000;
            break;
            case Act::Play:duration=3600000;
            break;
            default:break;
        }
        if(a==Act::Talk)duration=10000;
        // proposal and independent reply consume game time
        n.action={};
        n.action.type=a;
        n.action.start=state_.now;
        n.action.end=state_.now+duration;
        n.action.target=target;
        n.action.id=++state_.sequence;
        n.action.last_window=state_.now;
        n.action.moral=choice.moral;
        n.action.expected=choice.score;
        n.action.initial_sensed=n.sensed;
        auto effects=n.reactions.effects({{n.place==n.home?Id(5):n.place,1}},state_.now);
        const double basic=a==Act::Work?.15:(a==Act::Play?.45:0);
        n.pleasant=basic==0?0:pleasure(basic,0,effects[0],0,0,1,n.habituation,.4,0);
        if(a==Act::Move)update_occupancy(id,n.place,none);
        if(n.pleasant>0){Appraisal ap;
            ap.significance=.5;
            ap.pleasant=n.pleasant;
            n.targets=emotion_targets(ap);
            n.target_until=n.action.end;
        }
        event("start",id,target,std::int64_t(a));
        event("decision_score_millionths",id,target,std::llround(choice.score*1e6));
        n.next_review=state_.now+30000;
        schedule(id);
    }

    void World::review(Id id){
        auto& n=state_.npcs[id];
        n.next_review=state_.now+30000;
        if(n.action.type==Act::Sleep&&n.action.asleep){schedule(id);
            return;
        }
        sense(n,state_.now);
        auto v=view(id);
        if(v.cognition[4]==0||v.place==none){schedule(id);
            return;
        }
        // Only a perceived public-room context activates this laboratory trigger.
        n.trigger=n.place==3?clamp(.5*n.innate_trigger+n.reactions.effects({{3,1}},state_.now)[5],-.5,.5):0;
        if(state_.now-n.action.last_window>=300000&&n.action.type!=Act::Wait)experience(n,false);
        auto choice=choose(v);
        ++n.decisions;
        ++state_.counters.choices;
        state_.counters.operations+=choice.used_operations;
        const bool emergency=std::max({n.sensed[0],n.sensed[1],n.sensed[2],n.sensed[3]})>=.85;
        bool change=n.action.type==Act::Wait;
        if(n.action.type!=Act::Wait&&choice.type!=n.action.type){const double threshold=.10+.30*v.cognition[7]+.20*.5;
            change=emergency||choice.score-n.action.expected>threshold;
        }
        if(change)start(id,choice);
        else schedule(id);
    }
    void World::supply(){auto count=std::int64_t(state_.npcs.size());
        if(!state_.config.scarce){state_.places[1].food+=count*4;
            state_.counters.supplied_food+=count*4;
            event("external_food_supply",none,1,count*4);
        }state_.places[2].water+=count*8;
        state_.counters.supplied_water+=count*8;
        event("external_water_supply",none,2,count*8);
    }

    void World::boundary(){
        std::vector<Id> ids;
        if(indexed_){while(!due_.empty()&&due_.top().first<=state_.now){auto [t,id]=due_.top();
                due_.pop();
                auto& n=state_.npcs[id];
                Time due=n.next_review;
                if(n.action.end>0)due=std::min(due,n.action.end);
                if(due==t)ids.push_back(id);
            }}
        else for(auto& n:state_.npcs)if(n.next_review<=state_.now||(n.action.end>0&&n.action.end<=state_.now))ids.push_back(n.id);
        std::sort(ids.begin(),ids.end());
        ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
        std::vector<std::pair<std::uint64_t,Id>> completions;
        for(Id id:ids){auto& n=state_.npcs[id];
            if(n.action.end>0&&n.action.end<=state_.now)completions.emplace_back(seeded(state_.config.seed,catalog_hash(),"commit-order",id,n.action.id),id);
        }
        std::sort(completions.begin(),completions.end());
        for(auto [key,id]:completions){(void)key;
            finish(id);
        }
        for(Id id:ids){if(state_.npcs[id].next_review<=state_.now)review(id);
            else schedule(id);
        }
    }

    void World::advance(std::uint32_t seconds){
        if(seconds>31U*86400)throw std::invalid_argument("advance interval >31 days");
        if(seconds==0)return;
        for(std::uint32_t step=0;step<seconds;++step){boundary();
            for(auto& n:state_.npcs)physiology(n);
            state_.now+=1000;
            if(state_.now%86400000==0)supply();
        }
        boundary();
        validate(state_);
    }
    std::string World::snapshot()const{validate(state_);
        return encode(state_);
    }
    World World::restore(const std::string& s,bool indexed){return World(decode(s),indexed);
    }

    std::string World::summary()const{
        double minEnergy=1,minWater=1,maxSleep=0,maxDamage=0,minLeisure=1,minSocial=1;
        std::size_t w=0;
        std::array<std::uint64_t,action_count> actions{};
        for(auto& n:state_.npcs){minEnergy=std::min(minEnergy,n.body.energy);
            minWater=std::min(minWater,n.body.water);
            maxSleep=std::max(maxSleep,n.body.sleep);
            maxDamage=std::max(maxDamage,n.body.damage);
            minLeisure=std::min(minLeisure,n.leisure);
            minSocial=std::min(minSocial,n.social);
            w+=n.reactions.weights.size();
            for(std::size_t j=0;j<action_count;++j)actions[j]+=n.completed[j];
        }
        const auto& c=state_.counters;
        std::ostringstream out;
        out<<std::setprecision(17);
        out<<"{\n\"scenario\":\""<<(state_.config.scarce?"scarcity":(state_.config.temptation?"temptation":"normal"))<<"\",\n\"property_norm_override\":"<<state_.config.theft_norm_override<<",\n";
        out<<"\"version\":\""<<model_version<<"\",\n\"seed\":"<<state_.config.seed<<",\n\"population\":"<<state_.npcs.size()<<",\n\"simulated_seconds\":"<<state_.now/1000<<",\n\"semantic_hash\":\""<<semantic_hash(state_)<<"\",\n\"event_hash\":"<<c.digest<<",\n\"events\":"<<c.events<<",\n\"decisions\":"<<c.choices<<",\n\"operations\":"<<c.operations<<",\n\"failures\":"<<c.failures<<",\n\"unauthorized_takes\":"<<c.unauthorized<<",\n\"sparse_weights\":"<<w<<",\n\"min_energy\":"<<minEnergy<<",\n\"min_water\":"<<minWater<<",\n\"max_sleep_pressure\":"<<maxSleep<<",\n\"max_damage\":"<<maxDamage<<",\n\"min_leisure\":"<<minLeisure<<",\n\"min_social\":"<<minSocial<<",\n\"critical_person_seconds\":[";
        for(std::size_t j=0;j<7;++j){if(j)out<<',';
            out<<c.critical_seconds[j];
        }out<<"],\n\"completed\":{";
        for(std::size_t j=0;j<action_count;++j){if(j)out<<',';
            out<<'"'<<action_name(Act(j))<<"\":"<<actions[j];
        }out<<"}\n}\n";
        return out.str();
    }
}
