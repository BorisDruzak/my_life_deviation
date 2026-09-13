#include "test.hpp"
#include "life/world.hpp"
#include <filesystem>
using namespace life;
namespace {
World shop_world(){
    auto w=World::generate(42,8);w.configure_recovery();auto& s=w.edit_for_test();s.autonomy=false;s.now=9*3600000;s.next_physical=s.now;
    for(auto& a:s.actors){a.physical_at=s.now;a.place=a.home;a.action=Action{};a.conversation={};}
    auto& buyer=s.actors[0];s.ledger.initial_money+=300-buyer.money;buyer.money=buyer.mind.believed_money=300;buyer.place=7;
    auto& clerk=s.actors[1];clerk.place=7;w.command_for_test(2,Method::Work,7);return w;
}
}
TEST("civil_world", initial_profile_is_depleted_but_not_jobless_by_construction){auto w=World::generate(42,16);w.configure_recovery();for(const auto& a:w.state().actors){CHECK(a.money>=18&&a.money<=36);CHECK(a.equipment.garment&&a.equipment.phone);}unsigned capacity=0;for(const auto& h:w.state().hiring)capacity+=h.capacity;CHECK(capacity>=16);w.validate();}
TEST("civil_world", retail_purchase_moves_money_and_real_object){auto w=shop_world();const auto old=w.state().actors[0].equipment.garment;double money=w.state().actors[0].money;w.buy_clothes_for_test(1,1);w.run_seconds(121);const auto& a=w.state().actors[0];CHECK(a.equipment.garment!=old);NEAR(a.money,money-35,1e-8);CHECK(w.state().civil.purchases==1);w.validate();}
TEST("civil_world", absent_clerk_prevents_purchase){auto w=shop_world();auto& s=w.edit_for_test();s.actors[1].action={};s.actors[1].place=s.actors[1].home;double money=s.actors[0].money;w.buy_clothes_for_test(1,1);w.run_seconds(121);NEAR(w.state().actors[0].money,money,1e-8);CHECK(w.state().civil.purchases==0);}
TEST("civil_world", last_garment_cannot_be_sold_twice){auto w=shop_world();auto& s=w.edit_for_test();bool first=true;for(auto& x:s.civil.stock)if(x.sku==1&&!x.sold){if(first)first=false;else x.price=100000;}s.actors[2].place=7;s.ledger.initial_money+=300-s.actors[2].money;s.actors[2].money=s.actors[2].mind.believed_money=300;w.buy_clothes_for_test(1,1);w.buy_clothes_for_test(3,1);w.run_seconds(121);CHECK(w.state().civil.purchases==1);w.validate();}
TEST("civil_world", cancel_purchase_has_no_payment_or_reward){auto w=shop_world();auto money=w.state().actors[0].money;w.buy_clothes_for_test(1,1);w.run_seconds(30);w.command_for_test(1,Method::Idle,7);w.run_seconds(121);NEAR(w.state().actors[0].money,money,1e-8);CHECK(w.state().civil.purchases==0);}
TEST("civil_world", purchase_does_not_directly_modify_self){auto w=shop_world();auto before=w.state().actors[0].mind.self.predict(SelfDomain::General);w.buy_clothes_for_test(1,1);w.run_seconds(121);auto after=w.state().actors[0].mind.self.predict(SelfDomain::General);NEAR(before.efficacy,after.efficacy,0);CHECK(w.state().actors[0].cog.outcome_published>0);}
TEST("civil_world", recovery_profile_save_restore){auto w=shop_world();w.buy_clothes_for_test(1,1);w.run_seconds(45);auto p=(std::filesystem::temp_directory_path()/"life_civil13.save").string();w.save(p);auto r=World::load(p);w.run_seconds(600);r.run_seconds(600);CHECK(w.hash()==r.hash());std::filesystem::remove(p);}
TEST("civil_world", no_wallet_leak_in_personal_forecast){auto w=World::generate(42,8);w.configure_recovery();auto a=w.personal_view(1);auto& s=w.edit_for_test();s.ledger.initial_money+=1000;s.actors[1].money+=1000;auto b=w.personal_view(1);CHECK(a.civil.help_candidates==b.civil.help_candidates);CHECK(a.social.memory.community.resources.revision==b.social.memory.community.resources.revision);}
TEST("civil_world", changed_price_requires_new_assessment_before_payment){auto w=shop_world();auto& s=w.edit_for_test();for(auto& item:s.civil.stock)if(item.sku==1&&!item.sold)item.price=40;auto money=s.actors[0].money;w.buy_clothes_for_test(1,1);w.run_seconds(121);CHECK(s.civil.purchases==0);NEAR(s.actors[0].money,money,0);CHECK(s.actors[0].mind.civil.shops[0].offers[0].price==40);}
TEST("civil_world", diagnostic_report_exposes_provenance_and_material_conservation){
 auto w=World::generate(42,8);w.configure_recovery();
 const auto before=w.hash();const auto json=w.recovery_report_json();
 CHECK(json.find("\"garments_initial\":")!=std::string::npos);
 CHECK(json.find("\"garments_in_world\":")!=std::string::npos);
 CHECK(json.find("\"questions\":[")!=std::string::npos);
 CHECK(json.find("\"reviewed_basis\":")!=std::string::npos);
 CHECK(w.hash()==before);
}
