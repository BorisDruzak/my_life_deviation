#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <bit>
#include <set>
#include <sstream>
#include <iomanip>
#include <locale>
#include <stdexcept>
namespace life {
std::string norm_trace_json(const NormTrace& t){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    const auto quoted=[&](const std::string& s){
        out<<'"';
        constexpr char hex[]="0123456789abcdef";
        for(unsigned char c:s){
            if(c=='"'||c=='\\')out<<'\\'<<char(c);
            else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];
            else out<<char(c);
        }
        out<<'"';
    };
    out<<"{\"at\":"<<t.at<<",\"actor\":"<<t.actor<<",\"kind\":";quoted(t.kind);
    out<<",\"reason\":";quoted(t.reason);
    out<<",\"delivery\":"<<t.delivery<<",\"known_source\":"<<t.known_source
       <<",\"evidence_revision\":"<<t.evidence_revision<<",\"revision\":"<<t.evidence_revision
       <<",\"own_revision\":"<<t.own_revision<<",\"source_revision\":"<<t.source_revision
       <<",\"question\":"<<t.question<<",\"action\":"<<t.action
       <<",\"practice\":"<<t.key.practice<<",\"group\":"<<t.key.local_group
       <<",\"context\":"<<t.key.context<<",\"actor_role\":"<<t.key.actor_role<<",\"variant\":"<<t.key.variant
       <<",\"before\":"<<t.before<<",\"after\":"<<t.after<<",\"value\":"<<t.value
       <<",\"ledger_owner\":"<<unsigned(t.ledger_owner)<<",\"consequence_kind\":"<<unsigned(t.consequence_kind)
       <<",\"consequence_target\":"<<t.consequence_target<<",\"consequence_object\":"<<t.consequence_object
       <<",\"consequence_horizon\":"<<t.consequence_horizon<<",\"probability\":"<<t.probability
       <<",\"time_hours\":"<<t.time_hours<<",\"present_value\":"<<(t.present_value?"true":"false")
       <<",\"knownness\":"<<unsigned(t.knownness)<<",\"candidate_method\":"<<t.candidate_method
       <<",\"candidate_interaction\":"<<t.candidate_interaction<<",\"candidate_object\":"<<t.candidate_object<<'}';
    return out.str();
}
namespace {
constexpr Tick norm_day=86400000;
constexpr std::uint64_t input_base=11ull<<48, question_base=12ull<<48, norm_reply_base=13ull<<48, norm_end=14ull<<48;
bool unit_value(double x){return std::isfinite(x)&&x>=0&&x<=1;}
NormKey generic(NormPractice p,std::uint32_t variant=0){return {practice_id(p),0,0,0,variant};}
NormObservation interpret(const NormObservation& in,double gate){
    auto out=in;out.confidence*=unit(gate);
    // Hearing another person is not accepting their argument.
    if(in.source.origin==NormOrigin::Reported&&in.channel==NormChannel::PersonalPrinciple)out.accepted_argument=false;
    // A missing value and silence remain missing, including after interpretation.
    if(!in.applicable||!in.value_known)out.dose=0;
    return out;
}
double mismatch(const Actor& a,const NormKey& key,const NormObservation& input){
    if(key.practice==practice_id(NormPractice::ClothingCondition))return a.mind.civil.garment_condition<.4?1-a.mind.civil.garment_condition:0;
    // Approval belongs to this exact variant; only wearing it makes its disapproval an own loss.
    if(key.practice==practice_id(NormPractice::ClothingTier))return a.mind.civil.garment_tier==key.variant?1:0;
    (void)input;
    // An applicable report is not evidence of an unmet own plan.
    return 0;
}
std::uint64_t question_basis(const Actor& a,const NormPrediction& p,double significance){
    auto basis=p.own_revision;
    auto mix=[&](std::uint64_t value){basis=(basis^value)*1099511628211ull;};
    mix(std::bit_cast<std::uint64_t>(significance));
    if(p.key.practice==practice_id(NormPractice::ClothingCondition)||p.key.practice==practice_id(NormPractice::ClothingTier)){
        mix(a.mind.civil.garment_tier);mix(std::uint64_t(a.mind.civil.garment_condition*20));
        for(const auto& shop:a.mind.civil.shops)mix(shop.source);
        mix(std::uint64_t(std::max(0.,a.mind.believed_money)));
    }
    return basis?basis:1;
}
bool active_norm_operation(Operation op){return op>=Operation::InterpretNormObservation&&op<=Operation::ReflectPersonalPrinciple;}
void finish(Actor& a){auto& c=a.cog;c.active=false;c.operation=Operation::None;c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;a.review=c.review_at;}
}
bool norm_learning_active(NormMode m){return m!=NormMode::Legacy&&m!=NormMode::FrozenLearning;}
bool norm_effects_active(NormMode m){return m==NormMode::Enabled||m==NormMode::FrozenLearning;}
const char* norm_mode_name(NormMode m){switch(m){case NormMode::Legacy:return "Legacy";case NormMode::Shadow:return "Shadow";case NormMode::Enabled:return "Enabled";case NormMode::FrozenLearning:return "FrozenLearning";case NormMode::NoNormDecisionEffects:return "NoNormDecisionEffects";}throw std::invalid_argument("norm mode");}
NormMode parse_norm_mode(const std::string& s){if(s=="legacy")return NormMode::Legacy;if(s=="shadow")return NormMode::Shadow;if(s=="enabled")return NormMode::Enabled;if(s=="frozen-learning")return NormMode::FrozenLearning;if(s=="no-effects")return NormMode::NoNormDecisionEffects;for(auto m:{NormMode::Legacy,NormMode::Shadow,NormMode::Enabled,NormMode::FrozenLearning,NormMode::NoNormDecisionEffects})if(s==norm_mode_name(m))return m;throw std::invalid_argument("unknown norm mode: "+s);}
const KnownNormGroup* NormContextMemory::group(Id id)const{auto it=std::find_if(groups.begin(),groups.end(),[&](const auto& g){return g.id==id;});return it==groups.end()?nullptr:&*it;}
bool NormContextMemory::member_known(Id id,Id member)const{const auto* g=group(id);return g&&std::any_of(g->members.begin(),g->members.end(),[&](const auto& m){return m.person==member&&m.source;});}
double NormContextMemory::group_significance(Id id)const{double v=0;for(const auto& g:goals)if(g.group==id&&g.active&&g.source)v=std::max(v,g.importance*g.relevance);return v;}
void NormContextMemory::validate(Tick now)const{
    std::set<Id> ids;for(const auto& g:groups){if(!g.id||!g.source||!ids.insert(g.id).second)throw std::runtime_error("invalid known norm group");std::set<Id> people;for(const auto& m:g.members)if(!m.person||!m.source||m.at>now||!people.insert(m.person).second)throw std::runtime_error("invalid norm membership");}
    for(const auto& g:goals)if(!g.goal||!g.source||!group(g.group)||!unit_value(g.importance)||!unit_value(g.relevance))throw std::runtime_error("invalid norm goal");
    std::set<std::tuple<std::uint32_t,std::uint32_t,std::uint32_t,std::uint64_t>> unique;
    for(const auto& q:questions)if(!q.id||!unit_value(q.tension)||q.changed>now||!unique.emplace(q.key.practice,q.key.local_group,q.key.context,q.goal).second)throw std::runtime_error("invalid norm question");
}
void World::configure_civil_budget(CivilProfile profile){if(unsigned(profile.budget_policy)>1||!unit_value(profile.risk_importance))throw std::invalid_argument("civil budget profile");state_.civil.profile=profile;for(auto& a:state_.actors)++a.mind.version;}
void World::configure_norm_memory(NormProfile profile){
    profile.validate();if(state_.now)throw std::invalid_argument("norm profile requires initial world");
    if(!state_.civil.enabled)configure_recovery();state_.norm_profile=profile;
    const std::array<NormPractice,social_norm_count> mapping={NormPractice::Boundary,NormPractice::Privacy,NormPractice::Property,NormPractice::Modesty,NormPractice::PersonalRomance,NormPractice::Honesty,NormPractice::Promise};
    for(auto& a:state_.actors){
        a.mind.norm_memory=NormMemory(profile);a.mind.norm_context={};a.cog.norm={};a.cog.norm.frozen_shadow.configure(profile);
        if(profile.mode==NormMode::Legacy)continue;
        auto seed=[&](NormKey key,double weight){NormSource source;source.delivery=source.known_root=state_.next_id++;source.revision=1;source.origin=NormOrigin::LegacyPrior;source.independently_grounded=true;a.mind.norm_memory.seed_principle(key,weight,source,0);};
        for(std::size_t i=0;i<mapping.size();++i)seed(generic(mapping[i]),a.mind.social.norms[i]);
        seed(generic(NormPractice::Property,1),a.mind.norms[std::size_t(Method::TakeFood)]);
        seed(generic(NormPractice::PersonalRomance,1),a.mind.norms[std::size_t(Method::PrivateIntimacy)]);
        seed(generic(NormPractice::Privacy,1),a.mind.social.community.confidentiality);
        seed(generic(NormPractice::StrangerRomance),a.mind.social.community.stranger_romance);
        seed(generic(NormPractice::KinRomance),a.mind.social.community.kin_romance);
        // Authored contexts are local known relationships, never a global group roster.
        for(std::uint32_t id=1;id<=4;++id){KnownNormGroup g;g.id=id;g.context=id;g.place=id==1&&a.employment.workplace?a.employment.workplace:id==2?2:a.home;g.source=state_.next_id++;g.members.push_back({a.id,g.source,0});
            // Initial membership is justified by a specific known role, never mere familiarity.
            for(const auto& r:a.mind.relations)if(r.origin&&((id==1&&r.person==a.employment.supervisor)||(id==4&&r.role==1)))g.members.push_back({r.person,r.origin,0});
            if(id==1&&a.employment.supervisor&&a.employment.supervisor!=a.id&&std::none_of(g.members.begin(),g.members.end(),[&](const auto& m){return m.person==a.employment.supervisor;}))g.members.push_back({a.employment.supervisor,a.employment.source?a.employment.source:g.source,0});
            // Personally attended past contacts identify this local company.
            if(id==2)for(const auto& h:state_.history)if(h.place==g.place&&(h.a==a.id||h.b==a.id)){const Id other=h.a==a.id?h.b:h.a;if(other&&std::none_of(g.members.begin(),g.members.end(),[&](const auto& m){return m.person==other;}))g.members.push_back({other,h.id,h.end});}
            a.mind.norm_context.groups.push_back(g);
        }
        a.mind.norm_context.employment_source=a.employment.source;
        // Only explicit own goals supply significance. No cultural personality scalar.
        if(a.employment.organization&&a.employment.source)a.mind.norm_context.goals.push_back({a.employment.source,a.employment.source,1,a.employment.promise_importance,1,true});
        for(const auto& p:a.mind.projects.projects)if(p.live()&&p.goal.kind==GoalKind::FindCompany){a.mind.norm_context.goals.push_back({p.id,p.goal.basis,2,p.goal.importance,1,true});break;}
        auto prior_source=[&]{NormSource source;source.delivery=source.known_root=state_.next_id++;source.revision=1;source.origin=NormOrigin::LegacyPrior;source.independently_grounded=true;return source;};
        auto approval_prior=[&](NormPractice practice,double disapprove){
            // Preserve the known disapproval marginal; the two unspecified categories
            // retain the equal conditional odds of the approved symmetric prior.
            a.mind.norm_memory.seed_approval({practice_id(practice),1,1,0,0},{(1-disapprove)/2,disapprove,(1-disapprove)/2},3,prior_source(),0);
        };
        approval_prior(NormPractice::PersonalRomance,a.mind.social.public_romance_disapproval);
        approval_prior(NormPractice::Modesty,a.mind.social.public_boasting_disapproval);
        const auto risk=a.mind.risks[std::size_t(Method::TakeFood)];
        for(auto channel:{NormChannel::Detection,NormChannel::Classification,NormChannel::Reaction})a.mind.norm_memory.seed_binary(generic(NormPractice::Property,1),channel,channel==NormChannel::Detection?risk:1,2,prior_source(),0);
        a.mind.norm_memory.seed_severity(generic(NormPractice::Property,1),1,2,prior_source(),0);
        for(auto kind:{Interaction::AskPractice,Interaction::ExplainPractice,Interaction::ApprovePractice,Interaction::DisapprovePractice})a.mind.social.methods[std::size_t(kind)]={.85,.05,.6,.7,state_.next_id++};
        if(profile.mode!=NormMode::Legacy&&profile.mode!=NormMode::Shadow)a.mind.civil.desired_tier=a.mind.civil.garment_tier;
        a.cog.norm.frozen_shadow=a.mind.norm_memory;++a.mind.version;
    }
}
void World::bootstrap_norm_history(Id id,const std::vector<NormObservation>& history){
    if(state_.now)throw std::invalid_argument("historical norm bootstrap requires initial world");
    auto& a=state_.actors.at(id-1);auto candidate=a.mind.norm_memory;
    for(const auto& o:history){if(o.source.origin!=NormOrigin::Historical)throw std::invalid_argument("history must have explicit Historical provenance");candidate.apply(interpret(o,1),state_.now);}
    a.mind.norm_memory=std::move(candidate);a.cog.norm.frozen_shadow=a.mind.norm_memory;++a.mind.version;
}
void World::publish_norm_observation(Actor& a,const NormObservation& observation){
    if(state_.norm_profile.mode==NormMode::Legacy)return;
    auto o=observation;auto& n=a.cog.norm;
    if(!o.source.delivery||o.at>state_.now)throw std::invalid_argument("norm observation identity/time");
    if(std::find(n.delivered.begin(),n.delivered.end(),o.source.delivery)!=n.delivered.end()){++n.duplicates;return;}
    // Validate through an isolated candidate before enqueueing; never train here.
    NormMemory probe(a.mind.norm_memory.profile());probe.apply(o,state_.now);
    if(n.inbox.size()>=state_.norm_profile.inbox_limit){++n.dropped;if(norm_logger_)norm_logger_({state_.now,a.id,"dropped","inbox_full",o.key,o.source.delivery,o.source.known_root,o.source.revision});return;}
    n.delivered.push_back(o.source.delivery);n.inbox.push_back(o);++n.published;++a.cog.situation_version;a.cog.review_at=std::min(a.cog.review_at,state_.now);
    if(norm_logger_)norm_logger_({state_.now,a.id,"observation","queued",o.key,o.source.delivery,o.source.known_root,o.source.revision,a.mind.norm_memory.revision()});
}
void World::norm_tick(){
    if(state_.norm_profile.mode==NormMode::Legacy)return;
    for(auto& a:state_.actors){auto& n=a.cog.norm;if(!a.alive||state_.now<n.next_observe||capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep).gate==0)continue;n.next_observe=state_.now+60000;
        auto& known=a.mind.norm_context;
        if(a.employment.source&&a.employment.source!=known.employment_source){
            for(auto& goal:known.goals)if(goal.group==known.employment_group)goal.active=false;
            known.employment_source=a.employment.source;
            if(a.employment.active){
                known.employment_group=1000+a.employment.organization;
                auto existing=std::find_if(known.groups.begin(),known.groups.end(),[&](const auto& g){return g.id==known.employment_group;});
                if(existing==known.groups.end()){
                    KnownNormGroup group;group.id=group.context=known.employment_group;group.place=a.employment.workplace;group.source=a.employment.source;
                    group.members.push_back({a.id,group.source,state_.now});
                    if(a.employment.supervisor&&a.employment.supervisor!=a.id)group.members.push_back({a.employment.supervisor,group.source,state_.now});
                    known.groups.push_back(group);
                }
                known.goals.push_back({a.employment.source,a.employment.source,known.employment_group,a.employment.promise_importance,1,true});
            }
            ++known.revision;++a.mind.version;
        }
        for(const auto& p:a.cog.percepts){if(!p.recognized||state_.now-p.last>1000)continue;
            const auto& other=state_.actors.at(p.token-1);
            const auto garment=std::find_if(state_.social.objects.begin(),state_.social.objects.end(),[&](const auto& x){return x.id==other.equipment.garment&&x.kind==ObjectKind::Garment&&x.wearer==p.token&&x.holder==p.token;});
            if(garment==state_.social.objects.end())continue;
            for(const auto& group:a.mind.norm_context.groups){if(group.place!=a.place||!a.mind.norm_context.member_known(group.id,p.token))continue;
                for(auto practice:{NormPractice::ClothingCondition,NormPractice::ClothingTier}){
                    NormExposureDose exposure{p.token,practice_id(practice),group.context,state_.now/norm_day,1};
                    auto sensed=std::lower_bound(n.sensed.begin(),n.sensed.end(),exposure);
                    if(sensed!=n.sensed.end()&&*sensed==exposure)continue;
                    if(n.inbox.size()>=state_.norm_profile.inbox_limit){++n.dropped;continue;}
                    n.sensed.insert(sensed,exposure);
                    NormObservation o;o.key={practice_id(practice),group.id,group.context,0,practice==NormPractice::ClothingTier?std::uint32_t(std::lround(garment->prestige*2)):0};
                    // Known root encodes the same recognized person/day exposure across deliveries.
                    o.source.delivery=state_.next_id++;o.source.known_root=(std::uint64_t(p.token)<<32)|((state_.now/norm_day+1)<<8)|practice_id(practice);o.source.revision=1;o.source.origin=NormOrigin::Observed;o.source.independently_grounded=true;
                    o.at=state_.now;o.observed_actor=p.token;o.quality=o.confidence=o.reliability=1;o.dose=1;o.value=practice==NormPractice::ClothingCondition?garment->condition:1;o.value_known=o.applicable=true;o.channel=NormChannel::Descriptive;
                    publish_norm_observation(a,o);
                }
            }
        }
    }
}
void World::receive_norm_request(Actor& a,const InteractionObservation& observation){
    if(state_.norm_profile.mode==NormMode::Legacy||observation.kind!=Interaction::AskPractice||observation.stage!=SocialStage::Completed||observation.initiated||!observation.norm_payload.present)return;
    auto& n=a.cog.norm;n.request=observation.norm_payload;n.request_from=observation.other;n.request_delivery=observation.delivery;n.request_at=observation.at;++a.cog.situation_version;
}
void World::append_norm_topics(const Actor& a,std::vector<AttentionTopic>& input)const{
    if(state_.norm_profile.mode==NormMode::Legacy)return;
    if(a.cog.norm.request_delivery&&a.cog.norm.request.question)input.push_back({norm_reply_base+a.cog.norm.request_delivery,TopicKind::Memory,5,a.cog.norm.request_from,.7,.1,a.cog.norm.request_at,state_.now+31000,a.cog.situation_version,a.cog.norm.request_delivery,false});
    if(!a.cog.norm.inbox.empty()){const auto& o=a.cog.norm.inbox.front();input.push_back({input_base+o.source.delivery,TopicKind::Memory,5,o.source.speaker,.82,.2,o.at,state_.now+31000,a.cog.situation_version,o.source.delivery,false});}
    if(norm_effects_active(state_.norm_profile.mode))for(const auto& q:a.mind.norm_context.questions)if(q.active&&q.pending&&state_.now>=q.retry_at)input.push_back({question_base+q.id,TopicKind::Thought,5,0,q.tension,.1,q.changed,state_.now+31000,q.revision,q.basis,false});
}
bool World::start_norm_cognition(Actor& a){
    auto& c=a.cog;if(state_.norm_profile.mode==NormMode::Legacy||c.focus<input_base||c.focus>=norm_end)return false;
    if(c.focus>=norm_reply_base){c.norm.input.key=c.norm.request.key;return start_norm_context(a);}
    if(c.focus>=question_base){c.norm.question=c.focus-question_base;auto q=std::find_if(a.mind.norm_context.questions.begin(),a.mind.norm_context.questions.end(),[&](const auto& x){return x.id==c.norm.question;});if(q==a.mind.norm_context.questions.end())return false;c.norm.input.key=q->key;return start_norm_context(a);}
    auto it=std::find_if(c.norm.inbox.begin(),c.norm.inbox.end(),[&](const auto& o){return o.source.delivery==c.focus-input_base;});if(it==c.norm.inbox.end())return false;
    if(c.budget<2){finish(a);return true;}
    c.norm.input=*it;c.norm.interpreted={};c.norm.captured_revision=a.mind.norm_memory.revision();c.norm.captured_context=a.mind.norm_context.revision;
    c.context.resize(std::min<std::size_t>(1,c.context.size()));c.context.push_back({it->source.delivery,it->source.known_root,ThoughtKind::NormInterpret,5,it->observed_actor,0,it->confidence,Truth::Confirmed});c.peak_slots=std::max(c.peak_slots,int(c.context.size()));
    cognition_start(a,Operation::InterpretNormObservation);return true;
}
bool World::start_norm_context(Actor& a,Operation resume){
    if(state_.norm_profile.mode==NormMode::Legacy)return false;
    auto& c=a.cog;if(c.budget<2){finish(a);return true;}
    c.norm.captured_focus_metric=c.focus_metric;c.norm.captured_focus=c.focus;c.norm.resume_operation=std::uint8_t(resume);if(resume==Operation::SocialReply&&c.social_input.norm_payload.present)c.norm.input.key=c.social_input.norm_payload.key;c.norm.context={};c.norm.context.now=state_.now;c.norm.context.own_revision=a.mind.norm_memory.revision();c.norm.captured_revision=a.mind.norm_memory.revision();c.norm.captured_context=a.mind.norm_context.revision;
    cognition_start(a,Operation::RecallNormContext);return true;
}
void World::norm_context_for_test(Id id){auto& a=state_.actors.at(id-1);a.cog.active=true;a.cog.budget=20;a.cog.spent=0;a.cog.focus_metric=5;a.cog.snapshot=personal_view(id);start_norm_context(a);}
bool World::complete_norm_cognition(Actor& a,Operation op,Tick started){
    if(!active_norm_operation(op))return false;
    auto& c=a.cog;auto& n=c.norm;auto& memory=a.mind.norm_memory;
    if(n.captured_revision!=memory.revision()||n.captured_context!=a.mind.norm_context.revision){++c.stale_ops;finish(a);return true;}
    Thought t;t.started=started;t.metric=c.focus_metric;t.basis=n.input.source.known_root;t.origin=Origin::Inferred;t.status=Truth::Confirmed;
    if(op==Operation::InterpretNormObservation){n.interpreted=interpret(n.input,capability(a.body,a.mind.cognition,false).gate);++n.interpreted_count;t.kind=ThoughtKind::NormInterpret;t.detail="accessible_evidence_interpreted";t.value=n.interpreted.value;t.confidence=n.interpreted.confidence;emit_thought(a,t);cognition_start(a,n.interpreted.channel==NormChannel::PersonalPrinciple?Operation::ReflectPersonalPrinciple:Operation::IntegrateNormEvidence);return true;}
    if(op==Operation::IntegrateNormEvidence||op==Operation::ReflectPersonalPrinciple){
        auto& target=state_.norm_profile.mode==NormMode::FrozenLearning?n.frozen_shadow:memory;
        const auto before=target.predict(n.interpreted.key,state_.now);
        const auto result=target.apply(n.interpreted,state_.now);const auto after=target.predict(n.interpreted.key,state_.now);
        if(result==ApplyEvidence::Added||result==ApplyEvidence::Replaced){if(state_.norm_profile.mode==NormMode::FrozenLearning)++n.shadow_integrated;else{++n.integrated;++a.mind.version;}if(result==ApplyEvidence::Replaced)++n.replaced;}
        else if(result==ApplyEvidence::Duplicate)++n.duplicates;
        t.kind=op==Operation::ReflectPersonalPrinciple?ThoughtKind::NormReflect:ThoughtKind::NormIntegrate;t.value=after.prevalence;t.prior=before.prevalence;t.detail="own_source_revision_integrated";emit_thought(a,t);
        if(norm_logger_)norm_logger_({state_.now,a.id,"integration",std::to_string(unsigned(result)),n.interpreted.key,n.interpreted.source.delivery,n.interpreted.source.known_root,n.interpreted.source.revision,target.revision(),0,0,before.prevalence,after.prevalence});
        auto it=std::find_if(n.inbox.begin(),n.inbox.end(),[&](const auto& o){return o.source.delivery==n.input.source.delivery;});if(it!=n.inbox.end())n.inbox.erase(it);
        finish(a);c.review_at=state_.now+1000;return true;
    }
    if(op==Operation::RecallNormContext){
        std::vector<NormKey> keys;
        auto add_key=[&](const NormKey& key){if(keys.size()<state_.norm_profile.candidate_ids_limit&&std::find(keys.begin(),keys.end(),key)==keys.end())keys.push_back(key);};
        if((n.captured_focus>=question_base&&n.captured_focus<norm_end)||(n.resume_operation==std::uint8_t(Operation::SocialReply)&&n.input.key.practice))add_key(n.input.key);
        const bool clothing_question=std::any_of(a.mind.civil.shops.begin(),a.mind.civil.shops.end(),[&](const auto& shop){
            const auto* question=a.mind.civil.questions.find(QuestionKind::Clothing,shop.place);
            return question&&question->actionable&&!question->pending;
        });
        const auto clothing_key=[](const NormKey& key){
            return key.practice==practice_id(NormPractice::ClothingCondition)||key.practice==practice_id(NormPractice::ClothingTier);
        };
        const auto ordinary_context=[&](const KnownNormGroup& group){
            return group.place==a.place||(n.captured_focus_metric==7&&a.mind.norm_context.group_significance(group.id)>0)||
                (a.employment.in_shift(state_.now)&&group.place==a.employment.workplace);
        };
        // A ready personal clothing question survives a change of attention focus.
        // It recalls only clothing knowledge of already known, goal-linked groups.
        std::vector<const KnownNormGroup*> groups;
        for(const auto& group:a.mind.norm_context.groups)
            if(ordinary_context(group)||(clothing_question&&a.mind.norm_context.group_significance(group.id)>0))groups.push_back(&group);
        std::stable_sort(groups.begin(),groups.end(),[&](auto x,auto y){const double gx=(x->place==a.place?2:0)+a.mind.norm_context.group_significance(x->id),gy=(y->place==a.place?2:0)+a.mind.norm_context.group_significance(y->id);return gx!=gy?gx>gy:x->id<y->id;});
        if(groups.size()>state_.norm_profile.group_limit)groups.resize(state_.norm_profile.group_limit);
        for(auto g:groups)for(const auto& key:memory.keys_for_context(g->id,g->context,state_.norm_profile.candidate_ids_limit))
            if(ordinary_context(*g)||clothing_key(key))add_key(key);
        for(const auto& key:memory.keys_for_context(0,0,state_.norm_profile.candidate_ids_limit))add_key(key);
        n.candidates_seen+=keys.size();
        auto relevant=[&](const NormKey& k){double priority=(n.captured_focus>=question_base&&n.captured_focus<norm_end&&k==n.input.key)?100:0;const auto* g=a.mind.norm_context.group(k.local_group);if(g)priority+=(g->place==a.place?2:0)+a.mind.norm_context.group_significance(g->id);if(!k.local_group)priority+=.1;if(k.practice==practice_id(NormPractice::ClothingCondition)||k.practice==practice_id(NormPractice::ClothingTier))priority+=(n.captured_focus_metric==7||clothing_question)?2:0;if(k.practice==practice_id(NormPractice::PersonalRomance))priority+=n.captured_focus_metric==6?3:0;if(k.practice==practice_id(NormPractice::Property))priority+=n.captured_focus_metric==0?3:0;if(k.practice==n.input.key.practice)priority+=.5;
            if(k.practice==practice_id(NormPractice::Work)&&a.employment.active&&(a.place==a.employment.workplace||a.employment.in_shift(state_.now)))priority+=4;
            if(n.resume_operation==std::uint8_t(Operation::SocialReply)&&c.social_input.kind==Interaction::AskMoney&&k.practice==practice_id(NormPractice::Help))priority+=100;
            if(a.conversation.id||n.resume_operation==std::uint8_t(Operation::SocialReply)){
                if(k.practice==practice_id(NormPractice::Privacy)&&k.variant==1)priority+=3;
                if(k.practice==practice_id(NormPractice::Promise))priority+=3;
                if(k.practice==practice_id(NormPractice::Help))priority+=3;
            }
            return priority;};
        std::stable_sort(keys.begin(),keys.end(),[&](const auto& x,const auto& y){auto px=relevant(x),py=relevant(y);return px!=py?px>py:x<y;});
        n.candidates_displaced+=keys.size()>state_.norm_profile.deep_norm_limit?keys.size()-state_.norm_profile.deep_norm_limit:0;
        const auto slots=std::max(0,capability(a.body,a.mind.cognition,false).context-1);
        n.context.count=std::uint8_t(std::min({std::size_t(state_.norm_profile.deep_norm_limit),keys.size(),std::size_t(slots)}));
        for(unsigned i=0;i<n.context.count;++i){n.context.selected_keys[i]=keys[i];n.context.goal_relevance[i]=a.mind.norm_context.group_significance(keys[i].local_group);}
        n.considered={};n.considered.mode=state_.norm_profile.mode;n.plan_context={};t.kind=ThoughtKind::NormRecall;t.detail="bounded_own_context_keys";t.value=n.context.count;emit_thought(a,t);cognition_start(a,Operation::CompareNormAlternatives);return true;
    }
    n.considered.count=n.context.count;
    for(unsigned i=0;i<n.context.count;++i){n.considered.considered[i]=memory.predict(n.context.selected_keys[i],state_.now);++n.deep_evaluations;}
    c.context.resize(std::min<std::size_t>(1,c.context.size()));
    for(unsigned i=0;i<n.context.count;++i){const auto key=n.context.selected_keys[i];const auto& prediction=n.considered.considered[i];
        c.context.push_back({key.practice,prediction.own_revision,ThoughtKind::NormCompare,c.focus_metric,0,prediction.prevalence,prediction.coverage,prediction.descriptive_known?Truth::Confirmed:Truth::Unknown});c.peak_slots=std::max(c.peak_slots,int(c.context.size()));
        if(key==n.request.key&&n.request_delivery&&n.request.question&&n.outbound.size()<state_.norm_profile.new_option_limit&&std::none_of(n.outbound.begin(),n.outbound.end(),[&](const auto& p){return p.explanation&&p.key==key&&p.subject==n.request_from;})){
            for(auto channel:{NormChannel::Approval,NormChannel::Descriptive,NormChannel::PersonalPrinciple})if(auto evidence=memory.recall_evidence(key,channel,state_.now)){NormPayload reply;reply.present=reply.explanation=true;reply.key=key;reply.subject=n.request_from;reply.evidence=*evidence;reply.evidence.accepted_argument=false;n.outbound.push_back(reply);break;}
        }
        auto& effect=n.plan_context.effects[i];effect.group_significance=n.context.goal_relevance[i];effect.applicability=1;effect.approval_value=effect.disapproval_value=1;
        for(const auto& goal:a.mind.norm_context.goals)if(goal.group==key.local_group&&goal.active&&goal.source){effect.goal=goal.goal;break;}
        const auto* group=a.mind.norm_context.group(key.local_group);if(group)for(const auto& p:c.percepts)if(p.recognized&&state_.now-p.last<=1000&&a.mind.norm_context.member_known(key.local_group,p.token)){effect.audience=p.token;effect.audience_known=true;break;}
        const double discrepancy=mismatch(a,key,n.input);
        // Action applicability is distinct from a mismatch that motivates review.
        effect.applicability=1;
        if(n.input.channel==NormChannel::Descriptive&&key==n.input.key&&n.input.observed_actor&&n.input.observed_actor!=a.id&&(prediction.approval_known||prediction.personal_principle_known)&&(effect.group_significance>0||prediction.personal_resistance>0)&&n.reaction_considered!=n.input.source.known_root&&n.outbound.size()<state_.norm_profile.new_option_limit){
            if(prediction.personal_resistance>0||prediction.approve!=prediction.disapprove){NormPayload reaction;reaction.present=true;reaction.key=key;reaction.subject=n.input.observed_actor;reaction.evidence.key=key;reaction.evidence.source=n.input.source;reaction.evidence.channel=NormChannel::Approval;reaction.evidence.approval=prediction.personal_resistance>0?ApprovalValue::Disapprove:prediction.approve>prediction.disapprove?ApprovalValue::Approve:ApprovalValue::Disapprove;reaction.evidence.quality=reaction.evidence.confidence=reaction.evidence.reliability=reaction.evidence.dose=1;reaction.evidence.applicable=reaction.evidence.value_known=true;reaction.evidence.at=state_.now;n.outbound.push_back(reaction);n.reaction_considered=n.input.source.known_root;}
        }
        const double expected_loss=(prediction.approval_known?prediction.disapprove:0)*effect.disapproval_value;
        const double tension=effect.group_significance*discrepancy*prediction.approval_coverage*expected_loss;
        bool strongest=true;
        for(unsigned j=0;j<n.considered.count;++j){if(j==i)continue;const auto& other=n.considered.considered[j];
            if(other.key.practice!=key.practice||other.key.local_group!=key.local_group||other.key.context!=key.context)continue;
            const double candidate=effect.group_significance*mismatch(a,other.key,n.input)*other.approval_coverage*(other.approval_known?other.disapprove:0)*effect.disapproval_value;
            if(candidate>tension||(candidate==tension&&other.key<key))strongest=false;
        }
        if(norm_effects_active(state_.norm_profile.mode)&&effect.goal&&strongest){auto& questions=a.mind.norm_context.questions;auto q=std::find_if(questions.begin(),questions.end(),[&](const auto& x){return x.key.practice==key.practice&&x.key.local_group==key.local_group&&x.key.context==key.context&&x.goal==effect.goal;});
            if(q==questions.end()&&tension>=state_.norm_profile.motive_on){NormQuestion fresh;fresh.id=a.mind.norm_context.next_question++;fresh.goal=effect.goal;fresh.key=key;questions.push_back(fresh);q=std::prev(questions.end());++a.mind.norm_context.revision;}
            if(q!=questions.end()){const auto prior=std::tuple{q->revision,q->basis,q->active,q->pending,q->waiting};const auto basis=question_basis(a,prediction,effect.group_significance);const bool changed=q->basis!=basis;q->tension=tension;q->active=q->active?tension>=state_.norm_profile.motive_off:tension>=state_.norm_profile.motive_on;if(changed){q->waiting=false;q->revision=prediction.own_revision;q->basis=basis;q->key=key;q->changed=state_.now;q->pending=q->active;if(q->active){++a.mind.norm_context.questions_woken;
                    if(key.practice==practice_id(NormPractice::ClothingCondition)||key.practice==practice_id(NormPractice::ClothingTier))for(const auto& shop:a.mind.civil.shops)a.mind.civil.questions.wake(QuestionKind::Clothing,shop.place,(15ull<<48)|(q->basis&0xffffffffffffull),state_.now);
                }}if(q->active&&!q->waiting){n.plan_context.question_active=true;n.plan_context.question_key=key;n.plan_context.question_id=q->id;}if(q->id==n.question){q->pending=false;q->reviewed_revision=q->revision;}if(prior!=std::tuple{q->revision,q->basis,q->active,q->pending,q->waiting})++a.mind.norm_context.revision;}
        }
    }
    n.prepared_place=a.place;n.prepared_at=state_.now;n.prepared_revision=memory.revision();n.prepared_context=a.mind.norm_context.revision;
    n.captured_context=a.mind.norm_context.revision;
    t.kind=ThoughtKind::NormCompare;t.detail="at_most_four_known_norms";t.value=n.considered.count;emit_thought(a,t);populate_norm_view(a,c.snapshot);
    if(c.budget>0)cognition_start(a,Operation(n.resume_operation));else finish(a);return true;
}
void World::populate_norm_view(const Actor& a,PersonalView& v)const{
    v.norms_view=a.cog.norm.considered;v.norms_view.mode=state_.norm_profile.mode;v.norm_context=a.cog.norm.plan_context;
    const auto& n=a.cog.norm;
    bool valid=!n.prepared_place||(n.prepared_place==a.place&&n.prepared_revision==a.mind.norm_memory.revision()&&n.prepared_context==a.mind.norm_context.revision&&state_.now-n.prepared_at<=30000);
    if(valid&&n.prepared_place)for(unsigned i=0;i<v.norms_view.count;++i){const auto& e=v.norm_context.effects[i];if(e.audience_known&&std::none_of(a.cog.percepts.begin(),a.cog.percepts.end(),[&](const auto& p){return p.token==e.audience&&p.recognized&&state_.now-p.last<=1000;}))valid=false;}
    if(!valid){v.norms_view.count=0;v.norm_context={};}
    v.social.norms_view=v.norms_view;v.social.norm_context=v.norm_context;v.social.norm_options.clear();if(norm_effects_active(state_.norm_profile.mode))v.social.norm_options=a.cog.norm.outbound;
    if(norm_effects_active(v.norms_view.mode)&&v.norm_context.question_active){NormPayload p;p.present=p.question=true;p.key=v.norm_context.question_key;p.evidence.key=p.key;p.subject=a.id;if(v.social.norm_options.size()<state_.norm_profile.new_option_limit)v.social.norm_options.push_back(p);}
}
void World::validate_norm_runtime()const{
    state_.norm_profile.validate();
    if(unsigned(state_.civil.profile.budget_policy)>1||!unit_value(state_.civil.profile.risk_importance))throw std::runtime_error("invalid civil policy");
    for(const auto& a:state_.actors){a.mind.norm_memory.validate(state_.now);a.mind.norm_context.validate(state_.now);const auto& n=a.cog.norm;
        for(const auto* decision:{&a.cog.current,&a.cog.best}){
            validate_social_evaluation(decision->social_evaluation);
            DecisionLedger checked;for(const auto& term:decision->norm_ledger)checked.insert_unique(term);
        }
        if(n.resume_operation&&n.resume_operation!=std::uint8_t(Operation::Recall)&&n.resume_operation!=std::uint8_t(Operation::SocialReply))throw std::runtime_error("invalid norm continuation");
        if(n.inbox.size()>state_.norm_profile.inbox_limit||n.context.count>4||n.considered.count>4)throw std::runtime_error("norm runtime capacity");
        for(const auto& o:n.inbox){if(o.at>state_.now||!o.source.delivery)throw std::runtime_error("invalid norm inbox");NormMemory probe(a.mind.norm_memory.profile());probe.apply(o,state_.now);}
        if(active_norm_operation(a.cog.operation)&&a.cog.operation!=Operation::RecallNormContext&&a.cog.operation!=Operation::CompareNormAlternatives){NormMemory probe(a.mind.norm_memory.profile());probe.apply(n.input,state_.now);}
    }
}
std::string World::norm_report_json()const{
    std::ostringstream out;out.precision(17);
    out<<"{\"version\":\"0.14.0-norm01\",\"mode\":\""<<norm_mode_name(state_.norm_profile.mode)<<"\",\"actors\":[";
    auto key_json=[&](const NormKey& k){out<<'['<<k.practice<<','<<k.local_group<<','<<k.context<<','<<k.actor_role<<','<<k.variant<<']';};
    bool comma=false;
    for(const auto& a:state_.actors){
        if(comma)out<<',';comma=true;const auto& n=a.cog.norm;
        out<<"{\"id\":"<<a.id<<",\"records\":"<<a.mind.norm_memory.size()<<",\"revision\":"<<a.mind.norm_memory.revision()
           <<",\"published\":"<<n.published<<",\"interpreted\":"<<n.interpreted_count<<",\"integrated\":"<<n.integrated
           <<",\"duplicates\":"<<n.duplicates<<",\"dropped\":"<<n.dropped<<",\"deferred\":"<<n.inbox.size()
           <<",\"replaced\":"<<n.replaced<<",\"shadow_integrated\":"<<n.shadow_integrated
           <<",\"deep_evaluations\":"<<n.deep_evaluations<<",\"candidates_seen\":"<<n.candidates_seen
           <<",\"candidates_displaced\":"<<n.candidates_displaced<<",\"recognized_exposures\":"<<n.sensed.size()
           <<",\"questions\":"<<a.mind.norm_context.questions.size()<<",\"questions_woken\":"<<a.mind.norm_context.questions_woken
           <<",\"beliefs\":[";
        bool first=true;
        for(const auto& r:a.mind.norm_memory.records()){
            if(!first)out<<',';first=false;const auto p=a.mind.norm_memory.predict(r.key,state_.now);
            out<<"{\"key\":";key_json(r.key);
            out<<",\"revision\":"<<r.revision<<",\"descriptive_known\":"<<(p.descriptive_known?"true":"false")
               <<",\"approval_known\":"<<(p.approval_known?"true":"false")<<",\"sanction_known\":"<<(p.sanction_known?"true":"false")
               <<",\"prevalence\":"<<p.prevalence<<",\"coverage\":"<<p.coverage<<",\"approval_coverage\":"<<p.approval_coverage
               <<",\"approval\":["<<p.approve<<','<<p.disapprove<<','<<p.indifferent<<"],\"conditional_known\":["
               <<int(p.seen_known)<<','<<int(p.classified_known)<<','<<int(p.reacted_known)<<','<<int(p.severity_known)
               <<"],\"conditional\":["<<p.seen<<','<<p.classified<<','<<p.reacted<<','<<p.severity
               <<"],\"personal_known\":"<<(p.personal_principle_known?"true":"false")<<",\"personal_weight\":"<<p.principle_weight
               <<",\"personal_resistance\":"<<p.personal_resistance<<",\"prior_origins\":[";
            for(unsigned i=0;i<6;++i){if(i)out<<',';out<<(r.prior_source_known[i]?int(r.prior_sources[i].origin):-1);}
            out<<"],\"sources\":[";bool source_first=true;
            for(const auto& e:r.sources){if(!source_first)out<<',';source_first=false;
                out<<"{\"channel\":"<<unsigned(e.channel)<<",\"delivery\":"<<e.source.delivery<<",\"root\":"<<e.source.known_root
                   <<",\"revision\":"<<e.source.revision<<",\"origin\":"<<unsigned(e.source.origin)<<",\"at\":"<<e.original_at
                   <<",\"dose\":"<<e.dose<<",\"pooled\":"<<(e.pooled?"true":"false")<<'}';}
            out<<"],\"personal_transforms\":"<<r.personal.transforms.size()<<'}';
        }
        out<<"],\"question_state\":[";first=true;
        for(const auto& q:a.mind.norm_context.questions){if(!first)out<<',';first=false;out<<"{\"id\":"<<q.id<<",\"key\":";key_json(q.key);
            out<<",\"goal\":"<<q.goal<<",\"revision\":"<<q.revision<<",\"tension\":"<<q.tension<<",\"active\":"<<(q.active?"true":"false")
               <<",\"pending\":"<<(q.pending?"true":"false")<<",\"waiting\":"<<(q.waiting?"true":"false")<<'}';}
        out<<"]}";
    }
    out<<"]}";return out.str();
}
}
