// Matched causal scenarios for NORM-MEMORY-0.1.  The pair differs only in
// explicit history content; both copies then use the production World planner.
#include "life/world.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace life;

namespace {
struct CaseDefinition {
    NormKey key{};
    Id history_actor=1;
    Tick duration=12*3600000;
    int expected_direction=1;
    const char* primary_metric="";
    bool approval_history=false;
};

struct Result {
    std::string history_initial_hash,hash;
    double primary=0;
    std::uint64_t offered=0,accepted=0,completed=0,declined=0;
    std::uint64_t norm_published=0,norm_integrated=0,norm_duplicates=0,norm_dropped=0;
    std::uint64_t self_updates=0,acceptance_updates=0;
    double money_before=0,money_after=0;
};

std::uint64_t number(std::string_view text){
    std::uint64_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{}||end!=text.data()+text.size())throw std::invalid_argument("invalid unsigned integer");
    return value;
}

std::ofstream checked(const std::filesystem::path& path){
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("cannot write "+path.string());
    out.exceptions(std::ios::badbit|std::ios::failbit);out.precision(17);return out;
}

CaseDefinition definition(const std::string& name){
    constexpr Id group=91;
    constexpr std::uint32_t context=191;
    static const std::map<std::string,CaseDefinition> cases{
        {"clothing_history_pair",{{practice_id(NormPractice::ClothingTier),group,context,0,2},1,36*3600000,1,"actor1_garment_tier",true}},
        {"privacy_history_pair",{{practice_id(NormPractice::Privacy),0,0,0,1},1,2*3600000,-1,"actor1_disclosures",false}},
        {"return_history_pair",{{practice_id(NormPractice::Promise),0,0,0,0},1,2*3600000,1,"actor1_returns",false}},
        {"help_history_pair",{{practice_id(NormPractice::Help),group,context,0,0},2,2*3600000,1,"actor1_gifts_received",true}},
        {"work_history_pair",{{practice_id(NormPractice::Work),group,context,0,0},1,18*3600000,1,"actor1_work_hours",true}},
    };
    const auto it=cases.find(name);if(it==cases.end())throw std::invalid_argument("unknown --case: "+name);return it->second;
}

void add_known_context(World& world,const CaseDefinition& def){
    if(!def.approval_history)return;
    auto& state=world.edit_for_test();
    auto& actor=state.actors.at(def.history_actor-1);
    const Id audience=def.history_actor==1?2:1;
    const auto practice=NormPractice(def.key.practice);
    const Id group_place=practice==NormPractice::Work?actor.employment.workplace:
                         practice==NormPractice::ClothingTier?Id{2}:actor.place;
    if(!group_place)throw std::runtime_error("norm context place is unknown");
    actor.mind.norm_context.groups.push_back(
        {def.key.local_group,def.key.context,group_place,0x4100000000001000ull,
         {{actor.id,0x4100000000001001ull,state.now},
          {audience,0x4100000000001002ull,state.now}}});
    actor.mind.norm_context.goals.push_back(
        {0x4100000000001010ull,0x4100000000001011ull,def.key.local_group,1,1,true});
    ++actor.mind.norm_context.revision;
}

void same_room(World& world){
    auto& state=world.edit_for_test();
    for(std::size_t i=0;i<2;++i){state.actors[i].place=2;state.actors[i].action={};state.actors[i].social=.1;state.actors[i].leisure=.8;}
    if(!world.conversation_for_test(1,2))throw std::runtime_error("failed to establish authored initial conversation");
}

void prepare_fixture(World& world,const std::string& name){
    auto& state=world.edit_for_test();
    auto& actor=state.actors[0];
    if(name=="clothing_history_pair"){
        auto garment=std::find_if(state.social.objects.begin(),state.social.objects.end(),[&](const auto& object){return object.id==actor.equipment.garment;});
        if(garment==state.social.objects.end())throw std::runtime_error("fixture garment missing");
        garment->condition=.18;actor.mind.civil.garment_condition=.18;actor.mind.civil.desired_tier=0;
        state.ledger.initial_money+=500-actor.money;actor.money=actor.mind.believed_money=500;
        auto& question=actor.mind.civil.questions.ensure(QuestionKind::Clothing,7,state.next_id++,state.now);question.actionable=true;
    }else if(name=="privacy_history_pair"){
        same_room(world);
        Information information;information.id=state.next_id++;information.subject=8;information.kind=NewsKind::Opinion;
        information.material=.8;information.importance=.9;information.cited_source=2;information.disclosure=Disclosure::Entrusted;
        actor.mind.social.community.receive(information,0,state.next_id++,state.now);
    }else if(name=="return_history_pair"){
        same_room(world);
        WorldObject object;object.id=Id(state.next_id++);object.owner=2;object.holder=1;object.place=2;object.kind=ObjectKind::Book;object.active_loan=state.next_id++;
        state.social.objects.push_back(object);state.social.loans.push_back({object.active_loan,object.id,2,1,state.now+3600000,false});
        ItemMemory known;known.id=object.id;known.owner=2;known.holder=1;known.place=2;known.kind=object.kind;
        known.owner_status=known.holder_status=Truth::Confirmed;known.origin=FactOrigin::Agreement;known.source=object.active_loan;known.uses=1;
        for(auto* person:{&state.actors[0],&state.actors[1]}){
            person->mind.social.items.push_back(known);
            std::sort(person->mind.social.items.begin(),person->mind.social.items.end(),[](const auto& a,const auto& b){return a.id<b.id;});
            person->mind.social.methods[std::size_t(Interaction::ReturnItem)]={.9,.2,.9,.9,state.next_id++};
        }
    }else if(name=="help_history_pair"){
        same_room(world);
        auto& receiver=state.actors[0];auto& donor=state.actors[1];
        state.ledger.initial_money-=receiver.money;receiver.money=receiver.mind.believed_money=0;
        state.ledger.initial_money+=300-donor.money;donor.money=donor.mind.believed_money=300;
        ResourceFact salary{state.next_id++,2,0,2,ResourceKind::IncomeHourly,30,1,state.now,true};
        ResourceFact spending{state.next_id++,2,0,2,ResourceKind::LeisureSpending,25,1,state.now,true};
        receiver.mind.social.community.resources.receive(salary,2,state.now);
        receiver.mind.social.community.resources.receive(spending,2,state.now);
        auto& question=receiver.mind.civil.questions.ensure(QuestionKind::Money,2,salary.root,state.now);question.actionable=true;
    }else if(name=="work_history_pair"){
        actor.body.energy=actor.body.water=.9;actor.body.sleep=.05;actor.leisure=actor.social=.7;
    }
}

std::vector<NormObservation> history(const World& world,const CaseDefinition& def,double target,std::uint64_t source_base){
    std::vector<NormObservation> out;out.reserve(def.approval_history?32:24);
    for(std::uint64_t i=0;i<24;++i){
        NormObservation observation;observation.key=def.key;
        const Id speaker=def.approval_history?(def.history_actor==1?2:1):def.history_actor;
        observation.source={source_base+i,source_base+i,1,speaker,NormOrigin::Historical,true};
        observation.at=0;observation.observed_actor=def.history_actor;observation.quality=observation.confidence=observation.reliability=observation.dose=1;
        observation.value=target;observation.applicable=observation.value_known=observation.outcome_window_complete=true;
        if(def.approval_history){
            observation.channel=NormChannel::Approval;
            observation.approval=target>0?ApprovalValue::Approve:ApprovalValue::Disapprove;
        }else{
            observation.channel=NormChannel::PersonalPrinciple;observation.accepted_argument=true;observation.plasticity=1;
            observation.principle_aspect=def.key.practice;observation.principle_applicability=1;
            observation.principle_exception=0;observation.principle_severity=1;
        }
        out.push_back(observation);
    }
    if(def.approval_history){
        const auto& events=world.state().history;
        for(const auto& event:events){
            if(out.size()==32)break;
            if(event.a!=def.history_actor&&event.b!=def.history_actor)continue;
            const Id witness=event.a==def.history_actor?event.b:event.a;
            if(!witness)continue;
            NormObservation observation;observation.key=def.key;
            observation.source={event.id,event.id,1,witness,NormOrigin::Historical,true};
            observation.at=0;observation.observed_actor=def.history_actor;
            observation.quality=observation.confidence=observation.reliability=observation.dose=1;
            observation.value=1;observation.channel=NormChannel::Detection;
            observation.applicable=observation.value_known=observation.outcome_window_complete=true;
            out.push_back(observation);
        }
        if(out.size()<26)throw std::runtime_error("fixture lacks two accessible historical witnesses");
    }
    return out;
}

double total_money(const World& world){double total=0;for(const auto& actor:world.state().actors)total+=actor.money;return total;}

double primary_metric(const World& world,const std::string& name){
    const auto& state=world.state();const auto& actor=state.actors[0];
    if(name=="clothing_history_pair")return actor.mind.civil.garment_tier;
    if(name=="privacy_history_pair")return std::count_if(state.social.events.begin(),state.social.events.end(),[](const auto& event){return event.initiator==1&&event.kind==Interaction::ShareNews&&event.phase==SocialPhase::Completed;});
    if(name=="return_history_pair")return std::count_if(state.social.events.begin(),state.social.events.end(),[](const auto& event){return event.initiator==1&&event.kind==Interaction::ReturnItem&&event.phase==SocialPhase::Completed;});
    if(name=="help_history_pair")return actor.mind.civil.gifts_received;
    if(name=="work_history_pair")return actor.economy.time_ms[std::size_t(TimeUse::Work)]/3600000.;
    throw std::logic_error("metric case");
}

Result run_variant(World world,const CaseDefinition& def,const std::string& case_name,const std::string& variant,double target,const std::filesystem::path& root,std::uint64_t source_base){
    const auto directory=root/variant;std::filesystem::create_directories(directory);
    world.bootstrap_norm_history(def.history_actor,history(world,def,target,source_base));
    Result result;result.history_initial_hash=world.hash();
    auto events=checked(directory/"events.jsonl");auto norms=checked(directory/"norms.jsonl");auto thoughts=checked(directory/"thoughts.jsonl");auto self=checked(directory/"self.jsonl");
    result.money_before=total_money(world);
    world.set_logger([&](const EventLog& event){if(event.actor==def.history_actor)events<<"{\"ms\":"<<event.time<<",\"actor\":"<<event.actor<<",\"event\":"<<event.event<<",\"method\":\""<<method_name(event.method)<<"\",\"kind\":\""<<event.kind<<"\",\"result\":\""<<event.result<<"\",\"interaction\":\""<<interaction_name(event.interaction)<<"\"}\n";});
    world.set_norm_logger([&](const NormTrace& trace){if(trace.actor==def.history_actor)norms<<norm_trace_json(trace)<<'\n';});
    world.set_thought_logger([&](const Thought& thought){if(thought.actor==def.history_actor)thoughts<<thought_json(thought)<<'\n';});
    world.set_self_logger([&](const SelfUpdateTrace& trace){if(trace.actor!=def.history_actor)return;++result.self_updates;if(trace.axis==SelfAxis::Acceptance)++result.acceptance_updates;self<<"{\"actor\":"<<trace.actor<<",\"source\":"<<trace.source<<",\"domain\":\""<<self_domain_name(trace.domain)<<"\",\"axis\":\""<<self_axis_name(trace.axis)<<"\"}\n";});
    world.norm_context_for_test(def.history_actor);world.run_seconds(def.duration/1000);world.validate();
    result.hash=world.hash();result.primary=primary_metric(world,case_name);result.money_after=total_money(world);
    for(const auto& event:world.state().social.events){++result.offered;if(event.phase==SocialPhase::Accepted||event.phase==SocialPhase::Completed)++result.accepted;if(event.phase==SocialPhase::Completed)++result.completed;if(event.phase==SocialPhase::Declined)++result.declined;}
    for(const auto& actor:world.state().actors){result.norm_published+=actor.cog.norm.published;result.norm_integrated+=actor.cog.norm.integrated;result.norm_duplicates+=actor.cog.norm.duplicates;result.norm_dropped+=actor.cog.norm.dropped;}
    auto summary=checked(directory/"summary.json");summary<<world.summary_json()<<'\n';
    auto norm_state=checked(directory/"norm-state.json");norm_state<<world.norm_report_json()<<'\n';
    auto recovery=checked(directory/"recovery-state.json");recovery<<world.recovery_report_json()<<'\n';
    auto career=checked(directory/"career-state.json");career<<world.career_report_json()<<'\n';
    auto self_state=checked(directory/"self-state.json");self_state<<world.self_report_json()<<'\n';
    world.save((directory/"final.save").string());return result;
}

void result_json(std::ostream& out,const Result& value){
    out<<"{\"history_initial_hash\":\""<<value.history_initial_hash<<"\",\"hash\":\""<<value.hash<<"\",\"primary\":"<<value.primary<<",\"offered\":"<<value.offered<<",\"accepted\":"<<value.accepted<<",\"completed\":"<<value.completed<<",\"declined\":"<<value.declined
       <<",\"norm_published\":"<<value.norm_published<<",\"norm_integrated\":"<<value.norm_integrated<<",\"norm_duplicates\":"<<value.norm_duplicates<<",\"norm_dropped\":"<<value.norm_dropped
       <<",\"self_updates\":"<<value.self_updates<<",\"acceptance_updates\":"<<value.acceptance_updates<<",\"money_before\":"<<value.money_before<<",\"money_after\":"<<value.money_after<<'}';
}
}

int main(int argc,char** argv){
    try{
        std::string case_name;std::filesystem::path out;std::uint64_t seed=42;Tick duration_override=0;
        for(int i=1;i<argc;++i){const std::string key=argv[i];if(i+1>=argc)throw std::invalid_argument("missing value for "+key);const std::string value=argv[++i];
            if(key=="--case")case_name=value;else if(key=="--seed")seed=number(value);else if(key=="--out")out=value;else if(key=="--seconds")duration_override=Tick(number(value))*1000;else throw std::invalid_argument("unknown argument: "+key);}
        if(case_name.empty()||out.empty())throw std::invalid_argument("norm_behavior_scenarios --case NAME --seed N --out DIRECTORY");
        auto def=definition(case_name);if(duration_override)def.duration=duration_override;auto base=World::generate(seed,16);base.configure_recovery();NormProfile profile;profile.mode=NormMode::Enabled;base.configure_norm_memory(profile);prepare_fixture(base,case_name);add_known_context(base,def);
        const auto common_physical_initial_hash=base.hash();std::filesystem::create_directories(out);
        const auto control=run_variant(base,def,case_name,"control",0,out,0x4100000000000000ull);
        const auto treatment=run_variant(base,def,case_name,"treatment",1,out,0x4100000000000000ull);
        auto pair=checked(out/"pair.json");pair<<"{\"schema\":\"norm-behavior-pair-0.1\",\"case\":\""<<case_name<<"\",\"seed\":"<<seed<<",\"population\":16,\"duration_ms\":"<<def.duration
            <<",\"same_physical_initial_state\":true,\"common_initial_hash\":\""<<common_physical_initial_hash<<"\",\"common_physical_initial_hash\":\""<<common_physical_initial_hash<<"\",\"history_channel\":\""<<(def.approval_history?"approval":"personal_principle")<<"\",\"norm_key\":["<<def.key.practice<<','<<def.key.local_group<<','<<def.key.context<<','<<def.key.actor_role<<','<<def.key.variant<<"],\"primary_metric\":\""<<def.primary_metric<<"\",\"expected_direction\":"<<def.expected_direction<<",\"control\":";result_json(pair,control);pair<<",\"treatment\":";result_json(pair,treatment);
        const double raw=treatment.primary-control.primary;pair<<",\"raw_delta\":"<<raw<<",\"signed_delta\":"<<def.expected_direction*raw<<"}\n";
        std::cout<<case_name<<" seed="<<seed<<" control="<<control.primary<<" treatment="<<treatment.primary<<" signed_delta="<<def.expected_direction*raw<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<"ERROR: "<<error.what()<<'\n';return 1;}
}
