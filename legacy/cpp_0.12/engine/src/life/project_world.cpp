#include "life/world.hpp"
#include "valuation.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace life {
namespace {
constexpr Tick H=3600000,D=86400000;
const KnownProcedure* recipe(const LifePlanView& v,std::uint32_t id){
    auto it=std::find_if(v.procedures.begin(),v.procedures.end(),[&](const auto& r){return r.id==id;});
    return it==v.procedures.end()?nullptr:&*it;
}
const RelationshipView* person(const LifePlanView& v,Id id){
    auto it=std::find_if(v.people.begin(),v.people.end(),[&](const auto& r){return r.person==id;});
    return it==v.people.end()?nullptr:&*it;
}
StepKind current_step(const ProjectMemory& m,const PersonalProject& p){return m.known(p.procedure)->steps.at(p.step).kind;}
void outcome(ProjectMemory& m,PersonalProject& p,StepOutcome value,std::uint64_t event,Tick now){m.observe(p.id,value,event,now);}
}
bool Mind::experience_contact(Id other,std::uint64_t event,Tick now,double seconds,double pleasure,Id place,bool introduced){
    if(!other||!event||now<0||seconds<0||!std::isfinite(seconds))throw std::invalid_argument("contact identity/time");
    require_range(pleasure,-1,1);
    auto* r=relation(other);
    if(!r){
        if(learning.address_slots+learning.weights.size()>=512)return false;
        Relation value;value.person=other;value.origin=event;
        auto it=std::lower_bound(relations.begin(),relations.end(),other,[](const auto& x,Id id){return x.person<id;});
        r=&*relations.insert(it,value);++learning.address_slots;
    }
    if(std::binary_search(r->contact_episodes.begin(),r->contact_episodes.end(),event))return false;
    r->contact_episodes.insert(std::lower_bound(r->contact_episodes.begin(),r->contact_episodes.end(),event),event);
    r->last_contact=now;r->last_seen=now;r->last_known_place=place;r->introduced|=introduced;
    const double dose=std::min(1.,seconds/300.);
    r->familiarity=1-(1-r->familiarity)*std::exp(-.7*dose/5);
    r->contact_pleasure.observe(pleasure,cognition.learnability,.8,dose,cognition.base[3],true);
    // Familiarity can grow after an unpleasant meeting. Care is a separate, slow channel.
    const double rate=.03*cognition.plasticity*.7*dose*std::max(0.,pleasure);
    r->care=unit(r->care+(1-std::exp(-rate))*(1-r->care));
    ++version;return true;
}
void World::configure_life_projects(bool satiation){
    if(state_.now!=0||state_.life.enabled)throw std::invalid_argument("life profile requires an untouched initial world");
    if(!state_.community.enabled)configure_community();
    auto& s=state_;s.life.enabled=true;s.balance="life-projects-0.11-experimental1";
    Organization organization;organization.id=1;organization.place=5;organization.supervisor=1;
    for(auto& a:s.actors){
        auto& m=a.mind.projects;m.enabled=true;m.profile.forecast_satiation=satiation;
        for(unsigned j=0;j<9;++j){double n=.20+.25*s.random.uniform("life-notice",a.id,j);m.profile.thresholds[j]={n,n-.06,std::min(.85,n+.35)};}
        m.profile.discount_per_hour=.01+.08*s.random.uniform("life-patience",a.id,0);
        m.profile.romantic_interest=.2+.7*s.random.uniform("life-interest",a.id,0);
        for(unsigned k=0;k<unsigned(GoalKind::Count);++k)m.procedures.push_back(familiar_procedure(GoalKind(k),s.next_id++));
        a.mind.known[std::size_t(Method::Visit)]=true;
        for(auto& r:a.mind.relations){r.introduced=true;r.last_seen=0;}
        for(unsigned k=unsigned(Interaction::Introduce);k<interaction_count;++k){
            auto& b=a.mind.social.methods[k];b.mastery=.85;b.expected_acceptance=.65;b.confidence=.6;b.source=s.next_id++;
            b.expected_pleasure=k==unsigned(Interaction::PartnerIntimacy)?.7:.25;
            a.social_traits.pleasure[k]=b.expected_pleasure;
        }
        auto& e=a.employment;e.active=true;e.organization=1;e.workplace=5;e.supervisor=1;e.rank=a.id==1?2:0;
        e.hourly=12.5*(1+.5*e.rank);e.promise_importance=.45+.5*s.random.uniform("life-promise",a.id,0);e.source=s.next_id++;
        EmployeeContribution member;member.person=a.id;member.hourly=e.hourly;member.role_weight=a.id==1?1.5:1;organization.members.push_back(member);
        a.economy.job=0;a.economy.wage_known=e.hourly;
    }
    s.life.organizations.push_back(organization);
}
void World::populate_life_view(const Actor& a,PersonalView& v)const{
    if(!state_.life.enabled)return;
    auto& l=v.life;l=LifePlanView{};l.enabled=true;l.profile=a.mind.projects.profile;l.procedures=a.mind.projects.procedures;
    l.employment=a.employment;l.conversation_project=a.conversation.project;l.current_project=a.action.project?a.action.project:a.conversation.project;
    if(a.cog.focus>=(4ull<<48)&&a.cog.focus<(5ull<<48))l.focus_project=a.cog.focus-(4ull<<48);
    l.private_satiation=a.satiation[std::size_t(Method::PrivateIntimacy)];l.primary_busy=a.action.phase!=Phase::Idle;
    l.conversation_started=a.conversation.id?a.conversation.started:a.action.started;
    for(const auto& p:a.mind.projects.projects)if(p.live()){auto copy=p;copy.observations.clear();l.projects.push_back(copy);}
    for(const auto& m:a.appointments)if(m.until>=state_.now)l.meetings.push_back(m);
    std::vector<Id> ids=v.perceived_people;
    if(v.social.partner)ids.insert(ids.begin(),v.social.partner);
    for(const auto& p:l.projects)if(p.goal.target)ids.push_back(p.goal.target);
    // Bounded retrieval from OWN social memory; a person's current hidden position is never used.
    for(const auto& r:a.mind.relations)if(r.introduced)ids.push_back(r.person);
    for(Id id:ids){
        if(l.people.size()==4)break;
        if(person(l,id))continue;
        const auto* r=a.mind.relation(id);const auto* belief=a.mind.social.person(id);
        if(!r&&!belief)continue;
        RelationshipView x;x.person=id;x.visible=std::find(v.perceived_people.begin(),v.perceived_people.end(),id)!=v.perceived_people.end()||v.social.partner==id;
        if(r){x.introduced=r->introduced;x.familiarity=r->familiarity;x.care=r->care;x.pleasure=r->contact_pleasure.count?r->contact_pleasure.mean:0;
            x.safety=r->trust[3].expectation();x.last_known_place=r->last_known_place;x.last_seen=r->last_seen;x.basis=r->origin;}
        if(x.visible){x.last_known_place=v.place;x.last_seen=v.now;}
        if(belief){if(!x.basis)x.basis=belief->source;
            x.attraction_known=belief->appearance_known;
            if(x.attraction_known)x.attraction=attraction(belief->appearance,a.mind.social.community.preference)*a.mind.social.expected_attraction[unsigned(belief->gender)];
            // Kin is a remembered fact, not a population lookup.
            if(belief->known_kin)x.attraction=0;
        }
        for(bool pub:{false,true}){
            if(const auto* e=a.mind.social.experience(Interaction::InviteMeeting,id,0,pub);e&&e->attempts)x.meeting_acceptance=(e->accepted+.3)/(e->accepted+e->refused+1.);
            if(const auto* e=a.mind.social.experience(Interaction::PartnerIntimacy,id,0,pub);e&&e->attempts)x.intimacy_acceptance=double(e->accepted)/(e->accepted+e->refused);
        }
        l.people.push_back(x);
    }
}
void World::life_recall(Actor& a){
    auto& m=a.mind.projects;if(!m.enabled)return;
    const Tick now=state_.now;m.review(now);
    if(now-m.last_generation<30000)return;
    m.last_generation=now;
    PersonalView v=a.cog.snapshot;v.life=LifePlanView{};populate_life_view(a,v);
    const auto cooled=[&](GoalKind k,Id id){for(auto it=m.projects.rbegin();it!=m.projects.rend();++it)if(it->goal.kind==k&&it->goal.target==id)return it->live()||now-it->updated>6*H;return true;};
    auto propose=[&](GoalKind k,Id who,double importance,double gain,double success,std::uint64_t basis){
        if(!cooled(k,who))return;
        if(std::count_if(m.projects.begin(),m.projects.end(),[](const auto& p){return p.live()&&p.goal.kind!=GoalKind::WorkShift;})>=4&&std::none_of(m.projects.begin(),m.projects.end(),[&](const auto& p){return p.live()&&p.goal.kind==k&&p.goal.target==who;}))return;
        GoalCandidate g;g.kind=k;g.target=who;g.importance=unit(importance);g.expected_gain=unit(gain);g.success=unit(success);g.basis=basis;
        g.deadline=now+2*D;g.delay=who?H:0;
        m.propose(g,1+unsigned(k),now);
    };
    auto& job=a.employment;
    const Tick day=now/D, tod=now%D;
    if(job.active&&day%7<5&&tod>=job.shift_start-H&&tod<job.shift_end&&job.created_shift!=day){
        GoalCandidate g;g.kind=GoalKind::WorkShift;g.place=job.workplace;g.importance=job.promise_importance;g.expected_gain=.8;g.success=.95;
        g.basis=job.source;g.not_before=day*D+job.shift_start-15*60000;g.deadline=day*D+job.shift_end;g.delay=std::max<Tick>(0,g.deadline-now);
        if(m.propose(g,1+unsigned(GoalKind::WorkShift),now))job.created_shift=day;
    }
    const double social=m.profile.thresholds[5].activation(v.need[5]);
    const double desire=m.profile.thresholds[6].activation(v.need[6]);
    // A sex drive does not conjure a partner. A known public-contact procedure can be retrieved first.
    if(const auto* known=m.known(1+unsigned(GoalKind::FindCompany));known&&(social>0||desire>.3))
        propose(GoalKind::FindCompany,0,std::max(social,.6*desire),.65,.7,known->source);
    for(const auto& r:v.life.people){
        if(!r.introduced)continue;
        if(social>0&&r.familiarity>.15&&r.pleasure>-.15)propose(GoalKind::MeetPerson,r.person,social,.6+.25*std::max(0.,r.pleasure),r.meeting_acceptance,r.basis);
        // Attraction and contact history are separate. Care does NOT add a sexual reward.
        if(r.attraction_known&&r.attraction>.20&&r.familiarity>.10&&r.pleasure>-.2&&desire>0){
            double motivation=unit(desire*(.4+.6*m.profile.romantic_interest)+.25*r.attraction+.15*std::max(0.,r.pleasure));
            propose(GoalKind::SeekIntimacy,r.person,motivation,.65+.25*r.attraction,r.meeting_acceptance*r.intimacy_acceptance,r.basis);
        }
    }
    for(auto& p:m.projects)if(p.live()&&p.goal.kind==GoalKind::WorkShift)p.goal.importance=unit(job.obligation(now)+.15);
}
std::vector<PlanOption> project_options(const PersonalView& v){
    std::vector<PlanOption> out;if(!v.life.enabled)return out;
    for(const auto& p:v.life.projects){
        if(!p.live()||v.now<p.retry_at||v.now<p.goal.not_before||(p.goal.deadline&&v.now>p.goal.deadline))continue;
        const auto* r=recipe(v.life,p.procedure);if(!r||p.step>=r->steps.size())continue;
        const auto step=r->steps[p.step].kind;
        PlanOption o;o.project=p.id;o.project_step=step;o.salience=.45+.55*p.goal.importance+(p.id==v.life.focus_project?.35:0);
        o.partner=p.goal.target;o.place=v.place;o.method=Method::Visit;
        const auto* who=person(v.life,p.goal.target);
        const auto can_talk=[&](){return v.social.in_conversation&&(!p.goal.target||v.social.partner==p.goal.target);};
        if(step==StepKind::Visit){
            // Public contact hubs known from biography. No scan for actual occupancy.
            auto hub=std::find_if(v.places.begin(),v.places.end(),[&](const auto& place){return place.id!=v.home&&(place.services&service(Method::Talk))&&place.id==2;});
            if(hub==v.places.end())continue;
            o.place=hub->id;o.partner=0;
        }else if(step==StepKind::Work){
            if(v.now%86400000<v.life.employment.shift_start){o.place=v.life.employment.workplace;}
            else if(v.life.employment.in_shift(v.now)){
                o.place=v.life.employment.workplace;o.method=Method::Work;
                if(v.now<v.life.employment.break_until||v.need[3]>.7){
                    const auto rest=std::find_if(v.places.begin(),v.places.end(),[&](const auto& k){return k.id==v.place&&(k.services&service(Method::Rest));});
                    if(rest!=v.places.end()){o.method=Method::Rest;o.place=v.place;}
                }
            }else continue;
        }else if(step==StepKind::Study){o.method=Method::Study;o.place=6;}
        else if(step==StepKind::Contact){
            if(can_talk()){o.method=Method::Social;o.interaction=p.goal.kind==GoalKind::FindCompany?Interaction::Introduce:Interaction::InviteMeeting;o.partner=v.social.partner;}
            else if(!p.goal.target){
                if(v.perceived_people.empty())continue;
                for(Id id:v.perceived_people){PlanOption choice=o;choice.method=Method::Talk;choice.partner=id;out.push_back(choice);if(out.size()>=8)break;}continue;
            }else if(who&&who->visible){o.method=Method::Talk;}
            else if(who&&who->last_known_place){
                // One already completed visit is not a new lead. Directly
                // perceived people remain actionable above; new whereabouts
                // reopen this search without reading the target's true position.
                if(p.searched_place==who->last_known_place&&who->last_seen<=p.searched_at)continue;
                o.place=who->last_known_place;o.method=Method::Visit;if(o.place==v.place)continue;
            }
            else continue;
        }else if(step==StepKind::Introduce){
            if(!can_talk()){o.method=Method::Talk;if(!who||!who->visible)continue;}
            else{o.method=Method::Social;o.interaction=Interaction::Introduce;o.partner=v.social.partner;}
        }else if(step==StepKind::Invite){
            if(!can_talk()){if(!who||!who->visible)continue;o.method=Method::Talk;}
            else {o.method=Method::Social;o.interaction=Interaction::InviteMeeting;}
        }else if(step==StepKind::Attend){
            auto meeting=std::find_if(v.life.meetings.begin(),v.life.meetings.end(),[&](const auto& x){return x.id==p.appointment;});
            if(meeting==v.life.meetings.end()||v.now<meeting->at-15*60000)continue;
            o.place=meeting->place;if(v.place==o.place&&v.now<meeting->at)o.method=Method::Idle;
        }else if(step==StepKind::SpendTime){
            if(can_talk()&&v.life.conversation_project==p.id)continue;
            if(!who||!who->visible)continue;
            o.method=Method::Talk;
        }else if(step==StepKind::Intimacy){
            if(!can_talk()){if(!who||!who->visible)continue;o.method=Method::Talk;}
            else{o.method=Method::Social;o.interaction=Interaction::PartnerIntimacy;}
        }else continue;
        if(o.method==Method::Social&&o.interaction==Interaction::InviteMeeting){
            auto& offer=o.meeting;offer.host=v.self;offer.private_visit=p.goal.kind==GoalKind::SeekIntimacy;offer.place=offer.private_visit?v.home:2;
            offer.at=v.now+H;if(v.life.employment.in_shift(offer.at))offer.at=(v.now/D)*D+18*H;
            offer.until=offer.at+2*H;offer.route_from=1;
            for(const auto& e:v.map)if((e.from==1&&e.to==offer.place)||(e.to==1&&e.from==offer.place))offer.route_seconds=e.seconds;
        }
        if(o.method==Method::Social&&v.social.occupied)continue;
        if(std::any_of(v.places.begin(),v.places.end(),[&](const auto& place){return place.id==o.place;}))out.push_back(o);
        if(out.size()>=8)break;
    }
    return out;
}
double project_forecast(const PersonalView& v,const PlanOption& option){
    auto it=std::find_if(v.life.projects.begin(),v.life.projects.end(),[&](const auto& p){return p.id==option.project;});
    if(it==v.life.projects.end())return -1;
    auto goal=it->goal;
    if(goal.kind==GoalKind::WorkShift){goal.delay=std::max<Tick>(0,goal.deadline-v.now);goal.importance=unit(v.life.employment.obligation(v.now)+.15);}
    if(goal.kind==GoalKind::SeekIntimacy){goal.importance=std::max(.15,goal.importance-.2*(1-v.need[6]));}
    // Value of the remaining outcome ONCE, not a reward for each substep.
    return continuation_value(goal,v.life.profile.discount_per_hour);
}
void World::life_commit(Actor& a,const Decision& d){
    if(!a.mind.projects.enabled||!d.project)return;
    auto& m=a.mind.projects;
    if(!m.activate(d.project,state_.now))return;
    auto* p=m.find(d.project);
    if(p->goal.kind==GoalKind::FindCompany&&!p->goal.target&&d.partner)p->goal.target=d.partner;
}
void World::life_action_result(Actor& a,StepOutcome value){
    if(!a.mind.projects.enabled||!a.action.project)return;
    auto& m=a.mind.projects;auto* p=m.find(a.action.project);if(!p||!p->live())return;
    if(value==StepOutcome::Interrupted){m.pause(p->id,state_.now,state_.now+30000);return;}
    if(value!=StepOutcome::Success){outcome(m,*p,value,a.action.id,state_.now);return;}
    const auto step=current_step(m,*p);
    if(a.action.method==Method::Visit&&step==StepKind::Contact){
        p->searched_place=a.place;p->searched_at=state_.now;++m.revision;
    }
    if(step==StepKind::Work)return; // Shift outcome is judged against actual attendance at settlement.
    if((a.action.method==Method::Visit||a.action.method==Method::Idle)&&step==StepKind::Attend){
        auto meeting=std::find_if(a.appointments.begin(),a.appointments.end(),[&](const auto& x){return x.id==p->appointment;});
        if(meeting!=a.appointments.end()&&state_.now>=meeting->at&&a.place==meeting->place)
            outcome(m,*p,value,a.action.id,state_.now);
    }else if(a.action.method==Method::Visit&&step==StepKind::Visit)outcome(m,*p,value,a.action.id,state_.now);
    if(a.action.method==Method::Study&&step==StepKind::Study)outcome(m,*p,value,a.action.id,state_.now);
}
void World::life_social_result(Actor& a,const InteractionObservation& o){
    if(!state_.life.enabled||o.kind<Interaction::Introduce)return;
    auto& m=a.mind.projects;
    if(o.kind==Interaction::RequestWork&&o.stage==SocialStage::Completed&&!o.initiated){
        if(a.employment.receive_request(o.event,o.other,.8,state_.now))++state_.life.work_requests;
    }
    if(o.kind==Interaction::RequestWork&&o.stage==SocialStage::Declined&&o.initiated)++state_.life.work_request_refusals;
    if(o.stage==SocialStage::Completed&&o.kind==Interaction::Introduce)a.mind.experience_contact(o.other,o.event,o.at,30,o.pleasure,a.place,true);
    if(o.kind==Interaction::InviteMeeting&&o.stage==SocialStage::Completed){
        const auto& msg=o.meeting;
        if(!msg.place||msg.at<o.at||msg.until<=msg.at)return;
        MeetingAgreement agreement{o.event,a.id,o.other,msg.place,msg.at,msg.until,msg.private_visit};
        if(std::none_of(a.appointments.begin(),a.appointments.end(),[&](const auto& x){return x.id==o.event;}))a.appointments.push_back(agreement);
        // The host's communicated address becomes reported knowledge, not telepathic coordinates.
        if(std::none_of(a.mind.places.begin(),a.mind.places.end(),[&](const auto& x){return x.id==msg.place;})){
            KnownPlace p;p.id=msg.place;p.services=service(Method::Talk)|service(Method::Rest)|service(Method::Visit);a.mind.places.push_back(p);
            std::sort(a.mind.places.begin(),a.mind.places.end(),[](const auto& x,const auto& y){return x.id<y.id;});
        }
        if(msg.route_seconds&&std::none_of(a.mind.map.begin(),a.mind.map.end(),[&](const auto& e){return (e.from==msg.route_from&&e.to==msg.place)||(e.to==msg.route_from&&e.from==msg.place);})){a.mind.map.push_back({msg.route_from,msg.place,msg.route_seconds,Truth::Confirmed});}
        PersonalProject* p=o.initiated?m.find(o.project):nullptr;
        if(!p){GoalCandidate g;g.kind=GoalKind::MeetPerson;g.target=o.other;g.importance=.75;g.expected_gain=.7;g.success=.8;g.basis=o.event;g.deadline=msg.until;auto id=m.propose(g,2,state_.now);p=m.find(id);}
        if(p){p->appointment=o.event;p->goal.place=msg.place;p->goal.deadline=msg.until;p->goal.not_before=msg.at-15*60000;
            p->step=2;p->status=ProjectStatus::Waiting;p->retry_at=msg.at-15*60000;p->updated=state_.now;++m.revision;}
    }
    if(!o.initiated)return;
    auto* p=m.find(o.project);if(!p||!p->live())return;
    if(o.stage==SocialStage::Declined){outcome(m,*p,StepOutcome::Refused,o.event,state_.now);return;}
    if(o.stage==SocialStage::Cancelled){outcome(m,*p,StepOutcome::Unavailable,o.event,state_.now);return;}
    if(o.stage==SocialStage::Completed){
        const auto step=current_step(m,*p);
        if(o.kind==Interaction::Introduce&&(step==StepKind::Contact||step==StepKind::Introduce)){
            // Contact was observed when the conversation began. Introduction is separately observed.
            if(step==StepKind::Contact){p->step=2;++m.revision;}
            outcome(m,*p,StepOutcome::Success,o.event,state_.now);
        }
        if(o.kind==Interaction::PartnerIntimacy&&step==StepKind::Intimacy)outcome(m,*p,StepOutcome::Success,o.event,state_.now);
    }
}
void World::life_social_completed(SocialEvent& e){
    if(!state_.life.enabled)return;
    if(e.kind==Interaction::Introduce)++state_.life.introductions;
    if(e.kind==Interaction::InviteMeeting){const auto& x=e.meeting;state_.life.meetings.push_back({e.id,e.initiator,e.receiver,x.place,x.at,x.until,x.private_visit});++state_.life.meetings_agreed;}
    if(e.kind==Interaction::PartnerIntimacy){
        for(Id id:{e.initiator,e.receiver}){auto& a=state_.actors[id-1];a.desire.satisfy(e.id,.6);a.cog.needs[6].status=Truth::Unknown;a.partner_ms+=double(interaction_duration(e.kind));}
        ++state_.life.intimacies;state_.life.partner_seconds+=double(interaction_duration(e.kind))/1000.;
    }
}
void World::validate_life()const{
    if(!state_.life.enabled)return;
    for(const auto& o:state_.life.organizations)o.validate();
    for(const auto& a:state_.actors){a.mind.projects.validate(state_.now);
        if(a.conversation.id){if(!a.conversation.partner||a.conversation.partner>state_.actors.size()||a.conversation.ends<state_.now)throw std::runtime_error("invalid parallel conversation");
            const auto& b=state_.actors[a.conversation.partner-1];if(b.conversation.id!=a.conversation.id||b.conversation.partner!=a.id)throw std::runtime_error("asymmetric parallel channel");}
        for(const auto& r:a.mind.relations){if(!std::is_sorted(r.contact_episodes.begin(),r.contact_episodes.end())||std::adjacent_find(r.contact_episodes.begin(),r.contact_episodes.end())!=r.contact_episodes.end())throw std::runtime_error("duplicate contact experience");}
    }
}
std::string World::life_report_json()const{
    std::ostringstream o;o.precision(17);const auto& s=state_;
    std::uint64_t created=0,complete=0,paused=0,resumed=0,abandoned=0;double social_ms=0;
    o<<"{\"version\":\"0.12.0-self01\",\"enabled\":"<<(s.life.enabled?"true":"false")<<",\"seed\":"<<s.random.value<<",\"seconds\":"<<s.now/1000<<",\"population\":"<<s.actors.size();
    o<<",\"introductions\":"<<s.life.introductions<<",\"meetings\":"<<s.life.meetings_agreed<<",\"partner_intimacy_completed\":"<<s.life.intimacies<<",\"parallel_started\":"<<s.life.parallel_started<<",\"work_requests\":"<<s.life.work_requests<<",\"work_requests_declined\":"<<s.life.work_request_refusals;
    o<<",\"actors\":[";bool first=true;
    for(const auto& a:s.actors){const auto& m=a.mind.projects;created+=m.created;complete+=m.completed;paused+=m.pauses;resumed+=m.resumes;abandoned+=m.abandoned;social_ms+=a.secondary_social_ms;
        if(!first)o<<',';
        first=false;o<<"{\"id\":"<<a.id<<",\"money\":"<<a.money<<",\"secondary_social_hours\":"<<a.secondary_social_ms/3600000.<<",\"projects\":[";bool fp=true;
        for(const auto& p:m.projects){if(!fp)o<<',';fp=false;o<<"{\"id\":"<<p.id<<",\"kind\":\""<<goal_name(p.goal.kind)<<"\",\"target\":"<<p.goal.target<<",\"status\":\""<<project_status_name(p.status)<<"\",\"step\":"<<p.step<<",\"created\":"<<p.created<<",\"updated\":"<<p.updated<<",\"failures\":"<<p.failures<<'}';}o<<"]}";
    }
    o<<"],\"created\":"<<created<<",\"completed\":"<<complete<<",\"pauses\":"<<paused<<",\"resumes\":"<<resumed<<",\"abandoned\":"<<abandoned<<",\"secondary_social_hours\":"<<social_ms/3600000.<<",\"organizations\":[";first=true;
    for(const auto& g:s.life.organizations){if(!first)o<<',';first=false;o<<"{\"id\":"<<g.id<<",\"development\":"<<g.development<<",\"multiplier\":"<<g.multiplier()<<",\"payroll\":[";bool f=true;
        for(const auto& p:g.payroll){if(!f)o<<',';f=false;o<<"{\"person\":"<<p.person<<",\"day\":"<<p.day<<",\"hours\":"<<p.hours<<",\"effective_hours\":"<<p.effective_hours<<",\"amount\":"<<p.amount<<'}';}o<<"]}";
    }o<<"]}";return o.str();
}
void World::invitation_for_test(Id id,Id other,const MeetingProposal& p){Decision d;d.method=Method::Social;d.interaction=Interaction::InviteMeeting;d.partner=other;d.meeting=p;start_social(state_.actors.at(id-1),d);}
} // namespace life
