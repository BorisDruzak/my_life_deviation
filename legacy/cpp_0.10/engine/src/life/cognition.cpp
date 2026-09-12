#include "life/cognition.hpp"
#include "subjective.hpp"
#include "attention.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <map>
#include <stdexcept>

namespace life {
const char* thought_kind_name(ThoughtKind k){
 static constexpr const char* n[]={"notice","interpret","recall","forecast","compare","intent","question","interpret_reply","stale","pause_project","resume_project"};
 if(std::size_t(k)>=std::size(n))throw std::invalid_argument("thought kind");
 return n[std::size_t(k)];
}
const char* metric_name(std::uint8_t m){
 static constexpr const char* n[]={"еда","вода","сон","отдых","досуг","общение","влечение","средства","безопасность","забота","приятность"};
 if(m>=std::size(n))throw std::invalid_argument("thought metric");
 return n[m];
}
namespace {
const char* method_ru(Method method) {
 switch (method) {
 case Method::Idle: return "ожидание";
 case Method::Eat: return "поесть";
 case Method::Drink: return "попить";
 case Method::Sleep: return "поспать";
 case Method::Rest: return "отдохнуть";
 case Method::Leisure: return "досуг";
 case Method::Talk: return "поговорить";
 case Method::PrivateIntimacy: return "индивидуальное удовлетворение";
 case Method::AcquireFood: return "приобрести еду";
 case Method::TakeFood: return "взять еду без разрешения";
 case Method::Work: return "работать";
 case Method::Inspect: return "осмотреть источник";
 case Method::Shelter: return "укрыться";
 case Method::Social: return "взаимодействовать";
 case Method::UseObject: return "использовать предмет";
 case Method::Study: return "изучать профессиональные правила";
 default: throw std::invalid_argument("method localization");
 }
}
}
std::string thought_text(const Thought& t){
 std::ostringstream s;s.precision(3);
 if(t.method==Method::Social||t.method==Method::UseObject){
  if(t.kind==ThoughtKind::Forecast||t.detail=="social_accept_specific_proposal"||t.detail=="social_decline_specific_proposal"){
   s<<"Рассматриваю «"<<interaction_ru(t.debug_interaction)<<"»: ожидаемая приятность "<<t.debug_pleasure<<", собственная моральная цена "<<t.debug_moral<<", оценка "<<t.value;
  }else if(t.kind==ThoughtKind::Recall)s<<"Вспомнил известный способ «"<<interaction_ru(t.debug_interaction)<<"»; предмет "<<t.debug_object;
  else if(t.kind==ThoughtKind::Intent)s<<"Выбрал «"<<interaction_ru(t.debug_interaction)<<"» по завершённому прогнозу "<<t.basis<<"; "<<t.detail;
  else s<<interaction_ru(t.debug_interaction)<<"; "<<t.detail<<"; пережитый/рассчитанный результат "<<t.value<<"; предмет "<<t.debug_object;
  return s.str();
 }
 switch(t.kind){
 case ThoughtKind::Notice:s<<"Заметил присутствие человека; личность ещё не установлена.";break;
 case ThoughtKind::Interpret:s<<"Оцениваю собственное состояние: "<<metric_name(t.metric)<<"; ";
   if(t.status==Truth::Conflicting)s<<"ощущение расходится с прежним представлением.";
   else if(t.status==Truth::Unknown)s<<"недостаточно оснований.";
   else s<<"выраженность "<<t.value<<", уверенность "<<t.confidence<<'.';
   break;
 case ThoughtKind::Recall:
   if(t.origin==Origin::Observed&&t.person)s<<"Распознал локальный образ человека "<<t.person<<" после получения признаков: "<<t.detail<<'.';
   else s<<"Вспомнил известный способ: "<<method_ru(t.method)<<". Это возможность, а не выполненное действие.";
   break;
 case ThoughtKind::Forecast:s<<"Ожидаемая оценка "<<method_ru(t.method)<<": "<<t.value<<"; результат ещё не получен.";break;
 case ThoughtKind::Compare:s<<"Сравнил рассмотренный вариант; лучший сейчас: "<<method_ru(t.method)<<" ("<<t.value<<").";break;
 case ThoughtKind::Intent:
   if(!t.basis)s<<"Пригодный вариант не выбран; сохраняю текущее действие или ожидаю.";
   else s<<"Решение: "<<method_ru(t.method)<<"; основание — завершённый прогноз "<<t.basis<<'.';
   break;
 case ThoughtKind::Question:s<<"Не хватает основания для вывода: "<<t.detail<<'.';break;
 case ThoughtKind::Reply:s<<"После ответа человека "<<t.person<<": "<<t.detail<<"; неприятность "<<t.value<<". Это моя интерпретация, не его скрытый мотив.";break;
 case ThoughtKind::Stale:s<<"Посылки изменились до завершения операции; прежний вывод не принимаю.";break;
 case ThoughtKind::Pause:s<<"Приостанавливаю работу ради текущей потребности; цель дохода сохраняю.";break;
 case ThoughtKind::Resume:s<<"Возвращаюсь к работе: ожидаемый будущий результат остаётся значимым.";break;
 default:throw std::invalid_argument("thought kind");
 }
 return s.str();
}
namespace {
std::string quoted(const std::string& x){std::ostringstream s;s<<'"';for(unsigned char c:x){switch(c){case '"':s<<"\\\"";break;case '\\':s<<"\\\\";break;case '\n':s<<"\\n";break;case '\r':s<<"\\r";break;case '\t':s<<"\\t";break;default:if(c<32){s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;}else s<<char(c);}}s<<'"';return s.str();}
}
std::string thought_json(const Thought& t){
 std::ostringstream s;s.precision(17);
 s<<"{\"ms\":"<<t.time<<",\"started_ms\":"<<t.started<<",\"actor\":"<<t.actor<<",\"thought\":"<<t.id<<",\"episode\":"<<t.episode<<",\"focus\":"<<t.focus<<",\"kind\":"<<quoted(thought_kind_name(t.kind))<<",\"origin\":"<<unsigned(t.origin)<<",\"status\":"<<unsigned(t.status)<<",\"metric\":"<<quoted(metric_name(t.metric))<<",\"person\":"<<t.person<<",\"method\":"<<quoted(method_name(t.method))<<",\"destination\":"<<t.debug_destination<<",\"moral_cost\":"<<t.debug_moral<<",\"risk_cost\":"<<t.debug_risk<<",\"resource_cost\":"<<t.debug_resource<<",\"time_cost\":"<<t.debug_time<<",\"object\":"<<t.debug_object<<",\"interaction\":"<<quoted(interaction_name(t.debug_interaction))<<",\"expected_pleasure\":"<<t.debug_pleasure<<",\"expected_acceptance\":"<<t.debug_acceptance<<",\"expected_status_gain\":"<<t.debug_status_gain<<",\"knowledge_source\":"<<t.debug_knowledge_source<<",\"basis\":"<<t.basis<<",\"own_revision\":"<<t.own_revision<<",\"value\":"<<t.value<<",\"confidence\":"<<t.confidence<<",\"observation\":"<<t.observed<<",\"prior\":"<<t.prior<<",\"spent\":"<<t.spent<<",\"slots\":"<<t.slots<<",\"detail\":"<<quoted(t.detail)<<",\"text\":"<<quoted(thought_text(t))<<'}';return s.str();
}
std::vector<AttentionTopic> bounded_topics(std::span<const AttentionTopic> input,Tick now,std::uint64_t focus){
 std::map<std::uint64_t,AttentionTopic> latest;
 for(const auto& t:input){require_range(t.priority,0,1);require_range(t.urgency,0,1);if(!t.id||t.metric>=metric_count)throw std::invalid_argument("invalid topic");
   auto it=latest.find(t.id);if(it==latest.end()||it->second.revision<t.revision)latest[t.id]=t;
 }
 std::vector<AttentionTopic> out;for(const auto& [id,t]:latest){(void)id;if(t.expires>now)out.push_back(t);}
 auto order=[&](const auto&a,const auto&b){return std::tuple{!a.emergency,a.emergency?-a.urgency:0.,a.id!=focus,-a.priority,a.id}<std::tuple{!b.emergency,b.emergency?-b.urgency:0.,b.id!=focus,-b.priority,b.id};};
 std::sort(out.begin(),out.end(),order);if(out.size()>32)out.resize(32);return out;
}
SubjectiveNeed interpret_need(const SubjectiveNeed& previous,double observation,double quality,std::uint64_t source,Tick now){
 SubjectiveNeed out=previous;std::optional<cog04::Reading> prior;
 if(previous.prior_source&&previous.prior_quality>0)prior=cog04::Reading{previous.prior,previous.prior_quality,previous.prior_source};
 auto b=cog04::fuse(cog04::Reading{observation,quality,source},prior);
 out.status=b.status==cog04::Status::Supported?Truth::Confirmed:b.status==cog04::Status::Conflicting?Truth::Conflicting:Truth::Unknown;
 out.mean=b.mean;out.confidence=b.confidence;out.observation=observation;out.quality=quality;out.source=source;out.at=now;
 if(out.status==Truth::Confirmed)out.band=int(cog04::band(b.mean,cog04::Band(previous.band)));else out.band=-1;
 // Do NOT copy posterior to independent prior: no recursive evidence inflation.
 return out;
}
void validate_cognition(const CognitiveState& c,Tick now){
 auto ok=[](bool b,const char* text){if(!b)throw std::runtime_error(std::string("cognition invariant: ")+text);};
 ok(c.candidates.size()<=32&&c.context.size()<=12&&c.options.size()<=8&&c.recent.size()<=8,"capacity");
 ok(c.budget>=0&&c.budget<=32&&c.spent>=0&&c.spent<=32&&c.budget+c.spent<=32,"budget");
 ok(c.focus_metric<metric_count&&c.cursor<=c.options.size(),"cursor");
 ok(unsigned(c.operation)<=unsigned(Operation::SocialObserve)&&unsigned(c.focus_kind)<=unsigned(TopicKind::Project),"enum");
 require_range(c.work,0,1);require_range(c.rate,0,8);require_range(c.rejection_bias,0,1);
 if(c.operation!=Operation::None){ok(c.active&&c.due>=now&&c.op_last<=now&&c.op_started<=now,"operation clock");}
 for(auto until:c.no_continuation_until)ok(until>=0,"negative topic reconsideration time");
 for(const auto& n:c.needs){ok(unsigned(n.status)<=unsigned(Truth::Conflicting)&&n.band>=-1&&n.band<=3,"belief");require_range(n.mean,0,1);require_range(n.confidence,0,1);require_range(n.observation,0,1);require_range(n.prior_quality,0,1);}
 for(const auto& t:c.candidates){require_range(t.priority,0,1);ok(t.id&&t.metric<metric_count,"topic");}
 for(const auto& t:c.recent){ok(t.time<=now&&t.started<=t.time&&t.metric<metric_count&&unsigned(t.kind)<unsigned(ThoughtKind::Count),"thought clock/type");}
 for(const auto& p:c.percepts){ok(p.token>0&&p.since<=p.last&&p.last<=now,"percept");require_range(p.threshold,0,1e6);require_range(p.feature_work,0,1e6);}
}
}
