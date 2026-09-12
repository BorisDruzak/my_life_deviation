#include "life/world.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace life;
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::invalid_argument("fixture OUTPUT_DIRECTORY");
  std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
  auto common=World::generate(42,8);common.configure_adaptive_life();
  common.edit_for_test().autonomy=false;common.run_seconds(9*3600);
  common.edit_for_test().hiring[1].hourly=40;
  for(auto& person:common.edit_for_test().actors){person.mind.career.enabled=false;person.body.energy=person.body.water=.9;person.body.sleep=.1;}
  auto& actor=common.edit_for_test().actors[1];actor.mind.career.enabled=true;
  for(auto& q:actor.mind.career.questions)q.stage=CareerStage::Deferred;
  JobKnowledge k;k.organization=2;k.place=7;k.hourly=10;k.shift_start=32400000;k.shift_end=61200000;k.vacancy=true;k.known=31;
  common.career_information_for_test(2,k,900000);common.edit_for_test().autonomy=true;
  auto informed=common;k.hourly=40;informed.career_information_for_test(2,k,900001);
  common.save((dir/"control.save").string());informed.save((dir/"informed.save").string());
  std::ofstream(dir/"initial_control_career.json")<<common.career_report_json();
  std::ofstream(dir/"initial_informed_career.json")<<informed.career_report_json();
  std::cout<<"{\"actor\":2,\"same_actual_job_2_hourly\":40,\"control_believed_job_2_hourly\":10,\"new_report_hourly\":40,\"new_source\":900001,\"prior_source\":900000,\"actual_time_ms\":"<<common.state().now<<"}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
