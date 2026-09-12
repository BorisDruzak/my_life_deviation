#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace life {
namespace {
Place& at(State& s,Id id){auto it=std::find_if(s.places.begin(),s.places.end(),[&](const auto& p){return p.id==id;});if(it==s.places.end())throw std::runtime_error("missing place");return *it;}
bool moving(const Actor& a){return a.action.phase==Phase::Travel;}
std::pair<Id,Id> road(const Actor& a){Id x=a.action.path.at(a.action.leg),y=a.action.path.at(a.action.leg+1);return {std::min(x,y),std::max(x,y)};}
double position(const Actor& a,Tick now){double p=unit(double(now-a.action.segment_start)/double(a.action.end-a.action.segment_start));return a.action.path[a.action.leg]<a.action.path[a.action.leg+1]?p:1-p;}
Outcomes perceived_needs(const Actor& a,const Capability& c){
    Outcomes n{};const auto s=signals(a.body,a.biology);
    for(std::size_t i=0;i<4;++i)n[i]=sensation(s[i],a.mind.attention.focus==i+1?1:0,a.affect.total[1]);
    n[4]=1-a.leisure;n[5]=1-a.social;
    double sexual_signal=unit(a.biology.sensitivity[8]*a.desire.value*(1+.5*a.desire.value));
    n[6]=sensation(sexual_signal,a.mind.attention.focus==9?1:0,a.affect.total[1]);
    n[7]=unit((8-a.mind.believed_money)/8);
    n[8]=std::max({sensation(s[4],0,a.affect.total[1]),sensation(s[5],0,a.affect.total[1]),sensation(s[6],0,a.affect.total[1]),sensation(s[7],0,a.affect.total[1]),a.affect.total[0]});
    if(c.gate==0)n.fill(0);
    return n;
}
}
void World::rebuild_index(){
    Id maximum=0;for(const auto& p:state_.places)maximum=std::max(maximum,p.id);
    if(maximum>100000)throw std::runtime_error("place index limit");
    occupants_.assign(std::size_t(maximum)+1,{});road_occupants_.clear();
    for(const auto& a:state_.actors)if(a.alive){if(moving(a))road_occupants_[road(a)].push_back(a.id);else if(a.place<occupants_.size())occupants_[a.place].push_back(a.id);}
}
std::vector<Id> World::local_people(const Actor& a){
    std::vector<Id> candidates,result;
    if(indexed_){if(moving(a)){auto it=road_occupants_.find(road(a));if(it!=road_occupants_.end())candidates=it->second;}else if(a.place<occupants_.size())candidates=occupants_[a.place];}
    else {for(const auto& b:state_.actors)candidates.push_back(b.id);}
    metrics_.perception_candidates+=candidates.size();
    for(Id id:candidates){if(id==a.id)continue;const auto& b=state_.actors[id-1];if(!b.alive)continue;
        bool seen=!moving(a)&&!moving(b)&&a.place==b.place;
        if(moving(a)&&moving(b)&&road(a)==road(b)){double length=double(a.action.end-a.action.segment_start)/1000*1.2;seen=std::abs(position(a,state_.now)-position(b,state_.now))*length<=20;}
        if(seen)result.push_back(id);
    }
    std::sort(result.begin(),result.end());return result;
}
std::vector<Id> World::visible_for_test(Id id){rebuild_index();return local_people(state_.actors.at(id-1));}
void World::project(Actor& a){
    auto c=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
    if(c.gate==0){a.exposure.clear();a.primary_trigger=0;return;}
    std::vector<Id> visible;for(const auto& p:a.cog.percepts)if(p.recognized&&p.last>=state_.now-1000)visible.push_back(p.token);
    std::vector<std::pair<Id,double>> ranked;ranked.reserve(visible.size());
    for(Id id:visible){const auto* r=a.mind.relation(id);ranked.emplace_back(id,r?r->familiarity:0);}
    std::stable_sort(ranked.begin(),ranked.end(),[](auto x,auto y){return x.second!=y.second?x.second>y.second:x.first<y.first;});
    for(std::size_t i=0;i<visible.size();++i)visible[i]=ranked[i].first;
    if(visible.size()>4)visible.resize(4);
    a.exposure.clear();a.primary_trigger=0;
    if(!moving(a))a.exposure.push_back({100+a.place,a.place,1,1});
    if(a.action.phase!=Phase::Idle)a.exposure.push_back({100000+Id(a.action.method),a.action.id,1,.8});
    std::set<Id> styles;
    for(Id id:visible){const auto& b=state_.actors[id-1];if(a.mind.relation(id))a.exposure.push_back({10000+id,id,1,.8});if(styles.insert(b.style).second){a.exposure.push_back({200+b.style,id,1,.7});a.primary_trigger+=a.innate_trigger[b.style];}}
    if(const auto* e=social_event(a.mind.social.active_event);e&&e->phase==SocialPhase::Accepted){
        a.exposure.push_back({400000+Id(e->kind),e->id,1,1});
        if(e->object)a.exposure.push_back({500000+e->object,e->id,1,.9});
    }
    a.primary_trigger=signed_unit(a.primary_trigger);
    // This profile exposes legible shelf state at a visited counter; it does not query distant stock.
    if(!moving(a)&&a.place==3){auto& p=at(state_,3);for(auto& known:a.mind.places)if(known.id==3){Truth value=p.food>0?Truth::Confirmed:Truth::Refuted;if(known.food!=value){known.food=value;++a.mind.version;}if(p.food==0)known.retry_at=state_.now+60000;}}
}
PersonalView World::personal_view(Id id){
    rebuild_index();auto& a=state_.actors.at(id-1);project(a);auto c=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
    std::vector<Id> visible;for(const auto& p:a.cog.percepts)if(p.recognized&&p.last>=state_.now-1000)visible.push_back(p.token);
    auto view=a.mind.view(a.id,a.place,a.home,state_.now,c,perceived_needs(a,c),visible);populate_social_view(a,view);populate_economy(a,view);return view;
}
void World::emit(const Actor& a,const char* kind,const char* result,const Decision* d){
    if(!logger_)return;
    EventLog e{state_.now,a.id,a.action.id,a.action.method,kind,result,a.place,a.action.partner};
    e.object=a.action.object;if(a.action.method==Method::UseObject)e.interaction=Interaction::UseItem;
    if(d){e.score=d->score;e.moral=d->moral;e.risk=d->risk;e.operations=d->operations;e.slots=d->peak_slots;}
    logger_(e);
}
void World::physical_until(Actor& a,Tick now){
    if(now<a.physical_at)throw std::logic_error("physical time reversal");
    if(!a.alive){a.physical_at=now;return;}
    const Tick elapsed=now-a.physical_at;if(!elapsed)return;
    if(elapsed>1000)throw std::logic_error("physical interval exceeds reference maximum");
    const double seconds=double(elapsed)/1000.;auto& s=state_;
    community_account(a,elapsed);
        Input input;input.asleep=a.action.phase==Phase::Asleep;
        input.load=moving(a)?.25:(a.action.phase==Phase::Running&&!a.action.blocked?method_spec(a.action.method).load:0);
        input.temperature=moving(a)?0:at(s,a.place).temperature;
        if(s.community.enabled&&a.action.phase==Phase::Running&&a.action.method==Method::Leisure){const auto& place=at(s,a.place);if(place.recreation>=0)input.load=recreation_catalogue()[place.recreation].load;}
        const bool conscious=capability(a.body,a.mind.cognition,input.asleep).gate>0;
        Channels w;
        if(indexed_){
            if(effect_cache_.size()!=s.actors.size())effect_cache_.resize(s.actors.size());
            auto& cache=effect_cache_[a.id-1];const auto& learning=a.mind.learning;
            if(cache.revision!=learning.episodes||cache.size!=learning.weights.size()||cache.input!=a.exposure){
                cache.plan=learning.prepare(a.exposure);cache.input=a.exposure;cache.revision=learning.episodes;cache.size=learning.weights.size();
            }
            w=learning.prepared_effects(cache.plan,a.physical_at);
        }else w=a.mind.learning.effects(a.exposure,a.physical_at);
        const bool engaged=a.action.phase==Phase::Running&&!a.action.blocked&&conscious;
        double relation_effect=0,contact=0;
        if(engaged&&a.action.method==Method::Talk&&a.action.partner){const auto& b=s.actors[a.action.partner-1];if(b.action.method==Method::Talk&&b.action.id==a.action.id&&b.action.partner==a.id){const auto* r=a.mind.relation(b.id);contact=r?r->trust[3].expectation():.5;relation_effect=.1*contact;}}
        double nested_primary=0,nested_status=0;
        if(const auto* event=social_event(a.mind.social.active_event);event&&event->phase==SocialPhase::Accepted&&a.physical_at>=event->answered_at&&a.physical_at<event->ends){
            nested_primary=a.id==event->initiator?event->primary_a:event->primary_b;
            nested_status=a.id==event->initiator?event->approval_a:event->approval_b;
        }
        double base=engaged?method_spec(a.action.method).pleasure:0;
        if(engaged&&s.community.enabled&&a.action.method==Method::Leisure){const auto& place=at(s,a.place);if(place.recreation>=0)base=recreation_catalogue()[place.recreation].pleasure*a.economy.interests[place.recreation];}
        const auto category=std::size_t(a.action.method);
        const double p=pleasantness(base,0,.5*w[0],relation_effect,.25*nested_primary,0,engaged?1:0,a.satiation[category]);
        const double primary=pleasantness(base,0,0,relation_effect,.25*nested_primary,0,engaged?1:0,a.satiation[category]);
        input.activation_effect=.3*w[3];
        const auto old_body=a.body;const auto old_emotions=a.affect.total;
        advance_body(a.body,a.biology,input,old_emotions,seconds);++metrics_.body_steps;
        a.leisure=saturation(a.leisure,.8*std::max(0.0,p)+.5*std::max(0.0,w[1]),.3*std::max(0.0,-p)+.5*std::max(0.0,-w[1]),.06*a.biology.leisure_need,seconds/hour);
        a.social=saturation(a.social,.6*contact+.5*std::max(0.0,w[2]),.5*std::max(0.0,-w[2]),.025*a.biology.social_need,seconds/hour);
        for(std::size_t j=0;j<method_count;++j)a.satiation[j]=habituation(a.satiation[j],engaged&&j==category?1:0,seconds/hour);
        a.esteem=approach(a.esteem,signed_unit(.3*w[4]+.3*nested_status),seconds,1200);
        auto old_signals=signals(old_body,a.biology);
        const double inhibition=unit(.2*sensation(old_signals[3],0,old_emotions[1])+.3*sensation(old_signals[4],0,old_emotions[1])+.5*old_emotions[0]);
        a.desire.advance(seconds,.5*signed_unit(a.primary_trigger+w[5]),inhibition);
        a.affect.advance(a.physical_at,elapsed);
        if(a.action.phase!=Phase::Idle&&a.action.phase!=Phase::Travel&&conscious){
            a.action.pleasure_sum+=p*seconds;a.action.primary_sum+=primary*seconds;a.action.desire_primary_sum+=a.primary_trigger*seconds;a.action.samples+=seconds;
            for(const auto& f:a.exposure){auto it=std::lower_bound(a.action.feature_sum.begin(),a.action.feature_sum.end(),f.id,[](const auto& x,Id id){return x.first<id;});if(it==a.action.feature_sum.end()||it->first!=f.id)a.action.feature_sum.insert(it,{f.id,f.exposure*seconds});else it->second+=f.exposure*seconds;}
        }
        a.min_energy=std::min(a.min_energy,a.body.energy);a.min_water=std::min(a.min_water,a.body.water);a.max_damage=std::max(a.max_damage,a.body.damage);
        if(a.body.energy<.1||a.body.water<.1||a.body.sleep>.95)a.critical_seconds+=seconds;


    a.physical_at=now;
}
Tick World::next_boundary()const {
    Tick next=std::min(state_.next_physical,state_.next_supply);
    for(const auto& a:state_.actors){if(!a.alive)continue;
        if(a.action.phase!=Phase::Idle){next=std::min(next,a.action.end);
            if(a.action.phase!=Phase::Travel&&a.action.phase!=Phase::Asleep)next=std::min(next,a.action.next_window);}
        const auto& c=a.cog;
        if(c.operation!=Operation::None)next=std::min(next,c.due);
        if(c.rebuild_until>state_.now)next=std::min(next,c.rebuild_until);
        if(state_.autonomy&&c.review_at>state_.now&&a.action.phase!=Phase::Asleep)next=std::min(next,c.review_at);
    }
    for(const auto& a:state_.actors)if(const auto* event=social_event(a.mind.social.active_event))
        if(event->phase==SocialPhase::Proposed||event->phase==SocialPhase::Accepted)next=std::min(next,event->ends);
    return next;
}
void World::process_boundary(){
    auto& s=state_;const bool physical_tick=s.now>=s.next_physical;
    // Flush all relevant processes using OLD inputs before changing any action or perception.
    for(auto& a:s.actors){
        const bool action_due=a.action.phase!=Phase::Idle&&(a.action.end<=s.now||(a.action.phase!=Phase::Travel&&a.action.phase!=Phase::Asleep&&a.action.next_window<=s.now));
        const bool cog_due=a.cog.operation!=Operation::None&&a.cog.due<=s.now;
        if(physical_tick||action_due||cog_due||a.cog.rebuild_until==s.now){physical_until(a,s.now);
            if(a.action.method==Method::Talk&&a.action.partner)physical_until(s.actors.at(a.action.partner-1),s.now);}
    }
    if(s.now>=s.next_supply){
        const auto n=std::int64_t(s.actors.size());const std::int64_t food=s.scenario=="scarcity"?n/4:n*8,water=n*12;
        at(s,3).food+=food;at(s,4).water+=water;s.ledger.supplied_food+=food;s.ledger.supplied_water+=water;++s.ledger.supplies;s.next_supply+=86400000;
        if(logger_)logger_({s.now,0,s.next_id++,Method::Idle,"external_supply","food_and_water",3});else ++s.next_id;
    }
    for(auto& a:s.actors){
        if(!a.alive)continue;
        if(a.body.damage>=1){a.alive=false;cancel(a);emit(a,"physical","critical_damage");continue;}
        if(a.action.phase!=Phase::Idle&&a.action.end<=s.now)complete(a);
        if(a.action.phase==Phase::Asleep&&(a.body.sleep<=.10||a.body.water<.1||a.body.energy<.08))complete(a);
        if(a.action.phase!=Phase::Idle&&a.action.phase!=Phase::Travel&&a.action.phase!=Phase::Asleep&&a.action.next_window<=s.now)finish_window(a);
    }
    rebuild_index();
    if(physical_tick){
        for(auto& a:s.actors){if(!a.alive)continue;
            const auto raw=signals(a.body,a.biology);
            if(a.action.phase==Phase::Asleep&&std::max({raw[0],raw[1],raw[4],raw[5],raw[6],raw[7]})>=.85)complete(a);
            update_senses(a);project(a);
        }
        s.next_physical=s.now+1000;
    }
    if(physical_tick)community_tick();
    process_social();
    // These jobs only use frozen PERSONAL inputs. Publication waits for the whole due batch.
    std::vector<std::pair<PersonalView,PlanOption>> jobs;std::vector<Id> job_actors;
    for(const auto& a:s.actors){const auto& c=a.cog;
        if(c.operation==Operation::Forecast&&c.due<=s.now&&c.cursor<c.options.size()&&a.alive){jobs.emplace_back(c.snapshot,c.options[c.cursor]);job_actors.push_back(a.id);}}
    if(!workers_)workers_=std::make_shared<ForecastWorkers>(1);
    const auto results=workers_->evaluate(jobs,work_chunk_);std::size_t j=0;
    // Phase 3: collect every thought BEFORE phase 5 issues commands for this timestamp.
    for(auto& a:s.actors){auto& c=a.cog;if(c.operation!=Operation::None&&c.due<=s.now){
        const Decision* result=nullptr;if(j<job_actors.size()&&job_actors[j]==a.id){result=&results[j++];}
        cognition_complete(a,result);
    }}
    // Phase 5, authoritative action commits. Same-time cognition results were collected first.
    auto commands=std::move(pending_intents_);pending_intents_.clear();
    for(const auto& [actor,decision]:commands){auto& a=s.actors.at(actor-1);if(!a.alive)continue;
        physical_until(a,s.now);
        if(decision.method==Method::Social)start_social(a,decision);
        else {if(a.action.phase!=Phase::Idle)cancel(a);start(a,decision);}
        a.cog.captured_mind=a.mind.version;a.cog.captured_situation=a.cog.situation_version;
    }
    auto answers=std::move(pending_social_answers_);pending_social_answers_.clear();
    for(const auto& answer:answers)if(s.actors.at(answer.actor-1).alive)
        social_answer(s.actors[answer.actor-1],answer.offer,answer.accepted,answer.evaluation);
    for(auto& a:s.actors){if(!a.alive)continue;
        const bool relevant=physical_tick||a.cog.rebuild_until==s.now||(a.action.phase!=Phase::Asleep&&a.cog.operation==Operation::None&&(a.cog.review_at<=s.now||(a.review<=s.now&&a.cog.captured_mind!=a.mind.version)));
        if(relevant){physical_until(a,s.now);cognition_tick(a);}
    }
}
void World::step(){run_ms(1000);}
void World::run_ms(std::uint64_t milliseconds){
    if(milliseconds>366ull*86400000||state_.now>std::numeric_limits<Tick>::max()-Tick(milliseconds))throw std::invalid_argument("run duration limit");
    const Tick end=state_.now+Tick(milliseconds);
    // At t=0 consume initial observations once. run_ms(0) remains a pure validation.
    if(milliseconds&&state_.next_physical==state_.now)process_boundary();
    while(state_.now<end){
        Tick next=next_boundary();if(next<=state_.now)throw std::logic_error("non-progressing logical queue");
        state_.now=std::min(next,end);
        // External API stopping points do not split integrators or create observations.
        if(next<=end)process_boundary();
    }
    validate();
}
void World::run_seconds(std::uint64_t seconds){
    if(seconds>366ull*86400)throw std::invalid_argument("run duration limit");
    run_ms(seconds*1000);
}
void World::command_for_test(Id id,Method method,Id destination){
    auto& a=state_.actors.at(id-1);if(a.place!=destination)throw std::invalid_argument("test command requires actor already at destination");
    cancel(a);Decision d;d.method=method;d.place=destination;d.path.push_back(destination);start(a,d);
}
}
