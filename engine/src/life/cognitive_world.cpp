#include "life/world.hpp"
#include "attention.hpp"
#include "subjective.hpp"
#include "valuation.hpp"
#include "life/deliberation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>


namespace life {
namespace {
constexpr std::uint64_t external_base=1000000,memory_base=2000000,project_base=3000000,reply_base=4000000;
std::uint32_t legacy_focus(std::uint8_t metric){static constexpr std::uint32_t ids[]={1,2,3,4,10,11,9,12,5,13,14};return ids[metric];}
SocialExpectation& contact(CognitiveState& c,Id id){auto it=std::lower_bound(c.contacts.begin(),c.contacts.end(),id,[](const auto& v,Id key){return v.person<key;});if(it==c.contacts.end()||it->person!=id){SocialExpectation s;s.person=id;it=c.contacts.insert(it,s);}return *it;}
void context_push(CognitiveState& c,ContextItem item,int cap){
 if(cap<2)return;
 // Focus and the current operand/result use the same bounded working context.
 if(c.context.size()>=std::size_t(cap)){if(c.context.size()>1)c.context.erase(c.context.begin()+1);else return;}
 c.context.push_back(item);c.peak_slots=std::max(c.peak_slots,int(c.context.size()));
}
}
void World::emit_thought(Actor& a,Thought t){
 auto& c=a.cog;t.actor=a.id;t.id=c.next_thought++;t.episode=c.episode;t.focus=c.focus;
 t.time=state_.now;t.spent=c.spent;t.slots=int(c.context.size());t.own_revision=a.mind.version;
 if(t.started>t.time)throw std::logic_error("future thought start");
 ++c.counts[std::size_t(t.kind)];c.recent.push_back(t);if(c.recent.size()>8)c.recent.erase(c.recent.begin());
 if(thought_logger_)thought_logger_(t);
}
void World::reply_observed(Actor& a,Id other,ReplyMessage message,std::uint64_t event){
 auto& c=a.cog;
 if(std::any_of(c.inbox.begin(),c.inbox.end(),[&](auto& r){return r.event==event;}))return;
 const auto it=std::find_if(c.contacts.begin(),c.contacts.end(),[&](const auto& r){return r.person==other;});
 if(it!=c.contacts.end()&&it->last_event==event)return;
 c.inbox.push_back({event,other,message,state_.now});++c.situation_version;c.review_at=std::min(c.review_at,state_.now);
}
void World::update_senses(Actor& a){
 auto& c=a.cog;const auto cap=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
 if(!a.alive||cap.gate==0){c.percepts.clear();return;}
 auto visible=local_people(a);std::sort(visible.begin(),visible.end());
 c.percepts.erase(std::remove_if(c.percepts.begin(),c.percepts.end(),[&](const auto& p){return !std::binary_search(visible.begin(),visible.end(),p.token);}),c.percepts.end());
 std::vector<int> indices(state_.actors.size()+1,-1);
 for(std::size_t i=0;i<c.percepts.size();++i)indices[c.percepts[i].token]=int(i);
 for(Id id:visible){
  int found=indices[id];
  if(found<0){
   const auto seq=c.observation_sequence++;const double u=std::clamp(state_.random.uniform("cog-exposure",a.id,seq),1e-12,1.-1e-12);
   Percept p;p.token=id;p.since=p.last=state_.now;p.exposure_id=seq;p.threshold=-std::log(u);c.percepts.push_back(p);found=int(c.percepts.size()-1);indices[id]=found;
  }
  auto it=c.percepts.begin()+std::ptrdiff_t(found);
  const double seconds=double(state_.now-it->last)/1000.;it->last=state_.now;
  const double lambda=2*(.25+.75*cap.current[0])*(.25+.75*cap.current[1]);
  it->evidence+=lambda*seconds;
  if(!it->noticed&&it->evidence>=it->threshold){it->noticed=true;++c.situation_version;Thought t;t.kind=ThoughtKind::Notice;t.origin=Origin::Observed;t.started=it->since;t.basis=it->exposure_id;t.metric=5;t.status=Truth::Confirmed;t.confidence=.6;emit_thought(a,t);}
  // Only the attended object acquires identity-level features. No true ID reaches the planner yet.
  if(!a.mind.projects.enabled&&c.focus_kind==TopicKind::External&&c.focus_person==id&&state_.now>=c.rebuild_until)
   it->feature_work+=cog04::feature_rate(.8,cap.current[0],cap.current[1],true)*seconds;
 }
 const auto raw=signals(a.body,a.biology);
 for(std::size_t j=0;j<8;++j){int band=0;for(double threshold:{.25,.55,.85})if(raw[j]>=threshold)++band;
  if(c.last_signal_band[j]!=band){c.last_signal_band[j]=band;++c.situation_version;if(band>=2)c.review_at=std::min(c.review_at,state_.now);}
 }
}
void World::cognition_start(Actor& a,Operation op){
 auto& c=a.cog;const auto cap=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
 if(cap.gate==0||c.budget<=0){c.active=false;c.operation=Operation::None;return;}
 if(op==Operation::SocialReply||op==Operation::SocialObserve){
  // A bounded template has a focus, an observed offer/outcome, and the known
  // interaction schema. An item binding is a fourth operand, not free memory.
  const auto& in=c.social_input;c.context.resize(std::min<std::size_t>(1,c.context.size()));
  context_push(c,{in.delivery,in.event,ThoughtKind::Reply,5,in.other,0,.8,Truth::Confirmed},cap.context);
  const auto& method=a.mind.social.methods[std::size_t(in.kind)];
  if(cap.context>=3)context_push(c,{method.source,method.source,ThoughtKind::Recall,5,in.other,method.expected_pleasure,method.confidence,method.mastery>=.6?Truth::Confirmed:Truth::Unknown},cap.context);
  if(in.object&&cap.context>=4)context_push(c,{in.object,in.delivery,ThoughtKind::Notice,5,in.other,0,.8,Truth::Confirmed},cap.context);
 }
 --c.budget;++c.spent;++c.operation_starts;c.operation=op;c.work=0;c.op_started=c.op_last=state_.now;
 c.rate=cog04::operation_rate(cap.current[4],true);c.due=state_.now+Tick(std::ceil(1000/c.rate));
 c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;
}
void World::cognition_tick(Actor& a){
 auto& c=a.cog;const Tick now=state_.now;const auto cap=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
 if(!a.alive||!state_.autonomy||cap.gate==0){
  if(c.operation!=Operation::None){++c.stale_ops;Thought t;t.kind=ThoughtKind::Stale;t.started=c.op_started;t.metric=c.focus_metric;t.detail="conscious_gate_closed";emit_thought(a,t);}
  c.operation=Operation::None;c.active=false;c.context.clear();return;
 }
 if(c.context.size()>std::size_t(cap.context)){
  c.context.resize(std::size_t(cap.context));++c.situation_version;
 }
 // Advance a running operation with the rate that held over the preceding interval.
 if(c.operation!=Operation::None){
  c.work=std::min(1.,c.work+c.rate*double(now-c.op_last)/1000.);c.op_last=now;
  c.rate=cog04::operation_rate(cap.current[4],true);
  if(c.work<1)c.due=now+Tick(std::ceil(1000*(1-c.work)/c.rate));
 }
 const bool new_basis=c.captured_mind!=a.mind.version||c.captured_situation!=c.situation_version;
 if(!c.active&&now<c.review_at&&!new_basis&&c.inbox.empty()&&a.mind.social.inbox.empty())return;
 // Rebuild candidates from accessible cues, not from a global planning search.
 const auto raw=signals(a.body,a.biology);std::array<double,9> cues{};
 for(unsigned j=0;j<4;++j)cues[j]=raw[j];
 cues[4]=1-a.leisure;cues[5]=1-a.social;
 cues[6]=unit(a.desire.value);cues[7]=state_.community.enabled?unit((250-a.mind.believed_money)/250):unit((8-a.mind.believed_money)/8);
 cues[8]=std::max({raw[4],raw[5],raw[6],raw[7]});
 std::vector<AttentionTopic> input;
 for(std::uint8_t j=0;j<9;++j){
  const auto id=std::uint64_t(j+1);const bool emergency=(j<4||j==8)&&cues[j]>=.85;
  double novelty=c.needs[j].status==Truth::Unknown?.5:0;
  if(a.mind.projects.enabled&&!emergency)cues[j]=a.mind.projects.profile.thresholds[j].activation(cues[j],c.focus==id);
  double goal=c.project_id&&j==7?.7:(c.focus==id?.25:0);
  const double priority=unit(cog04::priority(cues[j],cues[j],novelty,goal,0,cues[j])
    -(state_.community.enabled&&now<c.no_continuation_until[j]?.4:0));
  input.push_back({id,TopicKind::Internal,j,0,priority,cues[j],now,now+31000,c.situation_version,c.needs[j].source,emergency});
 }
 for(const auto& p:c.percepts)if(p.noticed&&!p.recognized){
  const double pr=cog04::priority(.3+.5*cues[5],.2,.8,.2,0,.7);
  input.push_back({external_base+p.token,TopicKind::External,5,p.token,pr,.2,now,now+1001,c.situation_version,p.exposure_id,false});
 }
 for(const auto& m:c.contacts)if(m.unpleasantness>.05&&cues[5]>.3&&now-m.last_reply<3600000){
  const double pr=cog04::priority(m.unpleasantness,cues[5],0,.3,m.unpleasantness,.2);
  input.push_back({memory_base+m.person,TopicKind::Memory,5,m.person,pr,cues[5],now,now+31000,c.situation_version,m.last_event,false});
 }
 if(c.project_id){double pr=cog04::priority(cues[7],.1,0,.8,0,.2);input.push_back({project_base+1,TopicKind::Project,7,0,pr,.1,now,now+31000,c.situation_version,c.project_id,false});}
 if(!c.inbox.empty()){
  const auto& r=c.inbox.front();double pr=cog04::priority(.85,.75,.8,.8,.4,.9);
  input.push_back({reply_base+r.event,TopicKind::External,5,r.person,pr,.75,r.at,now+31000,c.situation_version,r.event,false});
 }
 if(a.mind.social.enabled){
  if(!a.mind.social.inbox.empty()){
   const auto& m=a.mind.social.inbox.front();
   const double pr=cog04::priority(.95,.85,.8,.9,.4,.9);
   input.push_back({(1ull<<48)+m.delivery,TopicKind::Memory,5,m.other,pr,.85,m.at,now+31000,c.situation_version,m.delivery,false});
  }else if(in_conversation(a)&&!a.mind.social.active_event){
   const double need=std::max(.5,cues[5]);
   const double pr=cog04::priority(need,.25,.3,.8,0,.5);
   input.push_back({(2ull<<48)+conversation_id(a),TopicKind::Thought,5,conversation_partner(a),pr,.25,now,now+31000,c.situation_version,conversation_id(a),false});
  }
  if(a.mind.social.goal_object){
   const auto* item=a.mind.social.item(a.mind.social.goal_object);
   const bool unfinished=!a.mind.social.goal_used||(item&&item->holder==a.id&&item->owner!=a.id);
   if(unfinished)input.push_back({(3ull<<48)+a.mind.social.goal_object,TopicKind::Project,4,0,cog04::priority(.9,.5,.4,.9,0,.5),.5,now,now+31000,c.situation_version,a.mind.social.goal_object,false});
  }
 }
 if(a.mind.projects.enabled){
  for(const auto& p:a.mind.projects.projects)if(p.live()&&now>=p.retry_at&&now>=p.goal.not_before&&(!p.goal.deadline||now<=p.goal.deadline)){
   const auto* procedure=a.mind.projects.known(p.procedure);
   if(procedure&&p.step<procedure->steps.size()&&procedure->steps[p.step].kind==StepKind::Attend){
    const auto meet=std::find_if(a.appointments.begin(),a.appointments.end(),[&](const auto& m){return m.id==p.appointment;});
    if(meet!=a.appointments.end()&&now<meet->at-15*60000)continue;
   }
   const std::uint8_t metric=p.goal.kind==GoalKind::WorkShift?7:p.goal.kind==GoalKind::SeekIntimacy?6:5;
   const double pr=cog04::priority(p.goal.importance,.2,0,p.goal.importance,0,.4);
   input.push_back({(4ull<<48)+p.id,TopicKind::Project,metric,p.goal.target,pr,.2,now,now+31000,a.mind.projects.revision,p.goal.basis,false});
  }
 }
 append_self_topics(a,input);append_career_topics(a,input);
 c.candidates=bounded_topics(input,now,c.focus);
 if(c.candidates.empty())return;
 auto best=std::max_element(c.candidates.begin(),c.candidates.end(),[](const auto& x,const auto& y){
  if(x.emergency!=y.emergency)return !x.emergency;
  return x.priority!=y.priority?x.priority<y.priority:x.id>y.id;
 });
 auto old=std::find_if(c.candidates.begin(),c.candidates.end(),[&](const auto& t){return t.id==c.focus;});
 if(old==c.candidates.end()){c.focus=0;c.focus_priority=0;}else c.focus_priority=old->priority;
 const cog04::Focus f{c.focus,c.focus_priority,c.focus_since,c.rebuild_until};
 const cog04::Candidate next{best->id,best->priority,best->revision,best->expires,best->emergency,best->urgency};
 if(cog04::may_switch(f,next,now,cap.current[1],.5)){
  if(c.operation!=Operation::None){++c.stale_ops;Thought t;t.kind=ThoughtKind::Stale;t.started=c.op_started;t.metric=c.focus_metric;t.detail="focus_preempted";emit_thought(a,t);c.operation=Operation::None;}
  const double difference=c.focus_metric==best->metric?0:1;
  c.focus=best->id;c.focus_priority=best->priority;c.focus_metric=best->metric;c.focus_person=best->person;c.focus_kind=best->kind;c.focus_basis=best->basis;c.emergency=best->emergency;
  c.focus_since=now;c.rebuild_until=now+cog04::switch_ms(cap.current[6],cap.current[1],difference);++c.focus_switches;
  a.mind.attention={legacy_focus(c.focus_metric),c.focus_since,c.rebuild_until};
  c.context.clear();c.context.push_back({c.focus,c.focus_basis,ThoughtKind::Notice,c.focus_metric,c.focus_person,c.focus_priority,1,Truth::Confirmed});
 }
 if(now<c.rebuild_until||c.operation!=Operation::None)return;
 if(c.active&&c.budget==0){c.active=false;a.review=c.review_at;return;}
 if(!c.active){
  if(now<c.review_at&&!new_basis&&c.inbox.empty()&&a.mind.social.inbox.empty())return;
  c.active=true;++c.episode;++a.mind.decisions;c.budget=cog04::operation_budget(cap.current[4],true);c.spent=0;c.cursor=0;c.peak_slots=1;
  c.alternatives=std::max(1,cap.alternatives);c.options.clear();c.best=Decision{};c.best.place=a.place;c.best.path.clear();c.best.path.push_back(a.place);c.best_thought=0;
  c.context.clear();c.context.push_back({c.focus,c.focus_basis,ThoughtKind::Notice,c.focus_metric,c.focus_person,c.focus_priority,1,Truth::Confirmed});
  c.review_at=now+30000;a.review=c.review_at;
 }
 c.snapshot=personal_view(a.id);c.snapshot.episode=c.episode;
 // The snapshot initially contains sensations, not facts from the hidden body.
 if(start_self_cognition(a)||start_career_cognition(a))return;
 if(c.focus>=(1ull<<48)&&c.focus<(2ull<<48)&&!a.mind.social.inbox.empty()){
  c.social_input=a.mind.social.inbox.front();
  cognition_start(a,c.social_input.stage==SocialStage::Offer?Operation::SocialReply:Operation::SocialObserve);
 }
 else if(c.focus>=reply_base&&c.focus<(1ull<<48)&&!c.inbox.empty())cognition_start(a,Operation::Reply);
 else if(c.focus_kind==TopicKind::External&&c.focus_person)cognition_start(a,Operation::Recognize);
 else cognition_start(a,Operation::Interpret);
}
void World::cognition_complete(Actor& a,const Decision* forecast){
 auto& c=a.cog;if(c.operation==Operation::None)return;
 const auto op=c.operation;const auto cap=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep);
 const auto started=c.op_started;c.operation=Operation::None;
 if(!a.alive||cap.gate==0||c.captured_mind!=a.mind.version||c.captured_situation!=c.situation_version){
  ++c.stale_ops;Thought t;t.kind=ThoughtKind::Stale;t.started=started;t.metric=c.focus_metric;t.detail="own_input_changed";emit_thought(a,t);
  if(cap.gate==0||c.budget==0)c.active=false;
  else {
   c.snapshot=personal_view(a.id);c.snapshot.episode=c.episode;c.options.clear();c.cursor=0;
   c.best=Decision{};c.best.place=a.place;c.best.path.clear();c.best.path.push_back(a.place);c.best_thought=0;
   c.context.resize(std::min(std::size_t(1),c.context.size()));
   if(start_self_cognition(a)||start_career_cognition(a))return;
   if(!a.mind.social.inbox.empty()){
    c.social_input=a.mind.social.inbox.front();cognition_start(a,c.social_input.stage==SocialStage::Offer?Operation::SocialReply:Operation::SocialObserve);
   }else cognition_start(a,Operation::Interpret);
  }return;
 }
 ++c.completed_ops;
 if(complete_self_cognition(a,op,started)||complete_career_cognition(a,op,started))return;
 Thought t;t.started=started;t.metric=c.focus_metric;t.person=c.focus_person;t.basis=c.focus_basis;t.status=Truth::Confirmed;t.confidence=.8;
 auto finish=[&]{c.active=false;c.operation=Operation::None;a.review=c.review_at;c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;};
 if(op==Operation::SocialReply||op==Operation::SocialObserve){
  const auto message=c.social_input;
  auto& inbox=a.mind.social.inbox;
  auto it=std::find_if(inbox.begin(),inbox.end(),[&](const auto& x){return x.delivery==message.delivery;});
  if(it==inbox.end()){finish();return;}inbox.erase(it);
  t.method=Method::Social;t.person=message.other;t.debug_interaction=message.kind;t.debug_object=message.object;t.basis=message.delivery;
  if(op==Operation::SocialReply){
   auto personal=c.snapshot.social;
   if(message.item_present){personal.memory.report_item(message.item,message.other,message.delivery,message.at,true,FactOrigin::Observed);}
   personal.proposal=message.meeting;
   auto evaluation=evaluate_social(personal,message.kind,message.other,message.object,true);
   if(state_.life.enabled&&message.kind==Interaction::InviteMeeting){
    const auto& offer=message.meeting;
    if(a.employment.in_shift(offer.at)||a.employment.in_shift(offer.until-1))evaluation.score=-1;
    // Only the receiver's already learned appointments constrain this reply.
    // Cancelling an old commitment in favour of a new one needs a separate procedure.
    for(const auto& m:a.appointments)if(offer.at<m.until&&m.at<offer.until)evaluation.score=-1;
   }
   if(cap.context<(message.object?4:3)){evaluation.known=false;evaluation.score=-1;}
   t.kind=ThoughtKind::Reply;t.origin=Origin::Inferred;t.value=evaluation.score;
   t.status=evaluation.known?Truth::Confirmed:Truth::Unknown;
   t.detail=evaluation.known?(evaluation.score>0?"social_accept_specific_proposal":"social_decline_specific_proposal"):"social_unknown_interaction";
   t.debug_moral=evaluation.moral;t.debug_risk=evaluation.devaluation;
   t.debug_pleasure=evaluation.pleasure;t.debug_acceptance=evaluation.acceptance;t.debug_status_gain=evaluation.status_gain;t.debug_knowledge_source=evaluation.basis;
   emit_thought(a,t);a.mind.social.observe(message,a.mind.cognition,cap.current[3]);
   if(state_.self_enabled){Decision d;d.method=Method::Social;d.interaction=message.kind;
    d.self_domain=domain_of(message.kind);d.self_prediction=a.mind.self.predict(d.self_domain);
    d.goal_importance=.5;d.uncertainty=1-a.mind.social.methods[std::size_t(message.kind)].confidence;d.operations=c.spent;
    capture_self_decision(a,message.event,d);}
   pending_social_answers_.push_back({a.id,message,evaluation.known&&evaluation.score>0,evaluation});
  }else{
   t.kind=ThoughtKind::Reply;t.origin=Origin::Observed;t.value=message.pleasure;
   t.detail=std::string("social_observed_")+interaction_name(message.kind)+"_"+social_reason_name(message.reason);
   t.value=social_observe(a,message);if(message.stage==SocialStage::Declined)t.origin=Origin::Inferred;emit_thought(a,t);
  }
  ++a.mind.version;finish();c.review_at=state_.now+1000;return;
 }
 if(op==Operation::Interpret){
  c.snapshot.focus_metric=int(c.focus_metric);
  const auto m=c.focus_metric;const auto previous=c.needs[m];const double y=c.snapshot.need[m];
  const double mastery=a.mind.knowledge.get(100+m);
  if(mastery<.6){t.kind=ThoughtKind::Question;t.status=Truth::Unknown;t.confidence=0;t.detail="не_усвоено_правило_интерпретации";emit_thought(a,t);c.needs[m].status=Truth::Unknown;finish();return;}
  const double quality=unit(mastery*cap.gate);
  c.needs[m]=interpret_need(previous,y,quality,c.observation_sequence++,state_.now);
  const auto& estimate=c.needs[m];t.kind=ThoughtKind::Interpret;t.origin=Origin::Inferred;t.status=estimate.status;t.value=estimate.mean;t.observed=y;t.prior=previous.prior;t.confidence=estimate.confidence;t.basis=estimate.source;
  emit_thought(a,t);context_push(c,{c.next_thought-1,t.basis,t.kind,m,0,t.value,t.confidence,t.status},cap.context);
  for(std::size_t j=0;j<metric_count;++j){
   const double sensed=c.snapshot.need[j];
   c.snapshot.need[j]=c.needs[j].status==Truth::Confirmed?c.needs[j].mean:0;
   // Familiar emergency interoception cannot be erased by attending to another need.
   if((j<4||j==8)&&sensed>=.85&&a.mind.knowledge.get(100+Id(j))>=.6)c.snapshot.need[j]=std::max(c.snapshot.need[j],sensed);
  }
  if(estimate.status==Truth::Conflicting){t.kind=ThoughtKind::Question;t.detail="ощущение_и_ожидание_расходятся";emit_thought(a,t);finish();return;}
  if(c.budget>0)cognition_start(a,Operation::Recall);else finish();return;
 }
 if(op==Operation::Recognize){
  auto it=std::find_if(c.percepts.begin(),c.percepts.end(),[&](auto& p){return p.token==c.focus_person;});
  if((a.mind.projects.enabled||c.focus_kind!=TopicKind::External)&&it!=c.percepts.end())it->feature_work+=cog04::feature_rate(.8,cap.current[0],cap.current[1],true)*double(state_.now-started)/1000.;
  if(it==c.percepts.end()||it->feature_work<1){t.kind=ThoughtKind::Question;t.status=Truth::Unknown;t.person=0;t.detail="недостаточно_наблюдённых_признаков";emit_thought(a,t);finish();return;}
  // In this catalogue the familiar identity template is exact after extraction;
  // ambiguous competing identity hypotheses are explicitly not implemented.
  it->recognized=true;social_known_person(a,it->token,it->exposure_id);t.kind=ThoughtKind::Recall;t.origin=Origin::Observed;t.method=Method::Talk;t.basis=it->exposure_id;t.detail=a.mind.relation(it->token)?"знакомый_образ_после_наблюдения":"новый_локальный_образ";
  emit_thought(a,t);++c.situation_version;project(a);finish();c.review_at=state_.now+1000;return;
 }
 if(op==Operation::Reply){
  if(c.inbox.empty()){finish();return;}const auto r=c.inbox.front();c.inbox.erase(c.inbox.begin());auto& memory=contact(c,r.person);
  if(memory.last_event==r.event){finish();return;}
  if(r.message==ReplyMessage::Accepted){
   // Consent is observed, not a completed rewarding conversation.
   memory.accepted+=1;memory.last_event=r.event;memory.message=r.message;
   memory.last_reply=state_.now;memory.next_opportunity=state_.now;
   t.kind=ThoughtKind::Reply;t.origin=Origin::Reported;t.person=r.person;t.basis=r.event;
   t.value=0;t.confidence=1;t.detail="согласился_на_контакт;_результат_разговора_ещё_не_получен";
   emit_thought(a,t);++a.mind.version;finish();return;
  }
  const bool busy=r.message==ReplyMessage::Busy||r.message==ReplyMessage::Later;
  const double rejection=busy?.1+.2*c.rejection_bias:.3+.5*c.rejection_bias;
  const double unpleasant=unit((.1+.35*(1-a.social))*rejection);
  memory.refused+=1;memory.unpleasantness+=(unpleasant-memory.unpleasantness)*.25;memory.last_event=r.event;memory.message=r.message;memory.last_reply=state_.now;memory.next_opportunity=state_.now+(busy?300000:60000);
  t.kind=ThoughtKind::Reply;t.origin=Origin::Inferred;t.person=r.person;t.basis=r.event;t.value=unpleasant;t.confidence=busy?.85:.45;t.detail=busy?"сообщил_о_занятости;_предполагаю_неподходящее_время":"причина_неизвестна;_возможно_личное_нежелание";emit_thought(a,t);
  Appraisal app;app.significance=unit(.2+.6*(1-a.social));app.loss=unpleasant;app.rejection=rejection;app.disapproval=.5*rejection;
  a.affect.set(r.event,state_.now+60000,appraisal_targets(app,a.body));
  if(state_.self_enabled){
   OutcomeSignal signal;signal.id=state_.next_id++;signal.root=signal.id;signal.action=r.event;signal.method=Method::Talk;
   signal.at=state_.now;signal.other=r.person;signal.blocked=true;signal.feedback_observed=true;
   signal.goal_importance=unit(.2+.6*(1-a.social));signal.directness=cap.gate;
   signal.acceptance_observed=true;signal.accepted=0;signal.other_decision=busy?1:.4;
   signal.observed_obstacle=busy?.8:0;signal.action_feedback=.1;
   signal.observed_mask=1u<<10;signal.observed[10]=-unpleasant;publish_outcome(a,signal);
  }
  // A real interpreted reply teaches its own outcome once. Thought repetition does not.
  Episode e;e.id=state_.next_id++;e.method=Method::Talk;e.time=state_.now;e.outcome[10]=-unpleasant;e.observed=1u<<10;e.primary[0]=-unpleasant;e.primary_observed=1;e.features={{10000+r.person,r.event,1,1}};
  a.mind.learning.observe(e,a.mind.cognition,cap.current[3]);++a.mind.version;
  finish();return;
 }
 if(op==Operation::Recall){
  if(c.options.empty()){
   life_recall(a);populate_life_view(a,c.snapshot);
   const auto* goal_item=a.mind.social.item(a.mind.social.goal_object);
   const bool return_contact=a.mind.social.goal_object&&goal_item&&a.mind.social.goal_used&&goal_item->holder==a.id&&goal_item->owner!=a.id;
   const bool missing_owner=return_contact&&std::none_of(c.snapshot.social.perceived.begin(),c.snapshot.social.perceived.end(),[&](const auto& person){return person.id==goal_item->owner;});
   if(((a.mind.projects.enabled&&c.focus_metric==5&&c.snapshot.perceived_people.empty())||missing_owner)&&!in_conversation(a)){
    // Looking for company has an explicit paid perceptual substep. Remembering
    // the goal is not equivalent to recognizing everybody in the location.
    auto percept=std::find_if(c.percepts.begin(),c.percepts.end(),[](const auto& p){return p.noticed&&!p.recognized;});
    if(percept!=c.percepts.end()&&c.budget>0){c.focus_person=percept->token;cognition_start(a,Operation::Recognize);return;}
   }
   c.options=Planner::ideas(c.snapshot,state_.random);if(c.options.size()>std::size_t(c.alternatives))c.options.resize(c.alternatives);
  }
  if(c.cursor>=c.options.size()||c.budget<2){
   if(c.budget>0)cognition_start(a,Operation::Commit);else finish();return;
  }
  const auto& option=c.options[c.cursor];t.debug_interaction=option.interaction;t.debug_object=option.object;t.person=option.partner;t.kind=ThoughtKind::Recall;t.method=option.method;t.debug_destination=option.place;t.basis=c.needs[c.focus_metric].source;t.detail="известная_процедура";emit_thought(a,t);
  context_push(c,{c.next_thought-1,t.basis,t.kind,c.focus_metric,option.partner,0,.8,Truth::Confirmed},cap.context);
  c.snapshot.social.need_social=c.snapshot.need[5];c.snapshot.social.need_leisure=c.snapshot.need[4];
  // Contextual negative experience is retained in the signed forecast, not lost at max(0,Q).
  if(option.method==Method::Talk){
   const auto k=std::size_t(Method::Talk);c.snapshot.forecasts[k]=a.mind.priors[k];
   for(std::size_t j=0;j<metric_count;++j){const auto& q=a.mind.learning.q[k][j];if(q.count>0){const double confidence=q.confidence();c.snapshot.forecasts[k][j]=(1-confidence)*a.mind.priors[k][j]+confidence*q.mean;}}
  }
  if(option.method==Method::Talk&&option.partner){auto it=std::find_if(c.contacts.begin(),c.contacts.end(),[&](auto& x){return x.person==option.partner;});
   if(it!=c.contacts.end()){
    const double acceptance=it->accepted/(it->accepted+it->refused);
    c.snapshot.forecasts[std::size_t(Method::Talk)][5]=a.mind.priors[std::size_t(Method::Talk)][5]*acceptance;
    c.snapshot.forecasts[std::size_t(Method::Talk)][10]=-it->unpleasantness*(1-acceptance);
    c.snapshot.need[10]=1;
   }
  }
  cognition_start(a,Operation::Forecast);return;
 }
 if(op==Operation::Forecast){
  if(!forecast)throw std::logic_error("missing pure forecast");
  c.current=*forecast;t.debug_interaction=c.current.interaction;t.debug_object=c.current.object;
  t.debug_pleasure=c.current.social_evaluation.pleasure;t.debug_acceptance=c.current.social_evaluation.acceptance;t.debug_status_gain=c.current.social_evaluation.status_gain;t.debug_knowledge_source=c.current.social_evaluation.basis;
  t.debug_destination=c.current.place;t.debug_moral=c.current.moral;t.debug_risk=c.current.risk;t.debug_resource=c.current.resource_cost;t.debug_time=c.current.time_cost;
  t.kind=ThoughtKind::Forecast;t.origin=Origin::Imagined;t.method=c.current.method;t.person=c.current.partner;t.value=c.current.score;t.basis=c.next_thought-1;
  t.detail=c.current.method==Method::Social||c.current.method==Method::UseObject?"social_forecast_from_own_knowledge_attitude_norms":c.current.method==Method::Work?"отложенная_выплата_после_смены":"ожидаемый_исход_из_личной_модели";
  if(state_.self_enabled&&state_.self_effects&&c.current.self_costs.total()>0&&!c.current.path.empty()){
   const auto& self=c.current.self_prediction;
   const double loss=unit(c.current.goal_importance*(1-self.efficacy)+c.current.risk);
   auto app=self_appraisal(self,c.current.goal_importance,loss,c.current.uncertainty);
   // One replaceable prospective cause per domain. Re-reading the same
   // forecast neither multiplies emotional causes nor trains a belief.
   a.affect.set(0x6f00000000000000ull|std::uint64_t(c.current.self_domain),state_.now+60000,appraisal_targets(app,a.body));
  }

  emit_thought(a,t);c.current_thought=c.next_thought-1;
  context_push(c,{c.current_thought,t.basis,t.kind,c.focus_metric,c.current.partner,t.value,.8,Truth::Confirmed},cap.context);
  if(c.budget==1){
   // A known one-candidate procedure needs no comparison with other candidates.
   // Interpretation, recall, forecast and commit still cost four operations.
   if(!c.current.path.empty()&&c.current.score>c.best.score){c.best=c.current;c.best_thought=c.current_thought;}
   ++c.cursor;cognition_start(a,Operation::Commit);
  }else if(c.budget>1)cognition_start(a,Operation::Compare);else finish();return;
 }
 if(op==Operation::Compare){
  if(!c.current.path.empty()&&c.current.score>c.best.score){c.best=c.current;c.best_thought=c.current_thought;}
  t.debug_destination=c.best.place;t.debug_moral=c.best.moral;t.debug_risk=c.best.risk;t.debug_resource=c.best.resource_cost;t.debug_time=c.best.time_cost;
  t.debug_interaction=c.best.interaction;t.debug_object=c.best.object;t.person=c.best.partner;
  t.kind=ThoughtKind::Compare;t.method=c.best.method;t.value=c.best.score;t.basis=c.current_thought;emit_thought(a,t);++c.cursor;
  c.context.resize(1);context_push(c,{c.best_thought,c.current_thought,ThoughtKind::Forecast,c.focus_metric,c.best.partner,c.best.score,.8,Truth::Confirmed},cap.context);
  if(c.budget>0)cognition_start(a,c.cursor<c.options.size()&&c.budget>=4?Operation::Recall:Operation::Commit);else finish();return;
 }
 if(op==Operation::Commit){
  auto d=c.best;d.operations=c.spent;d.peak_slots=c.peak_slots;d.alternatives=int(c.cursor);
  const bool interruptible=a.action.phase==Phase::Running&&(a.action.method==Method::Work||a.action.method==Method::Leisure||a.action.method==Method::Talk||a.action.method==Method::Rest||a.action.method==Method::Idle);
  const auto raw=signals(a.body,a.biology);const bool emergency=*std::max_element(raw.begin(),raw.end())>=.85;
  const bool nested=d.method==Method::Social&&in_conversation(a);
  const bool use_item=d.method==Method::UseObject;
  const bool parallel=a.mind.projects.enabled&&d.method==Method::Talk&&d.place==a.place;
  const bool project_switch=a.mind.projects.enabled&&d.project&&d.project!=a.action.project&&interruptible&&d.score>a.action.chosen_score+a.mind.projects.profile.switch_margin;
  // An ongoing, voluntarily accepted contact has a finite switching cost. It does
  // not seize the actor: urgent bodily signals or a better alternative can interrupt.
  bool contact_allows=true;
  if(a.mind.projects.enabled&&a.conversation.id&&!nested&&!parallel&&!emergency){
   const bool leaves=d.place!=a.place;
   const bool incompatible=!combine_resources(activity_resources(d.method,leaves,false),activity_resources(Method::Talk)).allowed;
   if(leaves||incompatible){
    const double contact_value=.12+.15*c.snapshot.need[5];
    contact_allows=d.score>contact_value+a.mind.projects.profile.switch_margin;
   }
  }
  if(a.mind.projects.enabled&&c.snapshot.social.occupied&&d.method==Method::Idle&&!emergency)contact_allows=false;
  bool change=contact_allows&&(parallel||project_switch||nested||a.action.phase==Phase::Idle||(use_item&&a.action.method==Method::Talk&&a.action.phase==Phase::Running)||(interruptible&&d.method!=Method::Idle&&d.method!=a.action.method&&(emergency||c.snapshot.need[c.focus_metric]>.75)));
  SwitchAssessment assessment;
  if(interruptible&&!parallel&&!nested&&!use_item){
   assessment=assess_interruption(a.action.method,a.action.chosen_score,d.method,d.score,c.snapshot.need,a.mind.projects.profile.switch_margin);
   change=contact_allows&&assessment.change;
  }
  // An active primary or secondary action is not a new executable candidate.
  if(d.method==Method::Work&&a.action.method==Method::Work&&a.action.phase==Phase::Running)change=false;
  if(parallel&&a.conversation.id)change=false;
  t.debug_destination=d.place;t.debug_moral=d.moral;t.debug_risk=d.risk;t.debug_resource=d.resource_cost;t.debug_time=d.time_cost;
  t.debug_interaction=d.interaction;t.debug_object=d.object;t.person=d.partner;
  t.kind=ThoughtKind::Intent;t.method=d.method;t.value=d.score;t.basis=c.best_thought;t.detail=change?"команда_исполнителю":"текущее_действие_сохранено";
  if(interruptible&&!parallel&&!nested)t.detail+=std::string(";")+switch_reason_name(assessment.reason);
  if(!change)++c.retained_decisions;else ++c.executed_decisions;
  emit_thought(a,t);++metrics_.decisions;
  if(change){
   if(state_.self_enabled&&state_.self_effects){
    const auto app=self_appraisal(d.self_prediction,d.goal_importance,unit(d.risk+d.uncertainty*.3),d.uncertainty);
    // One current contemplated-intent cause; repeated read does not stack fear.
    a.affect.set((9ull<<48)+a.id,state_.now+30000,appraisal_targets(app,a.body));
   }
   if(a.action.method==Method::Work&&d.method!=Method::Work&&c.project_id){c.project_paused=true;t.kind=ThoughtKind::Pause;emit_thought(a,t);}
   if(d.method==Method::Work){if(!c.project_id)c.project_id=c.next_thought;if(c.project_paused){t.kind=ThoughtKind::Resume;emit_thought(a,t);}c.project_paused=false;}
   pending_intents_.emplace_back(a.id,d);
  }
  if(state_.community.enabled&&c.focus_kind==TopicKind::Internal&&c.focus_metric<9&&d.method==Method::Idle&&!c.best_thought){
   // The completed comparison found no worthwhile known continuation. This
   // temporarily lowers this topic's salience, not its physiological urgency.
   // Another observed emergency retains its emergency flag and can be examined.
   c.no_continuation_until[c.focus_metric]=state_.now+60000;++c.fruitless_decisions;
   t.kind=ThoughtKind::Question;t.detail="нет_полезного_известного_продолжения;_рассмотрю_другую_тему";emit_thought(a,t);
  }
  finish();return;
 }
}
}
