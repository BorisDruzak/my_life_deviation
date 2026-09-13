#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace life {
namespace {
Place& venue(State& s,Id id){for(auto& p:s.places)if(p.id==id)return p;throw std::logic_error("community place");}
bool in_talk(const Actor& a){return a.alive&&a.action.phase==Phase::Running&&a.action.method==Method::Talk;}
}
void World::configure_community(){
    if(state_.now!=0||state_.community.enabled)throw std::invalid_argument("community configuration only once at t0");
    auto& s=state_;s.community.enabled=true;s.balance="community-0.10-test2";
    venue(s,3).fee=12;venue(s,5).job_level=0;venue(s,5).wage=job_catalogue()[0].hourly;
    venue(s,2).recreation=0;venue(s,2).fee=0;
    venue(s,6).recreation=1;venue(s,6).fee=2;venue(s,6).services|=service(Method::Study)|service(Method::Talk);
    for(int level=1;level<4;++level){Place p;p.id=Id(6+level);p.services=service(Method::Work)|service(Method::Eat);p.job_level=level;p.wage=job_catalogue()[level].hourly;s.places.push_back(p);s.edges.push_back({1,p.id,120u+30u*unsigned(level),true});}
    for(int variant=2;variant<6;++variant){Place p;p.id=Id(8+variant);p.services=service(Method::Leisure)|service(Method::Talk)|service(Method::Eat);p.recreation=variant;p.fee=recreation_catalogue()[variant].fee;s.places.push_back(p);s.edges.push_back({1,p.id,60u+30u*unsigned(variant),true});}
    for(auto& p:s.places)if(p.id<=13)p.services|=service(Method::Eat);
    for(auto& a:s.actors){
        auto& cm=a.mind.social.community;cm.enabled=true;
        a.appearance={150+45*s.random.uniform("appearance",a.id,0),.2+.65*s.random.uniform("appearance",a.id,1),std::uint8_t(s.random.integer("appearance",a.id,2,4))};
        cm.appearance=a.appearance;
        cm.preference.preferred_height=150+45*s.random.uniform("preference",a.id,0);
        cm.preference.preferred_build=s.random.uniform("preference",a.id,1);
        for(int h=0;h<4;++h)cm.preference.hair_liking[h]=.15+.8*s.random.uniform("hair-preference",a.id,h);
        a.social_traits.preference=cm.preference;
        cm.confidentiality=.2+.8*s.random.uniform("privacy-norm",a.id,0);
        cm.curiosity=.2+.7*s.random.uniform("curiosity",a.id,0);
        cm.ambition=.2+.7*s.random.uniform("ambition",a.id,0);
        cm.stranger_romance=.3+.65*s.random.uniform("stranger-norm",a.id,0);
        cm.kin_romance=.65+.35*s.random.uniform("kin-norm",a.id,0);
        // Cultural weights are seeded individual beliefs, not a universal verdict.
        a.mind.social.norms[std::size_t(SocialNorm::PersonalRomance)]=s.random.uniform("cultural-norm",a.id,0);
        a.mind.social.methods[std::size_t(Interaction::ShareNews)]={.9,.35,.8,.8,s.next_id++};
        a.mind.social.methods[std::size_t(Interaction::DiscussTopic)]={.9,.4,.8,.8,s.next_id++};
        a.social_traits.pleasure[std::size_t(Interaction::ShareNews)]=.25;
        a.social_traits.pleasure[std::size_t(Interaction::DiscussTopic)]=.35;
        a.mind.known[std::size_t(Method::Study)]=true;
        // Every resident knows basic work, not every professional qualification.
        for(unsigned level=0;level<4;++level){const Id id=job_catalogue()[level].skill;
            a.mind.knowledge.mastery.emplace_back(id,level==0?.9:level==1?.2+.35*s.random.uniform("training",a.id,level):0);}
        a.mind.priors[std::size_t(Method::Study)][7]=.4;
        a.mind.norms[std::size_t(Method::TakeFood)]=.7+.3*s.random.uniform("property",a.id,0);
        a.economy.wage_known=job_catalogue()[0].hourly;
        for(unsigned i=0;i<6;++i)a.economy.interests[i]=.55+.45*s.random.uniform("leisure-preferences",a.id,i);
        s.ledger.initial_money+=120-a.money;a.money=120;a.mind.believed_money=a.money;
        a.mind.places.clear();a.mind.map.clear();
        for(const auto& p:s.places)if(p.id<=13||p.id==a.home){KnownPlace k;k.id=p.id;k.services=p.services;k.price=p.fee;k.job_level=p.job_level;k.recreation=p.recreation;k.wage=p.wage;a.mind.places.push_back(k);}
        for(const auto& e:s.edges)if((e.a<=13&&e.b<=13)||e.a==a.home||e.b==a.home)a.mind.map.push_back({e.a,e.b,e.seconds,Truth::Confirmed});
        Information own;own.id=s.next_id++;own.kind=NewsKind::Employment;own.subject=a.id;own.other=5;own.material=.2;own.origin=NewsOrigin::Initial;own.cited_source=a.id;own.importance=.6;
        cm.receive(own,0,s.next_id++,0);
        Information hobby;hobby.id=s.next_id++;hobby.kind=NewsKind::Visit;hobby.subject=a.id;hobby.other=0;hobby.location=2;hobby.origin=NewsOrigin::Initial;hobby.importance=.3;
        cm.receive(hobby,0,s.next_id++,0);
        Information private_note;private_note.id=s.next_id++;private_note.kind=NewsKind::Opinion;private_note.subject=a.id;private_note.valence=-.2;private_note.disclosure=Disclosure::Personal;private_note.sensitivity=.8;private_note.importance=.7;private_note.origin=NewsOrigin::Initial;
        cm.receive(private_note,0,s.next_id++,0);
    }
    for(auto& a:s.actors)for(auto& p:a.mind.social.people){const auto& b=s.actors.at(p.id-1);p.appearance=b.appearance;p.appearance_known=true;const auto* r=a.mind.relation(p.id);p.known_kin=r&&r->role==1;}
    validate();
}
void World::populate_economy(const Actor& a,PersonalView& v)const{
    if(!state_.community.enabled)return;
    auto& e=v.economy;e.enabled=true;e.reserve=250;
    const int weekday=int((state_.now/86400000)%7);
    const double hour=double(state_.now%86400000)/3600000.;
    const double until_work=weekday<5?(hour<17?0:(weekday==4?3:1)):(weekday==5?2:1);
    // Known open hours and living costs, not the actual future world balance.
    const double essentials=54*(1+until_work);
    e.protected_cash=essentials;e.study_budget=std::max(0.,v.money-essentials);
    e.work_today=a.economy.work_today;e.study_today=a.economy.study_today;e.job=a.economy.job;e.interests=a.economy.interests;
    for(unsigned k=0;k<4;++k)e.mastery[k]=a.mind.knowledge.get(job_catalogue()[k].skill);
    // Own remembered wallet, explicit reserve goal, not someone else's balance.
    v.need[7]=unit((e.reserve-v.money)/e.reserve);
}
void World::community_account(Actor& a,Tick elapsed){
    if(!state_.community.enabled)return;
    TimeUse category=TimeUse::Other;
    if(a.action.blocked)category=TimeUse::Other;
    else if(a.action.phase==Phase::Travel)category=TimeUse::Travel;
    else if(a.action.phase==Phase::Asleep)category=TimeUse::Sleep;
    else if(a.action.phase==Phase::Idle||a.action.phase==Phase::Waiting)category=TimeUse::Idle;
    else switch(a.action.method){
      case Method::Idle:category=TimeUse::Idle;break;
      case Method::Eat:category=TimeUse::Eating;break;
      case Method::Drink:category=TimeUse::Drinking;break;
      case Method::Rest:category=TimeUse::Rest;break;
      case Method::Leisure:category=TimeUse::Leisure;break;
      case Method::Talk:category=TimeUse::Social;break;
      case Method::Work:category=TimeUse::Work;break;
      case Method::Study:category=TimeUse::Study;break;
      case Method::PrivateIntimacy:category=TimeUse::Private;break;
      default:break;
    }
    if(state_.life.enabled&&a.conversation.id&&(category==TimeUse::Idle||category==TimeUse::Other))category=TimeUse::Social;
    a.economy.time_ms[std::size_t(category)]+=double(elapsed);
    if(category==TimeUse::Work)a.economy.work_today+=double(elapsed)/3600000.;
    if(category==TimeUse::Study)a.economy.study_today+=double(elapsed)/3600000.;
}
void World::community_tick(){
    auto& s=state_;if(!s.community.enabled)return;
    if(s.now>=s.community.next_day){
        for(auto& a:s.actors){a.economy.day_index=s.now/86400000;a.economy.work_today=a.economy.study_today=0;
            if(!a.alive)continue;
            const double rent=std::min(a.money,18.);a.money-=rent;venue(s,5).account+=rent;a.economy.rent_paid+=rent;a.economy.rent_debt+=18-rent;
            a.mind.believed_money=a.money;++a.mind.version;
        }
        s.community.next_day+=86400000;
    }
    if(s.now>=s.community.next_forget){for(auto& a:s.actors){auto& m=a.mind.social.community;auto version=m.revision;m.forget(s.now);if(version!=m.revision)++a.mind.version;}s.community.next_forget+=3600000;}
}
void World::community_publish(Actor& actor,Information fact){
    auto& s=state_;if(!s.community.enabled)return;
    if(!fact.id)fact.id=s.next_id++;
    fact.subject=actor.id;fact.occurred_at=s.now;fact.location=actor.place;fact.origin=NewsOrigin::Observed;fact.cited_source=actor.id;
    actor.mind.social.community.receive(fact,0,s.next_id++,s.now);++actor.mind.version;
    ++s.community.observed_events;++s.community.observed_projections;
    // Each witness needs its own recognized, presently visible representation.
    for(auto& other:s.actors){if(other.id==actor.id||!other.alive||other.place!=actor.place||other.action.phase==Phase::Travel)continue;
        if(std::none_of(other.cog.percepts.begin(),other.cog.percepts.end(),[&](const auto& p){return p.token==actor.id&&p.recognized&&p.last>=s.now-1000;}))continue;
        if(capability(other.body,other.mind.cognition,other.action.phase==Phase::Asleep).gate==0)continue;
        if(fact.disclosure!=Disclosure::Open)continue;
        InteractionObservation o;o.event=fact.id;o.kind=Interaction::ShareNews;o.stage=SocialStage::Completed;o.other=actor.id;o.at=s.now;
        o.information=fact;o.information.job_terms_present=false;o.information.job_terms=JobKnowledge{};o.information.cited_source=other.id;o.information_present=true;
        // This is a projected direct observation, distinguished from spoken claims by parent=0.
        social_deliver(other,o);++s.community.observed_projections;
    }
}
void World::community_finish(Actor& a,Method method){
    if(!state_.community.enabled)return;
    auto& s=state_;auto& p=venue(s,a.place);Information f;bool publish=true;
    if(method==Method::Work){
        const unsigned tier=unsigned(std::max(0,p.job_level));
        if(!s.life.enabled&&tier>a.economy.job){a.economy.job=tier;++a.economy.promotions;}
        a.economy.wage_known=s.life.enabled?a.employment.hourly*a.employment.known_multiplier:p.wage;
        if(s.adaptive_life&&a.employment.active){f.job_terms_present=true;auto& k=f.job_terms;k.organization=a.employment.organization;k.place=a.employment.workplace;k.hourly=a.economy.wage_known;k.known=LocationKnown|WageKnown;}
        f.kind=NewsKind::Employment;f.other=p.id;f.material=.2+.2*tier;f.importance=.5;
    }else if(method==Method::Study){
        unsigned target=1;while(target<4&&a.mind.knowledge.get(job_catalogue()[target].skill)>=.6)++target;
        if(target<4){for(auto& [id,g]:a.mind.knowledge.mastery)if(id==job_catalogue()[target].skill){const double alpha=-std::expm1(-(s.life.enabled?.02:.5)*a.mind.cognition.learnability*.5);g=unit(g+alpha*(1-g));}}
        if(s.adaptive_life){auto& career=a.mind.career;for(auto& q:career.questions)if(q.stage==CareerStage::Study){const auto* k=career.find(q.organization);if(k&&a.mind.knowledge.get(k->required_skill)>=.6){q.stage=CareerStage::Review;q.next_review=s.now;q.basis=s.next_id++;++a.mind.version;}}}
        ++a.economy.study_sessions;f.kind=NewsKind::Skill;f.other=target<4?job_catalogue()[target].skill:704;f.importance=.5;
    }else if(method==Method::Leisure){f.kind=NewsKind::Visit;f.other=Id(std::max(0,p.recreation));f.importance=.25;}
    else if(method==Method::AcquireFood){f.kind=NewsKind::Purchase;f.other=3;f.material=.25;f.importance=.15;}
    else if(method==Method::Talk){f.kind=NewsKind::Visit;f.other=a.action.partner;f.importance=.35;}
    else if(method==Method::TakeFood){f.kind=NewsKind::Taking;f.other=3;f.importance=.8;f.sensitivity=.7; /* observation is NOT a proven crime */}
    else publish=false;
    if(publish)community_publish(a,f);
}
void World::community_begin_utterance(SocialEvent& e){
    if(!state_.community.enabled||e.kind!=Interaction::ShareNews)return;
    const auto& speaker=state_.actors[e.initiator-1];
    const auto info=speaker.mind.social.community.recall(e.object,state_.now);
    if(!info)return;
    e.information=*info;e.information_present=true;e.information.origin=NewsOrigin::Reported;
    if(info->origin==NewsOrigin::Observed&&info->cited_source)e.information.cited_source=speaker.id;
    // The initial recipient is separately delivered below. The shared floor admits
    // only already participating, attentive speakers/listeners, at most six total.
    for(const auto& a:state_.actors){if(a.id==e.initiator||a.id==e.receiver||!in_talk(a)||a.place!=speaker.place||a.mind.social.active_event)continue;
        if(e.listeners.size()>=4)break;
        if(std::none_of(a.cog.percepts.begin(),a.cog.percepts.end(),[&](const auto& p){return p.token==speaker.id&&p.recognized&&p.last>=state_.now-1000;}))continue;
        e.listeners.push_back(a.id);e.listener_parents.push_back(a.action.id);
    }
}
void World::community_complete_utterance(const SocialEvent& e){
    auto& s=state_;if(!s.community.enabled||(e.kind!=Interaction::ShareNews&&e.kind!=Interaction::DiscussTopic))return;
    ++s.community.utterances;s.community.speech_seconds+=double(e.ends-e.answered_at)/1000.;
    if(e.kind==Interaction::DiscussTopic)return;
    if(!e.information_present)return;
    ++s.community.topics[std::size_t(e.information.kind)];
    bool third=e.information.subject!=e.initiator&&e.information.subject!=e.receiver;
    for(Id who:e.listeners)if(who==e.information.subject)third=false;
    if(third)s.community.third_party_seconds+=double(e.ends-e.answered_at)/1000.;
    if(e.information.disclosure!=Disclosure::Open)++s.community.disclosures;
    auto& memory=s.actors[e.initiator-1].mind.social.community;
    memory.shared.push_back({e.information.id,e.receiver,s.now});memory.rehearse(e.information.id,s.now);
    ++s.community.deliveries; // direct recipient; receipt is interpreted later
    for(std::size_t i=0;i<e.listeners.size();++i){auto& a=s.actors[e.listeners[i]-1];
        if(!in_talk(a)||a.place!=s.actors[e.initiator-1].place||a.action.id!=e.listener_parents[i]){++s.community.missed;continue;}
        InteractionObservation o;o.event=e.id;o.parent=e.parent;o.other=e.initiator;o.kind=Interaction::ShareNews;o.stage=SocialStage::Completed;o.at=s.now;
        o.information=e.information;o.information_present=true;social_deliver(a,o);
        memory.shared.push_back({e.information.id,a.id,s.now});++s.community.deliveries;++s.community.group_deliveries;
    }
}
std::string World::community_report_json()const{
    const auto& s=state_;std::ostringstream out;out.precision(12);
    static const char* names[]={"idle","travel","eating","drinking","sleep","rest","leisure","social","private","work","study","other"};
    out<<"{\"version\":\"0.10.0\",\"enabled\":"<<(s.community.enabled?"true":"false")<<",\"ms\":"<<s.now<<",\"utterances\":"<<s.community.utterances<<",\"deliveries\":"<<s.community.deliveries<<",\"group_deliveries\":"<<s.community.group_deliveries<<",\"speech_seconds\":"<<s.community.speech_seconds<<",\"third_party_seconds\":"<<s.community.third_party_seconds<<",\"disclosures\":"<<s.community.disclosures<<",\"paid_leisure\":"<<s.community.paid_leisure<<",\"actors\":[";
    bool first=true;for(const auto& a:s.actors){if(!first)out<<',';first=false;const auto& m=a.mind.social.community;const auto& e=a.economy;
        out<<"{\"id\":"<<a.id<<",\"alive\":"<<(a.alive?"true":"false")<<",\"job\":"<<e.job<<",\"money\":"<<a.money<<",\"earned\":"<<e.earned<<",\"food_spent\":"<<e.food_spent<<",\"leisure_spent\":"<<e.leisure_spent<<",\"study_spent\":"<<e.study_spent<<",\"rent_paid\":"<<e.rent_paid<<",\"rent_debt\":"<<e.rent_debt<<",\"promotions\":"<<e.promotions<<",\"study_sessions\":"<<e.study_sessions<<",\"memory_count\":"<<m.entries.size()<<",\"received\":"<<m.received<<",\"forgotten\":"<<m.forgotten<<",\"height\":"<<a.appearance.height_cm<<",\"time_ms\":{";
        for(std::size_t i=0;i<time_use_count;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":"<<e.time_ms[i];}out<<"},\"mastery\":[";
        for(unsigned i=0;i<4;++i){if(i)out<<',';out<<a.mind.knowledge.get(job_catalogue()[i].skill);}out<<"],\"opinions\":[";
        std::set<Id> subjects;for(const auto& entry:m.entries)subjects.insert(entry.content.subject);bool one=true;
        for(Id who:subjects){auto r=m.opinion(who,s.now);if(!r.material&&!r.helpfulness&&!r.honesty)continue;if(!one)out<<',';one=false;
            out<<"{\"subject\":"<<who<<",\"material\":";if(r.material)out<<*r.material;else out<<"null";out<<",\"helpfulness\":";if(r.helpfulness)out<<*r.helpfulness;else out<<"null";out<<",\"honesty\":";if(r.honesty)out<<*r.honesty;else out<<"null";out<<",\"sources\":"<<r.sources<<'}';}
        out<<"],\"memories\":[";one=true;for(const auto& entry:m.entries){auto f=m.recall(entry.content.id,s.now);if(!f)continue;if(!one)out<<',';one=false;out<<"{\"id\":"<<f->id<<",\"kind\":"<<unsigned(f->kind)<<",\"subject\":"<<f->subject<<",\"speaker\":"<<entry.speaker<<",\"origin\":"<<unsigned(f->origin)<<",\"cited_source\":"<<f->cited_source<<",\"location\":"<<f->location<<",\"occurred_at\":"<<f->occurred_at<<",\"encoded_at\":"<<entry.encoded_at<<",\"strength\":"<<entry.gist.at(s.now)<<",\"disclosure\":"<<unsigned(f->disclosure)<<'}';}out<<"]}";
    }
    out<<"]}";return out.str();
}
} // namespace life
