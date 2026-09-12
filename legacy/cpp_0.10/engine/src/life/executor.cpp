#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace life {
namespace {
Place& location(State& s,Id id){
    auto it=std::find_if(s.places.begin(),s.places.end(),[&](const Place& p){return p.id==id;});
    if(it==s.places.end())throw std::runtime_error("executor: invalid location");
    return *it;
}
const Edge& connection(const State& s,Id a,Id b){
    auto it=std::find_if(s.edges.begin(),s.edges.end(),[&](const Edge& e){return (e.a==a&&e.b==b)||(e.a==b&&e.b==a);});
    if(it==s.edges.end())throw std::runtime_error("executor: invalid route edge");
    return *it;
}
Outcomes own_state(const Actor& a){
    Outcomes o{};o[0]=a.body.energy;o[1]=a.body.water;o[2]=a.body.sleep;o[3]=a.body.fatigue;
    o[4]=a.leisure;o[5]=a.social;o[6]=a.desire.deficit;o[7]=a.money;return o;
}
void remember_food(Actor& a,const Place& p,Tick now){
    for(auto& k:a.mind.places)if(k.id==p.id){k.food=p.food>0?Truth::Confirmed:Truth::Refuted;k.retry_at=now+60000;}
    ++a.mind.version;
}
void remember_contact(Actor& a,Id other,std::uint64_t root,Tick now){
    auto* r=a.mind.relation(other);
    if(!r){
        // Cold-state overflow is not implemented: creation must respect the shared cap.
        if(a.mind.learning.address_slots+a.mind.learning.weights.size()>=512)return;
        Relation value;value.person=other;value.origin=root;
        auto it=std::lower_bound(a.mind.relations.begin(),a.mind.relations.end(),other,[](const Relation& x,Id id){return x.person<id;});
        r=&*a.mind.relations.insert(it,value);++a.mind.learning.address_slots;
    }
    r->last_contact=now;r->familiarity=1-(1-r->familiarity)*std::exp(-.7/5);
    r->trust[3].receive({root,root,.8,.7});
    // The experienced, voluntary contact is the ground; a role alone never grants care.
    const double rate=.03*a.mind.cognition.plasticity*.7*.3;
    r->care=unit(r->care+(1-std::exp(-rate))*(1-r->care));++a.mind.version;
}
}
void World::start(Actor& a,const Decision& d){
    physical_until(a,state_.now);
    if(std::size_t(d.method)>=method_count)throw std::invalid_argument("invalid method");
    auto& s=state_;Action action;action.method=d.method;action.id=s.next_id++;action.destination=d.place?d.place:a.place;
    action.partner=d.partner;action.object=d.object;action.path=d.path.empty()?std::vector<Id>{a.place}:d.path;
    action.started=s.now;action.window_start=s.now;action.next_window=s.now+300000;action.chosen_score=d.score;
    if(action.path.front()!=a.place||action.path.back()!=action.destination)throw std::runtime_error("invalid command route");
    a.action=std::move(action);++a.cog.situation_version;emit(a,"intention","selected",&d);
    if(a.action.path.size()>1){
        const auto& e=connection(s,a.action.path[0],a.action.path[1]);
        a.action.phase=Phase::Travel;a.action.segment_start=s.now;a.action.blocked=!e.open;
        a.action.end=s.now+(e.open?Tick(e.seconds)*1000:15000);
    }else begin_at_destination(a);
}
void World::begin_at_destination(Actor& a){
    physical_until(a,state_.now);
    auto& s=state_;auto& x=a.action;auto& p=location(s,a.place);
    x.phase=Phase::Running;x.end=s.now+Tick(method_spec(x.method).seconds)*1000;
    x.window_start=s.now;x.next_window=s.now+300000;x.segment_start=s.now;
    x.samples=0;x.pleasure_sum=x.primary_sum=x.desire_primary_sum=0;x.feature_sum.clear();
    x.blocked=false;x.before=own_state(a);
    if(x.method!=Method::Idle&&x.method!=Method::UseObject&&!(p.services&service(x.method)))x.blocked=true;
    if(x.method==Method::UseObject){auto it=std::find_if(s.social.objects.begin(),s.social.objects.end(),[&](const auto& o){return o.id==x.object;});if(it==s.social.objects.end()||it->holder!=a.id)x.blocked=true;}
    if(state_.community.enabled){
        if(x.method==Method::Work){const auto hour=(s.now%86400000)/3600000;
            if(p.job_level<0||a.mind.knowledge.get(job_catalogue()[unsigned(p.job_level)].skill)<.6||hour<9||hour>=17||(s.now/86400000)%7>=5||a.economy.work_today>=8)x.blocked=true;}
        if(x.method==Method::Study&&a.economy.study_today>=2)x.blocked=true;
        const double price=x.method==Method::Leisure?p.fee:x.method==Method::Study?4:0;
        if(price>a.money)x.blocked=true;
        if(!x.blocked&&price>0){a.money-=price;p.account+=price;a.mind.believed_money=a.money;++a.mind.version;
            if(x.method==Method::Leisure){a.economy.leisure_spent+=price;++s.community.paid_leisure;}else a.economy.study_spent+=price;}
    }
    if(x.method==Method::Eat&&a.food<=0)x.blocked=true;
    if(x.method==Method::Drink&&p.water<=0)x.blocked=true;
    if(x.method==Method::AcquireFood){
        if(p.food-p.reserved>0){++p.reserved;x.reserved=true;}else x.blocked=true;
    }
    if(x.method==Method::TakeFood&&p.food<=0)x.blocked=true;
    if(x.method==Method::PrivateIntimacy&&(a.home!=a.place||p.home_owner!=a.id))x.blocked=true;
    if(x.method==Method::Sleep){x.phase=Phase::SleepAttempt;x.end=s.now+120000;}
    if(x.method==Method::Talk){
        // A request is answered independently from the target's own state and perceived relationship.
        rebuild_index();auto visible=local_people(a);Actor* partner=nullptr;
        for(Id id:visible){
            if(x.partner&&id!=x.partner)continue;
            auto& b=s.actors[id-1];physical_until(b,s.now);const auto cap=capability(b.body,b.mind.cognition,b.action.phase==Phase::Asleep);
            const bool interruptible=b.action.phase==Phase::Idle || (b.action.phase==Phase::Waiting&&b.action.method==Method::Talk) || (b.action.phase==Phase::Running&&(b.action.method==Method::Idle||b.action.method==Method::Leisure||b.action.method==Method::Rest));
            const auto* r=b.mind.relation(a.id);const double safety=r?r->trust[3].expectation():.5;
            if(b.alive&&cap.gate>0&&interruptible&&1-b.social>.15&&safety>.2){partner=&b;break;}
        }
        if(partner){
            auto& b=*partner;cancel(b);x.partner=b.id;
            Action other;other.method=Method::Talk;other.phase=Phase::Running;other.id=x.id;other.destination=b.place;other.partner=a.id;
            other.path.push_back(b.place);other.started=other.window_start=other.segment_start=s.now;other.end=x.end;other.next_window=s.now+300000;other.before=own_state(b);b.action=std::move(other);
            b.review=s.now+30000;project(b);emit(b,"response","accepted");
            reply_observed(a,b.id,ReplyMessage::Accepted,x.id);
        }else{
            x.phase=Phase::Waiting;x.end=s.now+30000;x.blocked=true;
            Id addressed=x.partner;if(!addressed&&!visible.empty())addressed=visible.front();
            if(addressed&&std::find(visible.begin(),visible.end(),addressed)!=visible.end()){
                const auto& b=s.actors[addressed-1];
                // A public, truthful phrase is delivered, never the private rejection threshold.
                ReplyMessage msg=b.action.method==Method::Work?ReplyMessage::Busy:ReplyMessage::Declined;
                reply_observed(a,addressed,msg,x.id);
                auto& receiver=s.actors[addressed-1];
                for(const auto& m:receiver.cog.contacts)if(m.person==a.id&&s.now-m.last_reply<60000)++receiver.cog.pressure_events;
            }
            emit(a,"response",addressed?"declined_contact":"no_available_partner");
        }
    }
    project(a);
}
void World::finish_window(Actor& a){
    auto& x=a.action;const Tick elapsed=state_.now-x.window_start;
    if(elapsed>0&&x.samples){
        const auto cap=capability(a.body,a.mind.cognition,false);
        Episode e;e.id=state_.next_id++;e.method=x.method;e.time=state_.now;e.dose=std::min(1.0,double(elapsed)/300000);
        e.quality=cap.gate;e.conscious=cap.gate>0;e.outcome[std::size_t(Metric::Pleasantness)]=x.pleasure_sum/x.samples;
        e.observed=1u<<std::size_t(Metric::Pleasantness);
        e.primary[0]=x.primary_sum/x.samples;e.primary[5]=x.desire_primary_sum/x.samples;e.primary_observed=(1u<<0)|(1u<<5);
        for(auto [id,value]:x.feature_sum)e.features.push_back({id,x.id,std::min(1.0,value/x.samples),.8});
        if(a.mind.learning.observe(e,a.mind.cognition,cap.current[3]))++a.mind.version;
    }
    x.window_start=state_.now;x.next_window=state_.now+300000;x.samples=0;x.pleasure_sum=x.primary_sum=x.desire_primary_sum=0;x.feature_sum.clear();
}
void World::cancel(Actor& a){
    physical_until(a,state_.now);
    stop_social(a);
    if(a.action.phase==Phase::Idle)return;
    finish_window(a);
    if(state_.community.enabled&&a.action.method==Method::Work&&a.action.phase==Phase::Running&&!a.action.blocked){
        const double pay=location(state_,a.place).wage*double(state_.now-a.action.segment_start)/3600000.;
        a.money+=pay;state_.ledger.wages+=pay;a.economy.earned+=pay;a.mind.believed_money=a.money;++a.mind.version;
    }
    if(a.action.method==Method::Talk&&a.action.partner){
        auto& b=state_.actors[a.action.partner-1];physical_until(b,state_.now);
        if(b.action.id==a.action.id&&b.action.partner==a.id){
            finish_window(b);emit(b,"response","partner_stopped");b.action=Action{};b.review=state_.now;b.exposure.clear();
        }
    }
    if(a.action.reserved){--location(state_,a.action.destination).reserved;a.action.reserved=false;}
    emit(a,"action","interrupted");a.action=Action{};a.review=state_.now;a.exposure.clear();
}
void World::fail(Actor& a,const char* reason){
    physical_until(a,state_.now);
    ++a.failures;
    if(a.action.reserved){--location(state_,a.action.destination).reserved;a.action.reserved=false;}
    // An unavailable facility is remembered only after a real local attempt.
    for(auto& k:a.mind.places)if(k.id==a.place)k.retry_method[std::size_t(a.action.method)]=state_.now+60000;
    if(a.action.method==Method::AcquireFood||a.action.method==Method::TakeFood)remember_food(a,location(state_,a.place),state_.now);
    if(a.action.method==Method::Talk||a.action.method==Method::Sleep){
        const auto cap=capability(a.body,a.mind.cognition,false);Episode e;e.id=state_.next_id++;e.time=state_.now;
        e.method=a.action.method;e.quality=cap.gate;e.conscious=cap.gate>0;
        const auto j=a.action.method==Method::Talk?std::size_t(Metric::Social):std::size_t(Metric::Sleep);
        e.observed=std::uint16_t(1u<<j);e.outcome[j]=0;if(a.mind.learning.observe(e,a.mind.cognition,cap.current[3]))++a.mind.version;
    }
    emit(a,"action",reason);a.action=Action{};a.review=state_.now;a.exposure.clear();
}
void World::complete(Actor& a){
    physical_until(a,state_.now);
    auto& s=state_;auto& x=a.action;
    if(x.phase==Phase::Idle)return;
    if(x.phase==Phase::Travel){
        if(x.blocked){
            const auto u=x.path[x.leg],v=x.path[x.leg+1];
            for(auto& edge:a.mind.map)if((edge.from==u&&edge.to==v)||(edge.from==v&&edge.to==u))edge.open=Truth::Refuted;
            ++a.mind.version;fail(a,"route_obstruction");return;
        }
        a.place=x.path[++x.leg];++a.cog.situation_version;emit(a,"movement","arrived_node");
        if(x.leg+1<x.path.size()){
            const auto& e=connection(s,x.path[x.leg],x.path[x.leg+1]);x.blocked=!e.open;x.segment_start=s.now;x.end=s.now+(e.open?Tick(e.seconds)*1000:15000);
        }else begin_at_destination(a);
        return;
    }
    if(x.phase==Phase::SleepAttempt){
        finish_window(a);
        if(!x.blocked&&a.body.sleep>=.30&&signals(a.body,a.biology)[4]<.7&&a.affect.total[0]<.3){
            x.phase=Phase::Asleep;x.before=own_state(a);x.end=s.now+28800000;project(a);emit(a,"action","asleep");return;
        }
        fail(a,"sleep_attempt_failed");return;
    }
    finish_window(a);
    const auto method=x.method;const std::size_t m=std::size_t(method);auto& p=location(s,a.place);
    if(x.blocked){fail(a,"attempt_failed");return;}
    stop_social(a);
    if(method==Method::UseObject){if(!complete_object_use(a)){fail(a,"item_no_longer_held");return;}++a.completed[m];emit(a,"action","completed");a.action=Action{};a.review=s.now;a.exposure.clear();return;}
    Episode e;e.id=s.next_id++;e.method=method;e.time=s.now;
    const auto cap=capability(a.body,a.mind.cognition,false);e.conscious=cap.gate>0;e.quality=cap.gate;
    const auto result=[&](Metric metric,double gain){const auto j=std::size_t(metric);e.observed|=std::uint16_t(1u<<j);e.outcome[j]=signed_unit(gain);};
    if(method==Method::Eat){
        if(a.food<=0){fail(a,"no_held_food");return;}
        --a.food;++s.ledger.eaten_food;++a.consumed_food;const double old=a.body.energy;a.body.energy=unit(old+.35);result(Metric::Food,a.body.energy-old);
    }else if(method==Method::Drink){
        if(p.water<=0){fail(a,"source_empty");return;}
        --p.water;++s.ledger.drunk_water;++a.consumed_water;const double old=a.body.water;a.body.water=unit(old+.25);result(Metric::Water,a.body.water-old);
    }else if(method==Method::AcquireFood){
        const double price=s.community.enabled?p.fee:1;
        if(p.food<=0||a.money<price){fail(a,"exchange_unavailable");return;}
        --p.food;++a.food;a.money-=price;p.account+=price;
        if(s.community.enabled)a.economy.food_spent+=price;
        if(x.reserved){--p.reserved;x.reserved=false;}
        remember_food(a,p,s.now);
    }else if(method==Method::TakeFood){
        if(p.food<=0){fail(a,"source_empty");return;}
        --p.food;++a.food;++a.violations;remember_food(a,p,s.now);
        // The debug violation count is not projected as a moral verdict to witnesses.
        // A generic perceived-claim interpreter is a documented gap of profile 0.7.
    }else if(method==Method::Inspect){remember_food(a,p,s.now);}
    else if(method==Method::Work){const double pay=s.community.enabled?p.wage*double(s.now-x.segment_start)/3600000.:2;a.money+=pay;s.ledger.wages+=pay;if(s.community.enabled)a.economy.earned+=pay;result(Metric::Money,.25);}
    else if(method==Method::PrivateIntimacy){const double old=a.desire.deficit;a.desire.satisfy(x.id,.6);result(Metric::Desire,old-a.desire.deficit);}
    else if(method==Method::Sleep){result(Metric::Sleep,x.before[2]-a.body.sleep);}
    else if(method==Method::Rest){result(Metric::Rest,x.before[3]-a.body.fatigue);}
    else if(method==Method::Leisure){result(Metric::Leisure,a.leisure-x.before[4]);}
    else if(method==Method::Talk){result(Metric::Social,a.social-x.before[5]);if(x.partner)remember_contact(a,x.partner,x.id,s.now);}
    community_finish(a,method);
    if(e.observed)a.mind.learning.observe(e,a.mind.cognition,cap.current[3]);
    // A perceived result invalidates the old assessment, not magically supplies the new value.
    for(std::size_t j=0;j<metric_count;++j)if(e.observed&(1u<<j))a.cog.needs[j].status=Truth::Unknown;
    a.mind.believed_food=int(a.food);a.mind.believed_money=a.money;++a.mind.version;
    if(method!=Method::Idle){
        Appraisal app;app.significance=.5;app.progress=e.observed?.5:0;
        if(method==Method::TakeFood){app.violation=a.mind.norms[m];app.responsibility=1;}
        a.affect.set(x.id,s.now+60000,appraisal_targets(app,a.body));
    }
    for(auto& reading:a.cog.needs){
        if(reading.status==Truth::Confirmed){reading.prior=reading.observation;reading.prior_quality=std::min(.5,reading.quality);reading.prior_source=reading.source;}
    }
    if(method==Method::Work&&a.money>=(s.community.enabled?125:8)){a.cog.project_id=0;a.cog.project_paused=false;}
    ++a.completed[m];emit(a,"action","completed");a.action=Action{};a.review=s.now;a.exposure.clear();
}
}
