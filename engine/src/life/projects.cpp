#include "life/projects.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <set>

namespace life {
namespace {
void valid_goal(const GoalCandidate& g) {
    if(unsigned(g.kind)>=unsigned(GoalKind::Count)||!g.basis)throw std::invalid_argument("goal kind/source");
    for(double v:{g.importance,g.expected_gain,g.success,g.remaining_cost})require_range(v,0,1);
    if(g.not_before<0||g.deadline<0||g.delay<0||(g.deadline&&g.deadline<g.not_before))throw std::invalid_argument("goal times");
    if((g.kind==GoalKind::MeetPerson||g.kind==GoalKind::SeekIntimacy)&&!g.target)throw std::invalid_argument("addressed goal needs a person");
}
void clock_check(const PersonalProject& p,Tick now){if(now<p.updated)throw std::invalid_argument("project time reversal");}
}
const char* goal_name(GoalKind k){
    static constexpr const char* names[]={"find_company","meet_person","seek_intimacy","work_shift","learn_skill"};
    if(unsigned(k)>=std::size(names))throw std::invalid_argument("goal kind");
    return names[unsigned(k)];
}
const char* project_status_name(ProjectStatus s){
    static constexpr const char* names[]={"candidate","active","waiting","paused","completed","abandoned","expired"};
    if(unsigned(s)>=std::size(names))throw std::invalid_argument("project status");
    return names[unsigned(s)];
}
bool PersonalProject::live()const{return status==ProjectStatus::Candidate||status==ProjectStatus::Active||status==ProjectStatus::Waiting||status==ProjectStatus::Paused;}
void NeedThreshold::validate()const{
    for(double v:{notice,release,strong})require_range(v,0,1);
    if(!(release<notice&&notice<strong))throw std::invalid_argument("need thresholds must be ordered");
}
double NeedThreshold::activation(double perceived,bool was_active)const{
    validate();require_range(perceived,0,1);
    if(perceived<=(was_active?release:notice))return 0;
    return unit((perceived-release)/(strong-release));
}
const KnownProcedure* ProjectMemory::known(std::uint32_t id)const{
    const auto it=std::find_if(procedures.begin(),procedures.end(),[&](const auto& p){return p.id==id;});
    return it==procedures.end()?nullptr:&*it;
}
PersonalProject* ProjectMemory::find(std::uint64_t id){return const_cast<PersonalProject*>(std::as_const(*this).find(id));}
const PersonalProject* ProjectMemory::find(std::uint64_t id)const{
    const auto it=std::lower_bound(projects.begin(),projects.end(),id,[](const auto& p,auto key){return p.id<key;});
    return it==projects.end()||it->id!=id?nullptr:&*it;
}
std::uint64_t ProjectMemory::propose(const GoalCandidate& goal,std::uint32_t procedure,Tick now){
    valid_goal(goal);if(now<0)throw std::invalid_argument("negative project clock");
    const auto* recipe=known(procedure);
    if(!enabled||!recipe||recipe->mastery<.6||recipe->goal!=goal.kind||recipe->steps.empty())return 0;
    for(const auto& p:projects)if(p.live()&&p.goal.kind==goal.kind&&p.goal.target==goal.target)return p.id;
    if(std::count_if(projects.begin(),projects.end(),[](const auto& p){return p.live();})>=8)return 0;
    PersonalProject p;p.id=next_id++;p.goal=goal;p.procedure=procedure;p.created=p.updated=now;p.retry_at=goal.not_before;
    projects.push_back(p);++created;++revision;return p.id;
}
bool ProjectMemory::activate(std::uint64_t id,Tick now){
    auto* p=find(id);if(!p)return false;clock_check(*p,now);
    if(!p->live()||now<p->retry_at||now<p->goal.not_before||(p->goal.deadline&&now>p->goal.deadline))return false;
    if(p->status==ProjectStatus::Active)return true;
    if(p->status==ProjectStatus::Paused||p->status==ProjectStatus::Waiting)++resumes;
    p->status=ProjectStatus::Active;p->updated=now;++revision;return true;
}
bool ProjectMemory::pause(std::uint64_t id,Tick now,Tick retry){
    auto* p=find(id);if(!p)return false;clock_check(*p,now);if(retry<now)throw std::invalid_argument("past retry");
    if(!p->live())return false;
    if(p->status!=ProjectStatus::Paused)++pauses;
    p->status=ProjectStatus::Paused;p->updated=now;p->retry_at=retry;++revision;return true;
}
bool ProjectMemory::abandon(std::uint64_t id,Tick now){
    auto* p=find(id);if(!p)return false;clock_check(*p,now);if(!p->live())return false;
    p->status=ProjectStatus::Abandoned;p->updated=now;++abandoned;++revision;return true;
}
bool ProjectMemory::observe(std::uint64_t id,StepOutcome outcome,std::uint64_t event,Tick now){
    if(!event||unsigned(outcome)>unsigned(StepOutcome::Interrupted))throw std::invalid_argument("project outcome");
    auto* p=find(id);if(!p)return false;clock_check(*p,now);
    if(!p->live()||std::binary_search(p->observations.begin(),p->observations.end(),event))return false;
    const auto* recipe=known(p->procedure);if(!recipe||p->step>=recipe->steps.size())throw std::logic_error("project recipe missing");
    const auto step=recipe->steps[p->step];
    p->observations.insert(std::lower_bound(p->observations.begin(),p->observations.end(),event),event);
    p->updated=now;++revision;
    if(outcome==StepOutcome::Interrupted){p->status=ProjectStatus::Paused;p->retry_at=now+30000;++pauses;return true;}
    if(outcome==StepOutcome::Success){
        p->step=step.success;p->status=ProjectStatus::Active;p->retry_at=now;
        if(p->step>=recipe->steps.size()||recipe->steps[p->step].kind==StepKind::Finish){p->status=ProjectStatus::Completed;++completed;for(auto& r:procedures)if(r.id==p->procedure)++r.successes;}
    }else{
        ++p->failures;p->step=outcome==StepOutcome::Refused?step.refused:step.unavailable;
        p->status=ProjectStatus::Waiting;p->retry_at=now+step.retry_delay;
        // The known social procedure abandons on a refusal once total failures >= 3;
        // a new relationship never materialises as a result of persistence.
        if(outcome==StepOutcome::Refused&&p->failures>=3){p->status=ProjectStatus::Abandoned;++abandoned;for(auto& r:procedures)if(r.id==p->procedure)++r.failures;}
    }
    return true;
}
void ProjectMemory::review(Tick now){
    if(now<0)throw std::invalid_argument("negative review clock");
    for(auto& p:projects){clock_check(p,now);if(p.live()&&p.goal.deadline&&now>p.goal.deadline){p.status=ProjectStatus::Expired;p.updated=now;++revision;}}
}
void ProjectMemory::validate(Tick now)const{
    for(const auto& t:profile.thresholds)t.validate();
    require_range(profile.discount_per_hour,0,1);require_range(profile.switch_margin,0,1);require_range(profile.romantic_interest,0,1);
    std::set<std::uint32_t> recipes;
    for(const auto& r:procedures){
        if(!r.id||!r.source||!recipes.insert(r.id).second||r.steps.empty()||r.steps.size()>16)throw std::runtime_error("invalid procedure");
        require_range(r.mastery,0,1);require_range(r.expected_gain,0,1);
        require_range(r.successes,1,1e12);require_range(r.failures,1,1e12);
        for(const auto& s:r.steps)if(unsigned(s.kind)>unsigned(StepKind::Finish)||s.success>=r.steps.size()||s.refused>=r.steps.size()||s.unavailable>=r.steps.size()||s.retry_delay<1000)throw std::runtime_error("invalid procedure step");
    }
    std::uint64_t previous=0;unsigned live_count=0;
    for(const auto& p:projects){
        if(p.searched_at<0)throw std::runtime_error("negative search time");
        valid_goal(p.goal);const auto* r=known(p.procedure);
        if(p.id<=previous||p.id>=next_id||!r||p.step>=r->steps.size()||unsigned(p.status)>unsigned(ProjectStatus::Expired)||p.created<0||p.updated<p.created||p.updated>now||p.retry_at<0)throw std::runtime_error("invalid personal project");
        previous=p.id;live_count+=p.live();
        if(!std::is_sorted(p.observations.begin(),p.observations.end())||std::adjacent_find(p.observations.begin(),p.observations.end())!=p.observations.end())throw std::runtime_error("project evidence duplicates");
    }
    if(live_count>8)throw std::runtime_error("too many live projects");
}
double continuation_value(const GoalCandidate& g,double discount){
    for(double v:{g.importance,g.expected_gain,g.success,g.remaining_cost,discount})require_range(v,0,1);
    if(g.delay<0)throw std::invalid_argument("negative delay");
    return g.importance*g.success*g.expected_gain*std::exp(-discount*double(g.delay)/3600000.)-g.remaining_cost;
}
double predicted_satiated_gain(double gain,double satiation){
    require_range(gain,-1,1);require_range(satiation,0,1);
    return gain>0?gain*(1-satiation):gain;
}
KnownProcedure familiar_procedure(GoalKind goal,std::uint64_t source){
    if(unsigned(goal)>=unsigned(GoalKind::Count)||!source)throw std::invalid_argument("procedure goal/source");
    KnownProcedure p;p.id=1+unsigned(goal);p.goal=goal;p.source=source;p.mastery=.85;
    std::vector<StepKind> sequence;
    switch(goal){
    case GoalKind::FindCompany:sequence={StepKind::Visit,StepKind::Contact,StepKind::Introduce,StepKind::SpendTime,StepKind::Finish};break;
    case GoalKind::MeetPerson:sequence={StepKind::Contact,StepKind::Invite,StepKind::Attend,StepKind::SpendTime,StepKind::Finish};break;
    case GoalKind::SeekIntimacy:sequence={StepKind::Contact,StepKind::Invite,StepKind::Attend,StepKind::SpendTime,StepKind::Intimacy,StepKind::Finish};break;
    case GoalKind::WorkShift:sequence={StepKind::Work,StepKind::Finish};break;
    case GoalKind::LearnSkill:sequence={StepKind::Study,StepKind::Finish};break;
    default:throw std::invalid_argument("procedure goal");
    }
    for(std::uint16_t i=0;i<sequence.size();++i){ProcedureStep s;s.kind=sequence[i];s.success=std::min<std::uint16_t>(i+1,std::uint16_t(sequence.size()-1));s.refused=s.unavailable=i;s.retry_delay=goal==GoalKind::WorkShift?300000:1800000;p.steps.push_back(s);}
    return p;
}
} // namespace life
