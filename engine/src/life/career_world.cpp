#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
namespace life {
namespace {
constexpr Tick H=3600000,D=86400000;
constexpr std::uint64_t career_base=8ull<<48,career_end=9ull<<48;
Place& venue(State& s,Id id){auto p=std::find_if(s.places.begin(),s.places.end(),[&](const auto& x){return x.id==id;});if(p==s.places.end())throw std::runtime_error("unknown hiring place");return *p;}
Organization& organization(State& s,Id id){auto o=std::find_if(s.life.organizations.begin(),s.life.organizations.end(),[&](const auto& x){return x.id==id;});if(o==s.life.organizations.end())throw std::runtime_error("unknown employer");return *o;}
unsigned employed(const State& s,Id org){return unsigned(std::count_if(s.actors.begin(),s.actors.end(),[&](const auto& a){return a.employment.active&&a.employment.organization==org;}));}
}
void World::configure_adaptive_life(bool effects){
    auto& s=state_;if(s.now||s.adaptive_life)throw std::invalid_argument("adaptive profile requires initial world");
    if(!s.life.enabled)configure_life_projects();
    if(!s.self_enabled)configure_self_model(effects);
    s.adaptive_life=true;s.balance="self01-adaptive-1";
    const unsigned n=unsigned(s.actors.size());
    s.hiring={{1,5,n,12.5,0,9*H,17*H,"workshop"},{2,7,(n+1)/2,15,0,9*H,17*H,"shop"},
              {3,8,(n+1)/2,18,0,9*H,17*H,"recreation_service"},{4,9,(n+1)/2,21,702,9*H,17*H,"qualified_service"}};
    for(auto& office:s.hiring){
        auto& p=venue(s,office.place);p.services|=service(Method::Work)|service(Method::InquireJob)|service(Method::ApplyJob)|service(Method::Rest)|service(Method::Drink)|service(Method::Eat);
        p.wage=office.hourly;p.job_level=0; // contractual qualification is checked at hiring, not at every 30-minute work segment
        p.water+=n*6;s.ledger.initial_water+=n*6;
        if(office.organization!=1){Organization o;o.id=office.organization;o.place=office.place;o.supervisor=1;s.life.organizations.push_back(o);}
    }
    auto& initial=organization(s,1);
    for(auto& a:s.actors){
        const double money=18+18*s.random.uniform("adaptive-starting-cash",a.id,0);
        s.ledger.initial_money+=money-a.money;a.money=a.mind.believed_money=money;
        s.ledger.initial_food+=2-a.food;a.food=a.mind.believed_food=2;
        if(a.id%4==0){a.employment.active=false;for(auto& m:initial.members)if(m.person==a.id)m.active=false;}
        auto& memory=a.mind.career;memory.enabled=true;
        a.mind.known[std::size_t(Method::InquireJob)]=a.mind.known[std::size_t(Method::ApplyJob)]=true;
        a.mind.priors[std::size_t(Method::InquireJob)][7]=.2;a.mind.priors[std::size_t(Method::ApplyJob)][7]=.5;
        for(const auto& office:s.hiring){
            auto& p=venue(s,office.place);auto k=std::find_if(a.mind.places.begin(),a.mind.places.end(),[&](const auto& x){return x.id==p.id;});
            if(k!=a.mind.places.end()){k->services=p.services;k->job_level=0;k->wage=office.hourly;}
            // Starting public notice: location and advertised wage, NOT a guaranteed vacancy or personal offer.
            JobKnowledge fact;fact.organization=office.organization;fact.place=office.place;fact.hourly=office.hourly;fact.known=LocationKnown|WageKnown;
            memory.receive(fact,s.next_id++,0);
        }
        ++a.mind.version;++a.cog.situation_version;
    }
    validate();
}
void World::receive_career_information(Actor& a,const JobKnowledge& fact,std::uint64_t source){
    if(!state_.adaptive_life||!a.mind.career.enabled)return;
    if(a.mind.career.receive(fact,source,state_.now)){++a.mind.version;++a.cog.situation_version;a.cog.review_at=std::min(a.cog.review_at,state_.now);}
}
void World::populate_career_view(const Actor& a,PersonalView& v)const{
    if(!a.mind.career.enabled)return;
    const auto& m=a.mind.career;auto& out=v.career;
    if(!m.active_target)return;
    const auto* offer=m.find(m.active_target);if(!offer)return;
    const auto q=std::find_if(m.questions.begin(),m.questions.end(),[&](const auto& x){return x.organization==offer->organization;});if(q==m.questions.end())return;
    out.enabled=true;out.stage=q->stage;out.offer=*offer;out.basis=q->basis;out.expected_gain=q->assessment.expected_gain;out.retry_at=m.retry_at;
    // Planner receives only a previously assessed personal offer. Provenance
    // roots are for the diagnostic hand-off, not an archive scan.
}
void World::append_career_topics(const Actor& a,std::vector<AttentionTopic>& input)const{
    if(!state_.adaptive_life)return;
    for(const auto& q:a.mind.career.questions)if(q.stage==CareerStage::Review&&q.next_review<=state_.now){
        // An unexamined answer relevant to the ongoing income goal must not
        // remain permanently below the focus-switch margin of "need money".
        // Waiting raises attention only, never utility or a duty to change jobs.
        const double pending_age=unit(double(state_.now-q.next_review)/600000.);
        const double priority=unit((a.employment.active?.62:.80)+.35*pending_age);
        input.push_back({career_base+q.organization,TopicKind::Thought,7,0,priority,.4,q.next_review,state_.now+31000,a.mind.career.revision,q.basis,false});
    }
}
bool World::start_career_cognition(Actor& a){
    auto& c=a.cog;if(!state_.adaptive_life||c.focus<career_base||c.focus>=career_end)return false;
    auto& memory=a.mind.career;auto* job=memory.find(Id(c.focus-career_base));if(!job)return false;
    if(c.budget<2){c.active=false;return true;}
    c.career_input=*job;c.career_assessment={};c.career_own_hourly=a.employment.active?a.employment.hourly*a.employment.known_multiplier:0;
    c.career_employed=a.employment.active;c.career_mastery=job->required_skill?a.mind.knowledge.get(job->required_skill):1;
    c.context.resize(std::min<std::size_t>(1,c.context.size()));
    c.context.push_back({job->organization,c.focus_basis,ThoughtKind::Recall,7,0,job->hourly,.8,Truth::Confirmed});
    c.peak_slots=std::max(c.peak_slots,int(c.context.size()));cognition_start(a,Operation::RecallCareerFacts);return true;
}
bool World::complete_career_cognition(Actor& a,Operation op,Tick started){
    if(op!=Operation::RecallCareerFacts&&op!=Operation::CompareCareerFacts)return false;
    auto& c=a.cog;auto& m=a.mind.career;
    auto it=std::find_if(m.questions.begin(),m.questions.end(),[&](const auto& q){return q.organization==c.career_input.organization;});
    if(it==m.questions.end())throw std::logic_error("career question lost");
    Thought t;t.started=started;t.metric=7;t.basis=it->basis;t.debug_knowledge_source=it->basis;t.debug_object=it->organization;
    t.origin=Origin::Inferred;t.status=Truth::Unknown;t.confidence=.8;
    if(op==Operation::RecallCareerFacts){
        t.kind=ThoughtKind::Recall;t.value=c.career_input.hourly;t.detail="remembered_job_terms_not_hidden_vacancy";emit_thought(a,t);
        c.career_assessment=assess_career(c.career_input,c.career_own_hourly,c.career_employed,c.career_mastery);
        if(a.employment.active&&a.employment.organization==it->organization){c.career_assessment.stage=CareerStage::Deferred;c.career_assessment.reason="already_employed_here";}
        cognition_start(a,Operation::CompareCareerFacts);return true;
    }
    const auto before=it->stage;it->assessment=c.career_assessment;it->stage=it->assessment.stage;it->reviewed_revision=c.career_input.revision;++it->reviews;++m.questions_reviewed;
    // A question already examined with unchanged inputs is dormant, not a
    // standing command to think about the same vacancy every physical tick.
    it->next_review=state_.now+D;
    Id selected=0;double value=-1;
    for(const auto& q:m.questions)if(q.stage==CareerStage::Inquire||q.stage==CareerStage::Apply||q.stage==CareerStage::Study){
        const double score=q.assessment.expected_gain+(q.stage==CareerStage::Apply?.3:q.stage==CareerStage::Inquire?.1:0);
        if(score>value){value=score;selected=q.organization;}
    }
    if(selected!=m.active_target){++m.changed_by_information;m.active_basis=it->basis;}m.active_target=selected;
    m.record({state_.now,it->organization,it->basis,0,0,std::string("review_to_")+career_stage_name(it->stage),c.career_own_hourly,c.career_input.hourly});
    t.kind=ThoughtKind::Compare;t.value=it->assessment.expected_gain;t.detail=std::string("career_")+career_stage_name(before)+"_to_"+career_stage_name(it->stage)+";"+it->assessment.reason;
    t.method=it->stage==CareerStage::Apply?Method::ApplyJob:it->stage==CareerStage::Inquire?Method::InquireJob:Method::Work;emit_thought(a,t);
    ++a.mind.version;c.active=false;c.operation=Operation::None;c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;
    c.review_at=state_.now+1000;a.review=c.review_at;return true;
}
bool World::complete_career_action(Actor& a){
    const auto method=a.action.method;if(method!=Method::InquireJob&&method!=Method::ApplyJob)return false;
    auto& s=state_;auto& m=a.mind.career;
    auto office=std::find_if(s.hiring.begin(),s.hiring.end(),[&](const auto& x){return x.place==a.place;});
    if(office==s.hiring.end()){fail(a,"unknown_hiring_office");return true;}
    const auto root=a.action.id;const auto event=s.next_id++;
    JobKnowledge fact;fact.organization=office->organization;fact.place=office->place;fact.hourly=office->hourly;
    fact.shift_start=office->shift_start;fact.shift_end=office->shift_end;fact.required_skill=office->required_skill;
    fact.vacancy=employed(s,office->organization)<office->capacity;fact.known=31;
    receive_career_information(a,fact,event);
    bool accepted=true;const char* result="job_terms_observed";
    if(method==Method::InquireJob){++m.inquiries;}
    else{
        ++m.applications;
        accepted=fact.vacancy&&(!fact.required_skill||a.mind.knowledge.get(fact.required_skill)>=.6);
        if(accepted){
            if(a.employment.active)for(auto& x:organization(s,a.employment.organization).members)if(x.person==a.id)x.active=false;
            auto& org=organization(s,office->organization);
            auto member=std::find_if(org.members.begin(),org.members.end(),[&](const auto& x){return x.person==a.id;});
            if(member==org.members.end()){EmployeeContribution x;x.person=a.id;x.hourly=office->hourly;org.members.push_back(x);}
            else{member->active=true;member->hourly=office->hourly;}
            auto& e=a.employment;e.active=true;e.organization=org.id;e.workplace=org.place;e.supervisor=org.supervisor;e.rank=0;
            e.hourly=office->hourly;e.known_multiplier=org.multiplier();e.shift_start=office->shift_start;e.shift_end=office->shift_end;e.source=event;e.created_shift=-1;e.work_streak=0;
            a.economy.wage_known=e.hourly*e.known_multiplier;a.economy.job=0;
            for(auto& p:a.mind.projects.projects)if(p.live()&&p.goal.kind==GoalKind::WorkShift)a.mind.projects.abandon(p.id,s.now);
            ++m.hired;result="employment_contract_accepted";
            for(auto& q:m.questions){q.stage=q.organization==org.id?CareerStage::Hired:CareerStage::Review;q.next_review=s.now;q.basis=event;}
        }else{++m.refused;result=fact.vacancy?"qualification_required":"vacancy_filled";
            for(auto& q:m.questions)if(q.organization==office->organization){q.stage=CareerStage::Deferred;q.next_review=s.now+D;q.basis=event;q.assessment.reason=result;}}
        m.active_target=0;m.retry_at=s.now+300000;
    }
    m.record({s.now,office->organization,event,root,0,result,0,fact.hourly});
    OutcomeSignal outcome;outcome.id=event;outcome.root=event;outcome.action=root;outcome.method=method;outcome.at=s.now;
    outcome.completed=accepted;outcome.blocked=!accepted;outcome.feedback_observed=true;outcome.directness=capability(a.body,a.mind.cognition,false).gate;
    outcome.goal_importance=.8;outcome.other_decision=method==Method::ApplyJob?1:0;outcome.action_feedback=accepted?.8:.1;
    outcome.acceptance_observed=method==Method::ApplyJob;outcome.accepted=accepted?1:0;outcome.decision=a.action.decision_experience;
    publish_outcome(a,outcome);
    ++a.completed[std::size_t(method)];emit(a,"career",result);a.action=Action{};a.review=s.now;a.exposure.clear();
    a.mind.believed_money=a.money;++a.mind.version;++a.cog.situation_version;return true;
}
void World::validate_career()const{
    if(!state_.adaptive_life)return;
    if(!state_.life.enabled||!state_.self_enabled||state_.hiring.empty())throw std::runtime_error("adaptive profile missing dependencies");
    std::set<Id> ids;for(const auto& office:state_.hiring){
        JobKnowledge k;k.organization=office.organization;k.place=office.place;k.hourly=office.hourly;k.shift_start=office.shift_start;k.shift_end=office.shift_end;k.known=7;k.validate();
        if(!ids.insert(office.organization).second)throw std::runtime_error("duplicate hiring office");
        if(employed(state_,office.organization)>office.capacity)throw std::runtime_error("overbooked employment capacity");
    }
    for(const auto& a:state_.actors){a.mind.career.validate();
        if(a.employment.active){auto it=std::find_if(state_.life.organizations.begin(),state_.life.organizations.end(),[&](const auto& o){return o.id==a.employment.organization;});
            if(it==state_.life.organizations.end()||std::none_of(it->members.begin(),it->members.end(),[&](const auto& x){return x.person==a.id&&x.active;}))throw std::runtime_error("employment membership mismatch");}
        if(a.cog.operation==Operation::RecallCareerFacts||a.cog.operation==Operation::CompareCareerFacts)a.cog.career_input.validate();
    }
}
std::string World::career_report_json()const{
    std::ostringstream out;out.precision(17);out<<"{\"enabled\":"<<(state_.adaptive_life?"true":"false")<<",\"actors\":[";
    bool first=true;for(const auto& a:state_.actors){if(!first)out<<',';first=false;const auto& m=a.mind.career;
        out<<"{\"id\":"<<a.id<<",\"employed\":"<<(a.employment.active?"true":"false")<<",\"organization\":"<<a.employment.organization<<",\"hourly\":"<<a.employment.hourly
           <<",\"inquiries\":"<<m.inquiries<<",\"applications\":"<<m.applications<<",\"hired\":"<<m.hired<<",\"refused\":"<<m.refused<<",\"interrupted\":"<<m.interrupted
           <<",\"application_starts\":"<<m.application_starts<<",\"inquiry_starts\":"<<m.inquiry_starts<<",\"interrupted_applications\":"<<m.interrupted_applications<<",\"failed_applications\":"<<m.failed_applications
           <<",\"semantic_changes\":"<<m.semantic_changes<<",\"reviews\":"<<m.questions_reviewed<<",\"reopened\":"<<m.reopened<<",\"target_changes\":"<<m.changed_by_information<<",\"active_target\":"<<m.active_target<<",\"questions\":[";
        bool fq=true;for(const auto& q:m.questions){if(!fq)out<<',';fq=false;out<<"{\"organization\":"<<q.organization<<",\"stage\":\""<<career_stage_name(q.stage)<<"\",\"basis\":"<<q.basis<<",\"reviews\":"<<q.reviews<<",\"reason\":\""<<q.assessment.reason<<"\"}";}out<<"],\"evidence\":[";
        bool fe=true;for(const auto& e:m.evidence){if(!fe)out<<',';fe=false;out<<"{\"ms\":"<<e.at<<",\"org\":"<<e.organization<<",\"source\":"<<e.source<<",\"action\":"<<e.action<<",\"changed_fields\":"<<e.changed_fields<<",\"kind\":\""<<e.kind<<"\",\"before_wage\":"<<e.before_wage<<",\"after_wage\":"<<e.after_wage<<'}';}
        out<<"]}";
    }out<<"]}";return out.str();
}
} // namespace life
