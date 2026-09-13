#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
namespace life {
namespace {
constexpr Tick H=3600000,D=86400000;
constexpr std::uint64_t question_base=10ull<<48,question_end=11ull<<48;
WorldObject* item(State& s,Id id){auto i=std::lower_bound(s.social.objects.begin(),s.social.objects.end(),id,[](const auto& a,Id b){return a.id<b;});return i!=s.social.objects.end()&&i->id==id?&*i:nullptr;}
const WorldObject* item(const State& s,Id id){return item(const_cast<State&>(s),id);}
Place& place(State& s,Id id){auto i=std::find_if(s.places.begin(),s.places.end(),[&](const auto& p){return p.id==id;});if(i==s.places.end())throw std::logic_error("retail place missing");return *i;}
std::uint64_t question_id(const OpenQuestion& q){return question_base+(std::uint64_t(q.kind)<<32)+q.target;}
void add_garment(State& s,Id shop,Id org,const RetailOffer& offer){
    WorldObject object;object.id=Id(s.next_id++);object.kind=ObjectKind::Garment;object.owner_org=org;object.place=shop;object.sku=offer.sku;object.condition=1;object.readable=false;object.prestige=double(offer.tier)/2.;
    s.social.objects.push_back(object);s.civil.stock.push_back({object.id,shop,offer.sku,offer.price,false});
}
bool shop_open(Tick now){return (now/D)%7<5&&now%D>=9*H&&now%D<17*H;}
}
void World::civil_record(Actor& a,const char* kind,std::uint64_t source,std::uint64_t action,Id person,Id object){
    CivilTrace t{state_.now,source,action,person,object,kind};a.mind.civil.record(t);
    auto& audit=state_.civil.trace;audit.push_back(t);if(audit.size()>512)audit.erase(audit.begin());
    if(logger_){EventLog e{state_.now,a.id,source,a.action.method,"civil",kind,a.place,person};e.object=object;e.parent=action;logger_(e);}
}
void World::configure_recovery(bool effects){
    auto& s=state_;if(s.now||s.civil.enabled)throw std::invalid_argument("recovery requires an initial world");
    if(!s.adaptive_life)configure_adaptive_life(effects);
    s.civil.enabled=true;s.balance="recovery-0.13-1";
    // Historic objects use a separate seeded range. New identities must not collide.
    for(const auto& existing:s.social.objects)s.next_id=std::max(s.next_id,std::uint64_t(existing.id)+1);
    s.civil.shops.push_back({7,2,{{1,35,0},{2,95,1},{3,180,2}}});
    auto& shop=place(s,7);shop.services|=service(Method::BuyClothes);
    // This initial shop employee is a seeded employment relationship, not a per-tick command.
    auto& clerk=s.actors.at(1);
    for(auto& org:s.life.organizations)for(auto& member:org.members)if(member.person==clerk.id)member.active=false;
    for(auto& org:s.life.organizations)if(org.id==2){EmployeeContribution member;member.person=clerk.id;member.hourly=18;org.members.push_back(member);}
    clerk.employment.organization=2;clerk.employment.workplace=7;clerk.employment.hourly=18;clerk.employment.supervisor=1;clerk.employment.created_shift=-1;
    for(auto& office:s.hiring)if(office.organization==2)office.hourly=18;
    shop.wage=18;
    for(auto& a:s.actors){
        auto& m=a.mind.civil;m.enabled=true;a.mind.social.community.resources.enabled=true;
        WorldObject g;g.id=Id(s.next_id++);g.kind=ObjectKind::Garment;g.owner=g.holder=g.wearer=a.id;g.place=a.place;g.sku=1;g.condition=.3+.6*s.random.uniform("clothing-condition",a.id,0);g.readable=false;g.prestige=0;
        a.equipment.garment=m.own_garment=g.id;m.garment_condition=g.condition;s.social.objects.push_back(g);++s.civil.garments_initial;
        WorldObject phone;phone.id=Id(s.next_id++);phone.kind=ObjectKind::Phone;phone.owner=phone.holder=a.id;phone.place=a.place;phone.phone_number=7000000000ull+phone.id;phone.readable=false;
        a.equipment.phone=m.own_phone=phone.id;s.social.objects.push_back(phone);
        m.desired_tier=a.mind.social.community.ambition>.72?1:0;
        const auto favourite=std::max_element(a.economy.interests.begin(),a.economy.interests.end())-a.economy.interests.begin();
        m.chosen_leisure_cost=recreation_catalogue()[std::size_t(favourite)].fee;
        const auto source=s.next_id++;
        m.shops.push_back({7,source,0,0,s.civil.shops[0].catalogue});m.questions.ensure(QuestionKind::Clothing,7,source,0);
        for(auto& known:a.mind.places)if(known.id==7){known.services=shop.services;known.wage=18;}
        a.mind.known[std::size_t(Method::BuyClothes)]=a.mind.known[std::size_t(Method::SendMessage)]=a.mind.known[std::size_t(Method::ReadMessage)]=true;
        for(auto kind:{Interaction::AskMoney,Interaction::AskPhone,Interaction::ExplainProcedure}){
            auto& method=a.mind.social.methods[std::size_t(kind)];method={.85,.15,.65,.7,s.next_id++};a.social_traits.pleasure[std::size_t(kind)]=.15;
        }
        JobKnowledge public_notice;public_notice.organization=2;public_notice.place=7;public_notice.hourly=18;public_notice.known=LocationKnown|WageKnown;
        receive_career_information(a,public_notice,s.next_id++);
        // A subset has not learned the social outing sequence. They retain atomic
        // actions, and can learn the sequence from another person's explanation.
        if(a.id%4==0)a.mind.projects.procedures.erase(std::remove_if(a.mind.projects.procedures.begin(),a.mind.projects.procedures.end(),[](const auto& p){return p.goal==GoalKind::FindCompany;}),a.mind.projects.procedures.end());
        ++a.mind.version;
    }
    // One explicitly seeded past exchange per actor; other addresses must be requested.
    for(auto& a:s.actors)if(!a.mind.relations.empty()){
        const auto slot=std::min(a.mind.relations.size()-1,std::size_t(s.random.uniform("known-phone-history",a.id,0)*a.mind.relations.size()));
        const auto other=a.mind.relations[slot].person;const auto* phone=item(s,s.actors[other-1].equipment.phone);
        a.mind.civil.learn_number(other,phone->phone_number,s.next_id++,0);
    }
    for(const auto& offer:s.civil.shops[0].catalogue){const auto count=std::max<std::size_t>(1,s.actors.size()/(offer.tier+1));for(std::size_t k=0;k<count;++k){add_garment(s,7,2,offer);++s.civil.garments_initial;}}
    validate();
}
void World::civil_physical(Actor& a,Tick elapsed){
    if(!state_.civil.enabled||elapsed==0)return;
    if(auto* g=item(state_,a.equipment.garment);g&&g->kind==ObjectKind::Garment&&g->wearer==a.id&&g->holder==a.id){
        const double factor=a.action.method==Method::Work&&a.action.phase==Phase::Running?1.4:1.;
        g->condition=worn_condition(g->condition,g->wear_per_day*factor,double(elapsed)/1000.);g->place=a.place;
    }
    if(auto* phone=item(state_,a.equipment.phone);phone&&phone->holder==a.id)phone->place=a.place;
}
void World::civil_tick(){
    auto& s=state_;if(!s.civil.enabled)return;
    if(s.now>=s.civil.next_supply){
        for(const auto& shop:s.civil.shops)for(std::size_t i=0;i<std::max<std::size_t>(1,s.actors.size()/8);++i){add_garment(s,shop.place,shop.organization,shop.catalogue.front());++s.civil.garments_supplied;}
        s.civil.next_supply+=D;const auto source=s.next_id++;
        if(logger_)logger_({s.now,0,source,Method::Idle,"external_supply","garments",7});
    }
    if(s.now<s.civil.next_observe)return;
    s.civil.next_observe=s.now+60000;
    for(auto& a:s.actors){if(!a.alive||capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep).gate==0)continue;
        auto& m=a.mind.civil;
        const auto* g=item(s,a.equipment.garment);
        if(g&&g->wearer==a.id&&g->holder==a.id){
            const auto band=int(g->condition*20),old=int(m.garment_condition*20);
            if(band!=old||m.own_garment!=g->id){m.own_garment=g->id;m.garment_condition=g->condition;m.garment_tier=std::uint8_t(g->sku-1);const auto source=s.next_id++;
                if(g->condition<.45||m.desired_tier>m.garment_tier)m.questions.wake(QuestionKind::Clothing,7,source,s.now);
                civil_record(a,"own_clothing_condition_observed",source,0,0,g->id);++a.mind.version;}
        }
        const auto* phone=item(s,a.equipment.phone);
        const Id available_phone=phone&&phone->owner==a.id&&phone->holder==a.id?phone->id:0;
        if(m.own_phone!=available_phone){m.own_phone=available_phone;++a.mind.version;}
        unsigned examined=0;
        for(const auto& p:a.cog.percepts){if(!p.recognized||p.last<s.now-1000||examined++>=4)continue;
            const auto& other=s.actors.at(p.token-1);const auto* garment=item(s,other.equipment.garment);
            if(!garment||garment->wearer!=other.id||garment->holder!=other.id)continue;
            const bool already=std::any_of(a.mind.social.community.resources.facts.begin(),a.mind.social.community.resources.facts.end(),[&](const auto& f){return f.subject==other.id&&f.object==garment->id&&f.kind==ResourceKind::VisibleClothing;});
            const bool queued=std::any_of(a.mind.social.inbox.begin(),a.mind.social.inbox.end(),[&](const auto& o){return o.information_present&&o.information.resource_present&&o.information.resource.object==garment->id;});
            if(already||queued)continue;
            Information f;f.id=s.next_id++;f.kind=NewsKind::Opinion;f.subject=other.id;f.location=a.place;f.occurred_at=s.now;f.importance=.3;f.resource_present=true;
            f.resource={f.id,other.id,garment->id,0,ResourceKind::VisibleClothing,double(garment->sku-1),.8,s.now,false};
            InteractionObservation o;o.event=f.id;o.other=other.id;o.kind=Interaction::ShareNews;o.stage=SocialStage::Completed;o.at=s.now;o.information=f;o.information_present=true;
            social_deliver(a,o);
        }
        // Reconsider the ready wardrobe question after an observed failed attempt,
        // never every frame. The new opportunity is a timed retry of a known shop.
        for(auto& q:m.questions.items)if(q.kind==QuestionKind::Clothing&&!q.pending&&!q.actionable&&q.retry_at&&s.now>=q.retry_at){q.pending=true;q.changed=s.now;++q.revision;}
        for(const auto& p:a.mind.projects.projects)if(p.live()&&p.goal.target&&m.number_for(p.goal.target)&&s.now>=m.next_contact){
            if(!m.questions.find(QuestionKind::Contact,p.goal.target))m.questions.ensure(QuestionKind::Contact,p.goal.target,p.goal.basis,s.now);
            else if(m.questions.find(QuestionKind::Contact,p.goal.target)->basis!=p.goal.basis)m.questions.wake(QuestionKind::Contact,p.goal.target,p.goal.basis,s.now);
        }
    }
}
void World::populate_civil_view(const Actor& a,PersonalView& v)const{
    if(!state_.civil.enabled)return;
    auto& c=v.civil;c.enabled=true;c.memory=a.mind.civil;c.memory.trace.clear();c.memory.lessons.lessons.clear();
    c.budget=civil_budget(a.mind.civil,v.money,v.food,state_.now);v.need[7]=c.budget.pressure;
    v.economy.reserve=c.budget.target;v.economy.protected_cash=c.budget.protected_cash;v.economy.study_budget=std::max(0.,v.money-c.budget.protected_cash);
    auto loads=activity_resources(a.action.method,a.action.phase==Phase::Travel,a.action.phase==Phase::Asleep);
    const auto text=activity_resources(Method::SendMessage);
    bool fits=combine_resources(loads,text).allowed;
    if(a.conversation.id){const auto talk=activity_resources(Method::Talk);for(std::size_t i=0;i<6;++i){const bool hard=i==0||i==1||i==4;if(loads.load[i]+talk.load[i]+text.load[i]>(hard?1.:1.5))fits=false;}}
    c.can_text=c.memory.own_phone&&!a.equipment.side_action&&!a.mind.social.active_event&&fits;
    v.social.civil_enabled=true;v.social.has_phone=c.memory.own_phone!=0;v.social.help_budget=std::max(0.,v.money-c.budget.protected_cash);
    v.social.memory.community.resources=a.mind.social.community.resources;
    for(const auto& r:a.mind.relations){auto hypothesis=a.mind.social.community.resources.assess(r.person,state_.now);
        if(hypothesis.may_have_spare_money){c.help_candidates.push_back(r.person);v.social.help_people.push_back(r.person);}}
    for(const auto& p:v.social.perceived)if(!c.memory.number_for(p.id))v.social.unknown_numbers.push_back(p.id);
    for(const auto& p:a.mind.projects.procedures)if(p.mastery>=.6)v.social.teachable.push_back(p.id);
}
void World::append_civil_topics(const Actor& a,std::vector<AttentionTopic>& out)const{
    if(!state_.civil.enabled)return;
    for(const auto& q:a.mind.civil.questions.items)if(q.pending&&state_.now>=q.retry_at){
        const double age=unit(double(state_.now-q.changed)/600000.);const double priority=unit(.5+.45*age);
        out.push_back({question_id(q),TopicKind::Thought,std::uint8_t(q.kind==QuestionKind::Contact?5:7),q.kind==QuestionKind::Contact?q.target:0,priority,.3,q.changed,state_.now+31000,q.revision,q.basis,false});}
}
bool World::start_civil_cognition(Actor& a){
    auto& c=a.cog;if(!state_.civil.enabled||c.focus<question_base||c.focus>=question_end)return false;
    auto& questions=a.mind.civil.questions;const auto key=c.focus-question_base;
    const auto* q=questions.find(QuestionKind(key>>32),Id(key));
    if(!q||!q->pending){c.active=false;return true;}if(c.budget<2){c.active=false;return true;}
    c.civil_question=*q;cognition_start(a,Operation::RecallCivilFacts);return true;
}
bool World::complete_civil_cognition(Actor& a,Operation op,Tick started){
    if(op!=Operation::RecallCivilFacts&&op!=Operation::CompareCivilFacts)return false;
    auto& c=a.cog;auto& m=a.mind.civil;auto* q=m.questions.find(c.civil_question.kind,c.civil_question.target);
    if(!q){c.active=false;return true;}
    if(q->revision!=c.civil_question.revision){c.active=false;c.operation=Operation::None;c.review_at=state_.now;return true;}
    Thought t;t.started=started;t.basis=q->basis;t.debug_knowledge_source=q->basis;t.metric=q->kind==QuestionKind::Contact?5:7;t.origin=Origin::Inferred;t.status=Truth::Unknown;
    if(op==Operation::RecallCivilFacts){t.kind=ThoughtKind::Recall;t.detail="recall_personal_question_and_accessible_facts";emit_thought(a,t);cognition_start(a,Operation::CompareCivilFacts);return true;}
    q->pending=false;q->reviewed_basis=q->basis;++m.questions.reviews;q->actionable=false;
    if(q->kind==QuestionKind::Clothing)q->actionable=m.garment_condition<.4||m.desired_tier>m.garment_tier;
    else if(q->kind==QuestionKind::Money)q->actionable=a.mind.believed_money<civil_budget(m,a.mind.believed_money,a.mind.believed_food,state_.now).protected_cash&&a.mind.social.community.resources.assess(q->target,state_.now).may_have_spare_money;
    else if(q->kind==QuestionKind::Contact){
        const bool nearby=std::find(c.snapshot.perceived_people.begin(),c.snapshot.perceived_people.end(),q->target)!=c.snapshot.perceived_people.end();
        if(!nearby&&state_.now>=m.next_contact&&1-a.social>.3&&m.number_for(q->target)){
            LetterContent letter;letter.kind=LetterKind::Greeting;m.compose(q->target,letter,q->basis,state_.now);m.next_contact=state_.now+3*H;
        }
    }
    t.kind=ThoughtKind::Compare;t.value=q->actionable?1:0;t.detail="question_reviewed_not_a_world_command";emit_thought(a,t);
    civil_record(a,"question_reviewed",q->basis,0,q->target,unsigned(q->kind));++a.mind.version;
    c.operation=Operation::None;c.active=false;c.captured_mind=a.mind.version;c.captured_situation=c.situation_version;c.review_at=state_.now+1000;return true;
}
void World::civil_failed_action(Actor& a){
    if(!state_.civil.enabled||a.action.method!=Method::BuyClothes)return;
    for(auto& shop:a.mind.civil.shops)if(shop.place==a.place)shop.retry_at=state_.now+30*60000;
    if(auto* q=a.mind.civil.questions.find(QuestionKind::Clothing,a.place)){q->actionable=false;q->retry_at=state_.now+30*60000;}
    civil_record(a,"retail_attempt_failed_wait_for_new_opportunity",a.action.id,a.action.id,0,a.action.object);
}
bool World::complete_civil_action(Actor& a){
    if(a.action.method!=Method::BuyClothes)return false;
    auto& s=state_;auto shop=std::find_if(s.civil.shops.begin(),s.civil.shops.end(),[&](const auto& x){return x.place==a.place;});
    if(!s.civil.enabled||shop==s.civil.shops.end()||!shop_open(s.now)){fail(a,"clothing_shop_closed");return true;}
    const auto clerk=std::find_if(s.actors.begin(),s.actors.end(),[&](const auto& x){return x.id!=a.id&&x.alive&&x.place==a.place&&x.employment.active&&x.employment.organization==shop->organization&&x.action.method==Method::Work&&x.action.phase==Phase::Running&&!x.action.blocked;});
    if(clerk==s.actors.end()){fail(a,"clothing_shop_unstaffed");return true;}
    const auto sku=a.action.object?a.action.object:1;
    auto stock=std::find_if(s.civil.stock.begin(),s.civil.stock.end(),[&](const auto& x){return x.place==a.place&&x.sku==sku&&!x.sold;});
    if(stock==s.civil.stock.end()){fail(a,"clothing_stock_empty");return true;}
    auto known_shop=std::find_if(a.mind.civil.shops.begin(),a.mind.civil.shops.end(),[&](const auto& k){return k.place==a.place;});
    if(known_shop==a.mind.civil.shops.end()){fail(a,"clothing_price_unknown");return true;}
    auto quote=std::find_if(known_shop->offers.begin(),known_shop->offers.end(),[&](const auto& x){return x.sku==sku;});
    if(quote==known_shop->offers.end()){fail(a,"clothing_product_unknown");return true;}
    if(std::abs(quote->price-stock->price)>1e-9){quote->price=stock->price;known_shop->source=s.next_id++;known_shop->observed_at=s.now;a.mind.civil.questions.wake(QuestionKind::Clothing,a.place,known_shop->source,s.now);++a.mind.version;fail(a,"clothing_quote_changed");return true;}
    auto* garment=item(s,stock->object);
    if(!garment||garment->owner_org!=shop->organization||garment->owner||garment->holder||a.money<stock->price){fail(a,"clothing_exchange_unavailable");return true;}
    // Check every precondition before the atomic transfer. No reservation grants a free object.
    const auto action=a.action.id;const auto root=s.next_id++;const double price_paid=stock->price;
    if(auto* old=item(s,a.equipment.garment);old&&old->wearer==a.id)old->wearer=0;
    a.money-=price_paid;place(s,a.place).account+=price_paid;stock->sold=true;
    garment->owner_org=0;garment->owner=garment->holder=garment->wearer=a.id;garment->place=a.place;a.equipment.garment=garment->id;
    auto& m=a.mind.civil;m.own_garment=garment->id;m.garment_condition=garment->condition;m.garment_tier=std::uint8_t(sku-1);
    ++m.clothes_bought;m.clothing_spent+=price_paid;++s.civil.purchases;
    a.mind.believed_money=a.money;++a.mind.version;++a.cog.situation_version;
    if(auto* q=m.questions.find(QuestionKind::Clothing,a.place)){q->actionable=false;q->pending=false;q->retry_at=0;}
    Outcomes result{};result[7]=-std::min(1.,price_paid/100.);publish_action_outcome(a,true,false,result,std::uint16_t(1u<<7));
    Information receipt;receipt.id=root;receipt.kind=NewsKind::Purchase;receipt.resource_present=true;receipt.resource={root,a.id,garment->id,0,ResourceKind::VisibleClothing,double(sku-1),1,s.now,false};receipt.importance=.6;community_publish(a,receipt);
    civil_record(a,"clothing_bought_and_worn",root,action,clerk->id,garment->id);
    ++a.completed[std::size_t(Method::BuyClothes)];emit(a,"retail","purchase_completed");a.action={};a.review=s.now;a.exposure.clear();return true;
}
void World::buy_clothes_for_test(Id id,Id sku){auto& a=state_.actors.at(id-1);cancel(a);Decision d;d.method=Method::BuyClothes;d.place=a.place;d.path={a.place};d.object=sku;start(a,d);}
void World::validate_civil()const{
    const auto& s=state_;if(!s.civil.enabled)return;
    if(!s.adaptive_life||!s.self_enabled||s.civil.shops.empty())throw std::runtime_error("civil dependencies");
    std::uint64_t garments=0;std::set<std::uint64_t> numbers;
    for(const auto& o:s.social.objects){if(o.kind==ObjectKind::Garment){++garments;if(o.sku<1||o.sku>3||o.wearer>s.actors.size()||(o.wearer&&o.holder!=o.wearer))throw std::runtime_error("garment invariant");require_range(o.wear_per_day,0,1);}
        if(o.kind==ObjectKind::Phone&&(!o.phone_number||!numbers.insert(o.phone_number).second))throw std::runtime_error("phone number uniqueness");}
    if(garments!=s.civil.garments_initial+s.civil.garments_supplied)throw std::runtime_error("garment conservation");
    for(const auto& stock:s.civil.stock){const auto* g=item(s,stock.object);if(!g||g->kind!=ObjectKind::Garment||g->sku!=stock.sku)throw std::runtime_error("retail stock identity");require_range(stock.price,0,1e9);if(!stock.sold&&(g->owner||g->holder))throw std::runtime_error("unsold item transferred");}
    for(const auto& a:s.actors){const auto& m=a.mind.civil;m.questions.validate(s.now);a.mind.social.community.resources.validate(s.now);
        if(!m.enabled||m.contacts.size()>128||m.drafts.size()>16||m.inbox.size()>32||m.trace.size()>128||m.lessons.lessons.size()>32||m.read_messages.size()>128||m.cancelled_meetings.size()>64)throw std::runtime_error("civil memory capacity");
        require_range(m.garment_condition,0,1);civil_budget(m,a.mind.believed_money,a.mind.believed_food,state_.now);
        if(const auto* g=item(s,a.equipment.garment);!g||g->wearer!=a.id)throw std::runtime_error("equipment garment link");
        std::set<Id> contacts;for(const auto& c:m.contacts)if(!c.person||!c.number||!c.source||c.at>s.now||!contacts.insert(c.person).second)throw std::runtime_error("contact knowledge provenance");
        std::set<Id> drafts;for(const auto& draft:m.drafts)if(!draft.id||draft.id>=m.next_draft||!draft.person||draft.person>s.actors.size()||!draft.basis||draft.created>s.now||draft.retry_at<0||unsigned(draft.content.kind)>3||!drafts.insert(draft.id).second)throw std::runtime_error("draft invariant");
        for(const auto& lesson:m.lessons.lessons){if(!lesson.content.root||!lesson.content.teacher||lesson.at>s.now)throw std::runtime_error("lesson provenance");require_range(lesson.understanding,0,1);ProjectMemory check;check.procedures={lesson.content.procedure};check.validate(s.now);}
        if(a.equipment.side_action&&(a.equipment.side_action>=s.next_id||a.equipment.side_started>s.now))throw std::runtime_error("phone action chronology");
        if(a.equipment.side_action&&a.equipment.side_end<s.now)throw std::runtime_error("past phone action");
    }
    if(s.civil.messages.size()>512)throw std::runtime_error("phone transport capacity");
    std::set<std::uint64_t> messages;for(const auto& e:s.civil.messages)if(unsigned(e.content.kind)>3||!e.from_number||!e.number||!e.id||!messages.insert(e.id).second||!e.sender||e.sender>s.actors.size()||e.receiver>s.actors.size()||e.sent_at>s.now||e.deliver_at<e.sent_at||(!e.delivered&&e.read))throw std::runtime_error("phone envelope invariant");
}
std::string World::recovery_report_json()const{
    std::ostringstream o;o.precision(17);const auto& s=state_;const auto& c=s.civil;
    const auto garments=std::count_if(s.social.objects.begin(),s.social.objects.end(),[](const auto& x){return x.kind==ObjectKind::Garment;});
    const auto on_sale=std::count_if(c.stock.begin(),c.stock.end(),[](const auto& x){return !x.sold;});
    o<<"{\"enabled\":"<<(c.enabled?"true":"false")<<",\"version\":\"0.13.0-recovery1\",\"purchases\":"<<c.purchases<<",\"sends\":"<<c.sends<<",\"deliveries\":"<<c.deliveries<<",\"reads\":"<<c.reads<<",\"failed_messages\":"<<c.failed_messages<<",\"gifts\":"<<c.gifts<<",\"money_transferred\":"<<c.money_transferred<<",\"lessons\":"<<c.lessons
     <<",\"garments_initial\":"<<c.garments_initial<<",\"garments_supplied\":"<<c.garments_supplied<<",\"garments_in_world\":"<<garments<<",\"garments_on_sale\":"<<on_sale<<",\"actors\":[";
    bool first=true;for(const auto& a:s.actors){if(!first)o<<',';first=false;const auto& m=a.mind.civil;
        o<<"{\"id\":"<<a.id<<",\"garment\":"<<a.equipment.garment<<",\"condition\":"<<m.garment_condition<<",\"tier\":"<<unsigned(m.garment_tier)<<",\"desired_tier\":"<<unsigned(m.desired_tier)<<",\"clothes_bought\":"<<m.clothes_bought<<",\"clothing_spent\":"<<m.clothing_spent<<",\"money\":"<<a.money<<",\"contacts\":"<<m.contacts.size()<<",\"sent\":"<<m.sent<<",\"read\":"<<m.read<<",\"lessons\":"<<m.lessons.received<<",\"procedures_acquired\":"<<m.procedures_acquired<<",\"reviews\":"<<m.questions.reviews<<",\"reopened\":"<<m.questions.reopened<<",\"gifts_given\":"<<m.gifts_given<<",\"gifts_received\":"<<m.gifts_received;
        const auto budget=civil_budget(m,a.mind.believed_money,a.mind.believed_food,s.now);
        o<<",\"budget\":{\"protected\":"<<budget.protected_cash<<",\"target\":"<<budget.target<<",\"pressure\":"<<budget.pressure<<"},\"facts\":[";
        bool ff=true;for(const auto& f:a.mind.social.community.resources.facts){if(!ff)o<<',';ff=false;o<<"{\"root\":"<<f.root<<",\"subject\":"<<f.subject<<",\"object\":"<<f.object<<",\"speaker\":"<<f.speaker<<",\"kind\":"<<unsigned(f.kind)<<",\"value\":"<<f.value<<",\"confidence\":"<<f.confidence<<",\"observed_ms\":"<<f.at<<",\"reported\":"<<(f.reported?"true":"false")<<'}';}
        o<<"],\"questions\":[";ff=true;for(const auto& q:m.questions.items){if(!ff)o<<',';ff=false;o<<"{\"kind\":"<<unsigned(q.kind)<<",\"target\":"<<q.target<<",\"basis\":"<<q.basis<<",\"reviewed_basis\":"<<q.reviewed_basis<<",\"revision\":"<<q.revision<<",\"retry_ms\":"<<q.retry_at<<",\"pending\":"<<(q.pending?"true":"false")<<",\"actionable\":"<<(q.actionable?"true":"false")<<'}';}
        o<<"],\"drafts\":[";ff=true;for(const auto& d:m.drafts){if(!ff)o<<',';ff=false;o<<"{\"id\":"<<d.id<<",\"person\":"<<d.person<<",\"basis\":"<<d.basis<<",\"kind\":"<<unsigned(d.content.kind)<<",\"created_ms\":"<<d.created<<'}';}
        o<<"],\"trace\":[";ff=true;for(const auto& t:m.trace){if(!ff)o<<',';ff=false;o<<"{\"ms\":"<<t.at<<",\"source\":"<<t.source<<",\"action\":"<<t.action<<",\"person\":"<<t.person<<",\"object\":"<<t.object<<",\"kind\":\""<<t.kind<<"\"}";}o<<"]}";
    }o<<"]}";return o.str();
}
}
