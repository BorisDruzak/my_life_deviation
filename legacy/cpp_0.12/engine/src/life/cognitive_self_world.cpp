#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace life {
namespace {
constexpr std::uint64_t outcome_base=5ull<<48,recovery_base=6ull<<48,next_base=7ull<<48;
InterpretationContext context_for(const Actor& a,SelfDomain domain){
    const auto cap=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
    InterpretationContext c;c.self=a.mind.self.predict(domain);
    const Id rule=domain==SelfDomain::Work?701:domain==SelfDomain::Study?702:domain==SelfDomain::Romance?106:105;
    const double knowledge=a.mind.knowledge.get(rule);
    c.causal_clarity=unit(cap.current[5]*(.4+.3*cap.current[3]+.3*knowledge));c.perception_quality=cap.gate;
    c.rejection_bias=a.cog.rejection_bias;c.prior_causal_control=c.self.control;return c;
}
double distress(const Actor& a){return std::max({a.affect.total[0],a.affect.total[1],a.affect.total[3],a.affect.total[8],a.affect.total[9]});}
void clear_source(CognitiveState& c,const OutcomeSignal& signal){
    if(signal.stage==SelfEpisodeStage::Outcome)
        std::erase_if(c.outcome_inbox,[&](const auto& x){return x.root==signal.root&&x.stage==signal.stage;});
    else for(auto& r:c.recoveries)if(r.episode==signal.root)r.active=false;
}
}
void World::configure_self_model(bool effects){
    if(state_.now!=0||state_.self_enabled)throw std::invalid_argument("SelfModel configuration requires initial state");
    state_.self_enabled=true;state_.self_effects=effects;
    // Existing personally experienced history, not independent random beliefs.
    for(const auto& h:state_.history)for(Id id:{h.a,h.b})if(id&&id<=state_.actors.size()){
        auto& a=state_.actors[id-1];OutcomeSignal s;s.id=s.root=s.action=h.id;s.at=h.end;
        s.other=id==h.a?h.b:h.a;s.method=s.other?Method::Talk:Method::Leisure;
        s.completed=true;s.intentional=id==h.a;s.feedback_observed=true;s.goal_importance=.6;
        s.directness=.6+.4*state_.random.uniform("projection",id,h.id);
        s.observed_mask=1u<<std::size_t(Metric::Pleasantness);s.observed[std::size_t(Metric::Pleasantness)]=h.kind==3?-.2:.3;
        s.action_feedback=s.other?.5:.85;s.other_decision=s.other?.5:0;
        s.acceptance_observed=s.other!=0;s.accepted=h.kind==3?0:1;
        auto c=context_for(a,domain_of(s.method));auto e=interpret_outcome(s,c);e.attribution=attribute_outcome(e,s,c);
        a.mind.self.integrate(e,a.mind.cognition,a.mind.cognition.base[3]);
    }
    for(auto& a:state_.actors)++a.mind.version;
    validate_self_runtime();
}
void World::capture_self_decision(Actor& a,std::uint64_t action,const Decision& d){
    if(!state_.self_enabled)return;
    DecisionExperience e;e.action=action;e.at=state_.now;e.domain=d.self_domain;
    e.predicted_uncertainty=d.uncertainty;e.predicted_risk=d.risk;e.importance=d.goal_importance;
    e.self_prediction=d.self_prediction;e.thought=a.cog.best_thought;e.knowledge_source=d.social_evaluation.basis;
    e.known=d.operations>0||e.thought>0;e.intentional=true;
    auto& history=a.cog.decision_experiences;
    std::erase_if(history,[&](const auto& x){return x.action==action;});
    if(history.size()>=32)history.erase(history.begin());
    history.push_back(e);
    if(a.action.id==action)a.action.decision_experience=e;
}
void World::publish_outcome(Actor& a,OutcomeSignal s){
    if(!state_.self_enabled)return;
    if(!s.root)s.root=s.id;
    if(!s.action)s.action=s.root;
    validate_outcome(s);if(s.at>state_.now)throw std::invalid_argument("future perceived outcome");
    if(s.directness<=0||(!s.feedback_observed&&!s.observed_mask&&!s.recovery_observed))return;
    auto& c=a.cog;
    const auto& processed=s.stage==SelfEpisodeStage::Outcome?a.mind.self.primary_sources:a.mind.self.recovery_sources;
    if(processed.contains(s.root))return;
    auto same=std::find_if(c.outcome_inbox.begin(),c.outcome_inbox.end(),[&](const auto& x){return x.root==s.root&&x.stage==s.stage;});
    if(same!=c.outcome_inbox.end()){
        // A more informative visible reply can enrich the same still-unprocessed
        // failure. It never creates a second sample of the action.
        bool changed=false;
        if(s.acceptance_observed&&!same->acceptance_observed){same->acceptance_observed=true;same->accepted=s.accepted;same->other_decision=std::max(same->other_decision,s.other_decision);changed=true;}
        for(unsigned i=0;i<metric_count;++i)if((s.observed_mask&(1u<<i))&&!(same->observed_mask&(1u<<i))){same->observed[i]=s.observed[i];same->observed_mask|=1u<<i;changed=true;}
        if(changed){++c.situation_version;c.review_at=std::min(c.review_at,state_.now);}return;
    }
    if(!s.decision.known){auto it=std::find_if(c.decision_experiences.begin(),c.decision_experiences.end(),[&](const auto& x){return x.action==s.action;});if(it!=c.decision_experiences.end())s.decision=*it;}
    if(c.outcome_inbox.size()>=self_cfg::inbox_capacity){
        auto least=c.outcome_inbox.end();
        for(auto it=c.outcome_inbox.begin();it!=c.outcome_inbox.end();++it){
            const bool pinned=(c.operation==Operation::InterpretOutcome||c.operation==Operation::AttributeOutcome)&&it->root==c.self_input.root;
            if(pinned)continue;
            if(least==c.outcome_inbox.end()||outcome_priority(*it)<outcome_priority(*least)||(outcome_priority(*it)==outcome_priority(*least)&&it->at<least->at))least=it;
        }
        ++c.outcome_dropped;
        if(least==c.outcome_inbox.end()||outcome_priority(s)<outcome_priority(*least))return;
        c.outcome_inbox.erase(least);
    }
    c.outcome_inbox.push_back(s);++c.outcome_published;++c.situation_version;
    c.review_at=std::min(c.review_at,state_.now);a.review=state_.now;
}
void World::publish_action_outcome(Actor& a,bool completed,bool blocked,const Outcomes& values,std::uint16_t mask,bool interrupted){
    if(!state_.self_enabled||!a.action.id||a.action.method==Method::Idle)return;
    // A heard contact refusal has its own perception/Reply path. Do not
    // count the later generic timeout as a second experience of that refusal.
    if(blocked&&a.action.method==Method::Talk){
        const auto root=a.action.id;
        if(std::any_of(a.cog.inbox.begin(),a.cog.inbox.end(),[&](const auto& x){return x.event==root;})||
           std::any_of(a.cog.contacts.begin(),a.cog.contacts.end(),[&](const auto& x){return x.last_event==root;}))return;
    }
    OutcomeSignal s;s.id=state_.next_id++;s.root=s.id;s.action=a.action.id;s.method=a.action.method;s.other=a.action.partner;
    s.at=state_.now;s.completed=completed;s.blocked=blocked;s.interrupted=interrupted;
    s.feedback_observed=true;s.observed=values;s.observed_mask=mask;s.decision=a.action.decision_experience;
    s.goal_importance=s.decision.known?std::max(.1,s.decision.importance):.35;
    s.directness=capability(a.body,a.mind.cognition,false).gate;
    s.action_feedback=completed?.85:interrupted?.4:.1;
    if(blocked&&s.method!=Method::Talk&&s.method!=Method::Sleep)s.observed_obstacle=1;
    publish_outcome(a,s);
}
void World::publish_social_outcome(Actor& a,const InteractionObservation& o){
    if(!state_.self_enabled||(o.stage!=SocialStage::Completed&&o.stage!=SocialStage::Declined&&o.stage!=SocialStage::Cancelled))return;
    OutcomeSignal s;s.id=o.delivery;s.root=o.outcome_source?o.outcome_source:o.event;s.action=o.event;s.at=o.at;s.method=Method::Social;s.interaction=o.kind;s.other=o.other;
    s.intentional=o.initiated;s.feedback_observed=true;s.completed=o.stage==SocialStage::Completed;
    s.blocked=o.stage==SocialStage::Declined;s.interrupted=o.stage==SocialStage::Cancelled;
    s.goal_importance=o.initiated?.65:.35;s.directness=capability(a.body,a.mind.cognition,false).gate;
    s.action_feedback=s.completed?.7:.15;s.other_decision=.8;
    if(o.stage!=SocialStage::Cancelled){s.acceptance_observed=true;s.accepted=s.completed?1:0;}
    if(o.pleasure_observed){s.observed_mask=1u<<std::size_t(Metric::Pleasantness);s.observed[std::size_t(Metric::Pleasantness)]=o.pleasure;}
    publish_outcome(a,s);
}
void World::append_self_topics(const Actor& a,std::vector<AttentionTopic>& input)const{
    if(!state_.self_enabled)return;
    const auto now=state_.now;
    for(const auto& s:a.cog.outcome_inbox){
        const auto d=s.method==Method::Social?domain_of(s.interaction):domain_of(s.method);
        const std::uint8_t metric=d==SelfDomain::Work||d==SelfDomain::Study?7:d==SelfDomain::Romance?6:5;
        input.push_back({outcome_base+s.root,TopicKind::Memory,metric,s.other,unit(outcome_priority(s)+.15*unit(double(now-s.at)/600000.)),.25,s.at,now+31000,a.cog.situation_version,s.root,false});
    }
    for(const auto& r:a.cog.recoveries)if(r.active&&now>=r.evaluate_after&&now<=r.expires&&a.cog.observed_activities>r.activities_at_start)
        input.push_back({recovery_base+r.episode,TopicKind::Memory,5,0,.72+.18*r.importance,.1,r.started,now+31000,a.cog.situation_version,r.episode,false});
}
bool World::start_self_cognition(Actor& a){
    auto& c=a.cog;if(!state_.self_enabled||c.focus<outcome_base||c.focus>=next_base)return false;
    OutcomeSignal s;
    if(c.focus<recovery_base){
        const auto root=c.focus-outcome_base;
        auto it=std::find_if(c.outcome_inbox.begin(),c.outcome_inbox.end(),[&](const auto& x){return x.root==root;});
        if(it==c.outcome_inbox.end())return false;
        s=*it;
    }else{
        const auto root=c.focus-recovery_base;
        auto it=std::find_if(c.recoveries.begin(),c.recoveries.end(),[&](const auto& r){return r.active&&r.episode==root;});
        if(it==c.recoveries.end())return false;
        const auto& r=*it;
        if(state_.now<r.evaluate_after||c.observed_activities<=r.activities_at_start)return false;
        s.id=state_.next_id++;s.root=r.episode;s.action=r.action;s.at=state_.now;s.stage=SelfEpisodeStage::Recovery;s.recovery_domain=r.domain;
        s.method=r.domain==SelfDomain::Work?Method::Work:r.domain==SelfDomain::Romance?Method::PrivateIntimacy:r.domain==SelfDomain::Study?Method::Study:Method::Talk;
        s.feedback_observed=s.recovery_observed=true;s.directness=capability(a.body,a.mind.cognition,false).gate;
        s.goal_importance=r.importance;s.elapsed_recovery_ms=state_.now-r.started;
        s.recovered=unit(1-distress(a)/std::max(.05,r.initial_distress));
        s.functioning=unit(double(c.observed_functioning-r.functioning_at_start)/double(c.observed_activities-r.activities_at_start));
        // Only own sensed pain is used here; hidden health/damage is not read as knowledge.
        s.consequences=unit(signals(a.body,a.biology)[4]-r.initial_damage);
        s.decision.known=true;s.decision.action=s.action;s.decision.at=r.started;s.decision.predicted_uncertainty=r.uncertainty;
        s.action_feedback=.5;s.completed=true;
    }
    if(c.budget<2){c.active=false;return true;}
    c.self_input=s;c.self_interpreted={};c.context.resize(std::min<std::size_t>(1,c.context.size()));
    c.context.push_back({s.id,s.root,ThoughtKind::InterpretOutcome,c.focus_metric,s.other,0,s.directness,Truth::Confirmed});
    c.peak_slots=std::max(c.peak_slots,int(c.context.size()));cognition_start(a,Operation::InterpretOutcome);return true;
}
bool World::complete_self_cognition(Actor& a,Operation op,Tick started){
    if(op!=Operation::InterpretOutcome&&op!=Operation::AttributeOutcome)return false;
    auto& c=a.cog;const auto& s=c.self_input;
    const auto domain=s.stage==SelfEpisodeStage::Recovery?s.recovery_domain:s.method==Method::Social?domain_of(s.interaction):domain_of(s.method);
    auto context=context_for(a,domain);
    Thought t;t.started=started;t.metric=c.focus_metric;t.method=s.method;t.person=s.other;t.basis=s.root;t.origin=Origin::Inferred;t.status=Truth::Confirmed;
    auto finish=[&]{c.active=false;c.operation=Operation::None;c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;a.review=c.review_at;};
    if(op==Operation::InterpretOutcome){
        c.self_interpreted=interpret_outcome(s,context);t.kind=ThoughtKind::InterpretOutcome;t.value=c.self_interpreted.success;t.confidence=c.self_interpreted.perception_quality;
        t.detail=s.stage==SelfEpisodeStage::Recovery?"observed_delayed_recovery_and_functioning":"perceived_result_not_objective_psychology";emit_thought(a,t);
        if(!c.self_interpreted.actual){clear_source(c,s);finish();return true;}
        if(c.budget>0)cognition_start(a,Operation::AttributeOutcome);else finish();return true;
    }
    auto e=c.self_interpreted;e.attribution=attribute_outcome(e,s,context);
    t.kind=ThoughtKind::Attribute;t.value=e.attribution.self;t.confidence=e.attribution.confidence;
    t.detail="attribution_from_visible_feedback_and_own_schema";emit_thought(a,t);
    const auto cap=capability(a.body,a.mind.cognition,false);
    // The single runtime mutation site. Recovery also reaches this point
    // through an observed envelope and both paid cognitive operations.
    const bool changed=a.mind.self.integrate(e,a.mind.cognition,cap.current[3],[&](const SelfUpdateTrace& raw){
        auto row=raw;row.actor=a.id;if(self_logger_)self_logger_(row);
        Thought u;u.started=started;u.kind=ThoughtKind::SelfUpdate;u.method=s.method;u.metric=c.focus_metric;u.person=s.other;u.basis=row.source;
        u.value=row.after;u.prior=row.before;u.observed=row.observation;u.confidence=row.quality;u.origin=Origin::Inferred;
        u.detail=std::string(self_domain_name(row.domain))+"."+self_axis_name(row.axis);emit_thought(a,u);
    });
    if(changed){
        ++a.mind.version;
        if(s.stage==SelfEpisodeStage::Recovery)++c.recovery_integrations;
        else{
            ++c.self_integrations;++c.observed_activities;
            if(s.completed&&!s.blocked&&e.success>=.5)++c.observed_functioning;
            const auto prediction=a.mind.self.predict(e.domain);
            if(state_.self_effects){
                auto app=self_appraisal(prediction,e.importance,e.adversity,e.uncertainty);
                app.loss=e.adversity;app.progress=e.success;
                if(e.acceptance_observed)app.rejection=1-e.accepted;
                if(s.observed_mask&(1u<<std::size_t(Metric::Pleasantness)))app.pleasantness=s.observed[std::size_t(Metric::Pleasantness)];
                a.affect.set(s.root,state_.now+300000,appraisal_targets(app,a.body));
            }
            if(e.adversity>=.2){
                auto slot=std::find_if(c.recoveries.begin(),c.recoveries.end(),[&](const auto& r){return !r.active||r.expires<state_.now;});
                if(slot==c.recoveries.end())slot=std::min_element(c.recoveries.begin(),c.recoveries.end(),[](const auto& x,const auto& y){return x.importance<y.importance;});
                if(!slot->active||slot->expires<state_.now||slot->importance<=e.importance){
                    RecoveryTrace r;r.episode=s.root;r.action=s.action;r.domain=e.domain;r.started=state_.now;
                    r.evaluate_after=state_.now+self_cfg::recovery_delay;r.expires=state_.now+self_cfg::recovery_expiry;
                    r.initial_distress=std::max(distress(a),e.adversity);r.importance=e.importance;r.uncertainty=e.uncertainty;
                    r.perception_quality=e.perception_quality;r.initial_damage=signals(a.body,a.biology)[4];
                    r.functioning_at_start=c.observed_functioning;r.activities_at_start=c.observed_activities;r.active=true;*slot=r;
                }
            }
        }
    }
    clear_source(c,s);finish();return true;
}
void World::validate_self_runtime()const{
    for(const auto& a:state_.actors){
        a.mind.self.validate();const auto& c=a.cog;
        if(c.outcome_inbox.size()>self_cfg::inbox_capacity||c.decision_experiences.size()>32)throw std::runtime_error("self runtime capacity");
        for(unsigned i=0;i<c.outcome_inbox.size();++i){validate_outcome(c.outcome_inbox[i]);if(c.outcome_inbox[i].at>state_.now)throw std::runtime_error("future self outcome");for(unsigned j=0;j<i;++j)if(c.outcome_inbox[i].root==c.outcome_inbox[j].root&&c.outcome_inbox[i].stage==c.outcome_inbox[j].stage)throw std::runtime_error("duplicate queued self source");}
        if(c.operation==Operation::InterpretOutcome||c.operation==Operation::AttributeOutcome){
            validate_outcome(c.self_input);
            if(c.self_input.at>state_.now)throw std::runtime_error("future active self input");
            if(c.operation==Operation::AttributeOutcome){
                const auto& e=c.self_interpreted;
                if(e.source_event!=c.self_input.root||e.source_action!=c.self_input.action||std::size_t(e.domain)>=self_domain_count)throw std::runtime_error("active interpretation provenance");
                for(double x:{e.success,e.importance,e.adversity,e.uncertainty,e.perception_quality})require_range(x,0,1);
            }
        }
        for(const auto& d:c.decision_experiences){d.self_prediction.validate();if(!d.action||d.at>state_.now||std::size_t(d.domain)>=self_domain_count)throw std::runtime_error("decision experience identity/time");for(double x:{d.predicted_uncertainty,d.predicted_risk,d.importance})require_range(x,0,1);}
        for(const auto& r:c.recoveries)if(r.active){
            if(!r.episode||!r.action||r.started>state_.now||r.evaluate_after<r.started||r.expires<r.evaluate_after||std::size_t(r.domain)>=self_domain_count||r.functioning_at_start>c.observed_functioning||r.activities_at_start>c.observed_activities)throw std::runtime_error("self recovery identity/time");
            for(double x:{r.initial_distress,r.importance,r.uncertainty,r.perception_quality,r.initial_damage})require_range(x,0,1);
        }
    }
}
std::string World::self_report_json()const{
    std::ostringstream o;o.precision(17);o<<"{\"enabled\":"<<(state_.self_enabled?"true":"false")<<",\"effects\":"<<(state_.self_effects?"true":"false")<<",\"actors\":[";
    bool first=true;for(const auto& a:state_.actors){if(!first)o<<',';first=false;
        o<<"{\"id\":"<<a.id<<",\"outcomes\":"<<a.cog.outcome_published<<",\"dropped\":"<<a.cog.outcome_dropped<<",\"integrated\":"<<a.cog.self_integrations<<",\"recoveries\":"<<a.cog.recovery_integrations<<",\"pending\":"<<a.cog.outcome_inbox.size()<<",\"alive\":"<<(a.alive?"true":"false")<<",\"money\":"<<a.money
         <<",\"food\":"<<a.food<<",\"energy\":"<<a.body.energy<<",\"water\":"<<a.body.water<<",\"sleep\":"<<a.body.sleep
         <<",\"retained_decisions\":"<<a.cog.retained_decisions<<",\"executed_decisions\":"<<a.cog.executed_decisions<<",\"fruitless_decisions\":"<<a.cog.fruitless_decisions
         <<",\"critical_seconds\":[";
        for(unsigned j=0;j<3;++j){if(j)o<<',';o<<a.critical_by_need[j];}o<<"],\"critical_longest_seconds\":[";
        for(unsigned j=0;j<3;++j){if(j)o<<',';o<<a.critical_longest[j];}o<<"],\"failures_by_reason\":{";
        bool first_reason=true;for(const auto& [reason,count]:a.failure_reasons){if(!first_reason)o<<',';first_reason=false;o<<'"'<<reason<<"\":"<<count;}
        o<<"},\"domains\":[";
        for(unsigned i=0;i<self_domain_count;++i){if(i)o<<',';auto p=a.mind.self.predict(SelfDomain(i));o<<"{\"name\":\""<<self_domain_name(SelfDomain(i))<<"\",\"efficacy\":"<<p.efficacy<<",\"control\":"<<p.control<<",\"coping\":"<<p.coping<<",\"acceptance\":"<<p.acceptance<<",\"uncertainty_tolerance\":"<<p.uncertainty_tolerance<<",\"confidence\":"<<p.confidence<<'}';}o<<"]}";
    }o<<"]}";return o.str();
}
} // namespace life
