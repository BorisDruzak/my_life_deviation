#include "life/career.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace life {
const char* career_stage_name(CareerStage stage){
    static constexpr const char* names[]={"review","inquire","apply","study","deferred","hired"};
    if(unsigned(stage)>unsigned(CareerStage::Hired))throw std::invalid_argument("career stage");
    return names[unsigned(stage)];
}
void JobKnowledge::validate()const{
    if(!organization||known>31)throw std::invalid_argument("job knowledge identity/mask");
    if((known&LocationKnown)&&!place)throw std::invalid_argument("known job without place");
    if(known&WageKnown)require_range(hourly,.001,1e6);
    if((known&ScheduleKnown)&&(shift_start<0||shift_end<=shift_start||shift_end>86400000||shift_end-shift_start>12*3600000))throw std::invalid_argument("known job schedule");
}
const JobKnowledge* CareerMemory::find(Id org)const{
    auto it=std::find_if(jobs.begin(),jobs.end(),[&](const auto& x){return x.organization==org;});return it==jobs.end()?nullptr:&*it;
}
JobKnowledge* CareerMemory::find(Id org){return const_cast<JobKnowledge*>(static_cast<const CareerMemory&>(*this).find(org));}
void CareerMemory::record(CareerEvidence item){
    if(evidence.size()>=64)evidence.erase(evidence.begin());
    evidence.push_back(std::move(item));
}
bool CareerMemory::receive(const JobKnowledge& message,std::uint64_t source,Tick now){
    message.validate();if(!source||now<0)throw std::invalid_argument("job knowledge needs accessible source/time");
    ++received;auto* old=find(message.organization);
    if(!old){if(jobs.size()>=8)return false;jobs.push_back(JobKnowledge{});old=&jobs.back();old->organization=message.organization;
      CareerQuestion q;q.organization=message.organization;questions.push_back(q);}
    bool changed=false;unsigned changed_fields=0;const double before_wage=old->hourly;
    for(unsigned f=0;f<5;++f){unsigned bit=1u<<f;if(!(message.known&bit))continue;
        if((old->known&bit)&&now<old->times[f])continue;
        bool different=!(old->known&bit);
        switch(bit){
          case LocationKnown:different|=old->place!=message.place;old->place=message.place;break;
          case WageKnown:different|=std::abs(old->hourly-message.hourly)>1e-9;old->hourly=message.hourly;break;
          case ScheduleKnown:different|=old->shift_start!=message.shift_start||old->shift_end!=message.shift_end;old->shift_start=message.shift_start;old->shift_end=message.shift_end;break;
          case VacancyKnown:different|=old->vacancy!=message.vacancy;old->vacancy=message.vacancy;break;
          case QualificationKnown:different|=old->required_skill!=message.required_skill;old->required_skill=message.required_skill;break;
        }
        old->known|=bit;old->times[f]=now;
        // A restatement is not a new evidential basis for changing a decision.
        if(different){old->sources[f]=source;changed=true;changed_fields|=bit;}
    }
    if(changed){record({now,message.organization,source,0,changed_fields,"new_information",before_wage,old->hourly});++revision;++semantic_changes;++old->revision;
        for(auto& q:questions)if(q.organization==old->organization){if(q.reviews)++reopened;q.stage=CareerStage::Review;q.next_review=now;q.basis=source;}
    }
    return changed;
}
CareerAssessment assess_career(const JobKnowledge& k,double own_hourly,bool employed,double mastery){
    k.validate();require_range(own_hourly,0,1e6);require_range(mastery,0,1);
    CareerAssessment a;
    if(!(k.known&LocationKnown)){a.reason="unknown_contact_location";return a;}
    if((k.known&WageKnown)&&employed&&k.hourly<=own_hourly*1.05){a.uncertainty=.15;a.reason="no_known_income_improvement";return a;}
    a.expected_gain=(k.known&WageKnown)?unit((k.hourly-own_hourly)/std::max(12.5,own_hourly)):.4;
    if(k.known!=31){a.stage=CareerStage::Inquire;a.reason="terms_vacancy_or_qualification_unknown";return a;}
    if(!k.vacancy){a.reason="known_no_vacancy";return a;}
    if(k.required_skill&&mastery<.6){
        // Explicit personal planning estimate: 40 work-hour horizon, up to
        // 24 study hours for an entirely missing basic qualification, tuition
        // 8/hour. This is prospective cost, never a deduction or a skill gain.
        const double training_hours=24*(.6-mastery)/.6;
        const double remaining_hours=std::max(0.,40-training_hours);
        a.expected_gain=unit(((k.hourly-own_hourly)*remaining_hours-8*training_hours)/(40*std::max(12.5,own_hourly)));
        a.stage=CareerStage::Study;a.uncertainty=.2;a.reason="qualification_missing_training_delay_and_cost";return a;
    }
    a.stage=CareerStage::Apply;a.uncertainty=.15;a.reason="known_advantage_apply_not_assume_hired";return a;
}
void CareerMemory::validate()const{
    if(jobs.size()>8||questions.size()!=jobs.size()||retry_at<0||evidence.size()>64)throw std::runtime_error("career capacity/state");
    std::set<Id> unique;
    for(const auto& k:jobs){k.validate();if(!unique.insert(k.organization).second)throw std::runtime_error("duplicate job knowledge");
        for(unsigned i=0;i<5;++i)if((k.known&(1u<<i))&&(!k.sources[i]||k.times[i]<0))throw std::runtime_error("job knowledge lacks provenance");}
    unique.clear();for(const auto& q:questions){if(!find(q.organization)||!unique.insert(q.organization).second||q.next_review<0)throw std::runtime_error("invalid career question");career_stage_name(q.stage);career_stage_name(q.assessment.stage);require_range(q.assessment.expected_gain,0,1);require_range(q.assessment.uncertainty,0,1);}
    for(const auto& e:evidence){if(e.at<0||!e.organization||!e.source||e.changed_fields>31)throw std::runtime_error("career evidence identity");require_range(e.before_wage,0,1e6);require_range(e.after_wage,0,1e6);}
    if(active_target&&!find(active_target))throw std::runtime_error("unknown career target");
}
} // namespace life
