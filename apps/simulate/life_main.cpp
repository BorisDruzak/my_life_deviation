#include "life/world.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <locale>
#include <map>
#include <optional>
#include <string_view>

namespace {
std::uint64_t number(const std::string& s){std::uint64_t n=0;auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),n);if(e!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument("invalid unsigned integer: "+s);return n;}
std::ofstream output(const std::string& path){std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("cannot write: "+path);out.exceptions(std::ios::badbit|std::ios::failbit);out.precision(12);return out;}
}
int main(int argc,char** argv){
    try{
        std::locale::global(std::locale::classic());
        const std::string help="life_sim [--seed N] [--population 8..4096] [--days N | --seconds N] [--scenario normal|scarcity|closed-road] [--community] [--community-state FILE] [--reference] [--load FILE] [--save FILE] [--summary FILE] [--actors CSV] [--social-state JSON] [--trace JSONL] [--trace-actor ID] [--social-scene mixed|friendly|personal-boundaries|romantic|romantic-norm|recipient-refuses|pleasure-mismatch|borrow|borrow-novice|information|status|status-norm] [--thoughts JSONL] [--workers 1..32] [--work-chunk N]\n";
        std::map<std::string,std::string> options;bool reference=false,community=false;
        for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--help"){std::cout<<help;return 0;}if(key=="--reference"){reference=true;continue;}if(key=="--community"){community=true;continue;}
            if(key!="--community-state"&&key!="--seed"&&key!="--population"&&key!="--days"&&key!="--seconds"&&key!="--scenario"&&key!="--load"&&key!="--save"&&key!="--summary"&&key!="--actors"&&key!="--social-state"&&key!="--trace"&&key!="--trace-actor"&&key!="--thoughts"&&key!="--workers"&&key!="--social-scene"&&key!="--work-chunk")throw std::invalid_argument("unknown argument: "+key);
            if(i+1>=argc||options.contains(key))throw std::invalid_argument("missing or duplicated argument: "+key);
            options[key]=argv[++i];
        }
        if(options.contains("--days")&&options.contains("--seconds"))throw std::invalid_argument("choose days or seconds");
        if(options.contains("--load")&&(options.contains("--seed")||options.contains("--population")||options.contains("--scenario")))throw std::invalid_argument("loaded state defines seed/population/scenario");
        const auto seed=options.contains("--seed")?number(options["--seed"]):42;
        const auto n=options.contains("--population")?number(options["--population"]):128;
        auto seconds=options.contains("--seconds")?number(options["--seconds"]):86400;
        if(options.contains("--days")){const auto days=number(options["--days"]);if(days>366)throw std::invalid_argument("maximum duration 366 days");seconds=days*86400;}
        const auto setup_start=std::chrono::steady_clock::now();
        auto world=options.contains("--load")?life::World::load(options["--load"],!reference):life::World::generate(seed,std::size_t(n),options.contains("--scenario")?options["--scenario"]:"normal");
        if(options.contains("--social-scene")){if(options.contains("--load"))throw std::invalid_argument("scene cannot alter loaded world");world.configure_social_scene(options["--social-scene"]);}
        if(community){if(options.contains("--load"))throw std::invalid_argument("cannot configure loaded world");world.configure_community();}
        world.set_indexed(!reference);
        const auto worker_count=options.contains("--workers")?number(options["--workers"]):1;
        if(worker_count<1||worker_count>32)throw std::invalid_argument("workers must be 1..32");
        world.set_workers(unsigned(worker_count),options.contains("--work-chunk")?number(options["--work-chunk"]):64);
        std::optional<std::ofstream> trace,thoughts;
        const auto trace_actor=options.contains("--trace-actor")?number(options["--trace-actor"]):0;
        if(trace_actor>world.state().actors.size())throw std::invalid_argument("trace actor outside population");
        if(options.contains("--thoughts")){
            thoughts.emplace(output(options["--thoughts"]));world.set_thought_logger([&](const life::Thought& t){if(trace_actor&&t.actor!=trace_actor)return;*thoughts<<life::thought_json(t)<<'\n';});
        }
        if(options.contains("--trace")){
            trace.emplace(output(options["--trace"]));world.set_logger([&](const life::EventLog& e){if(trace_actor&&e.actor!=trace_actor)return;
                *trace<<"{\"ms\":"<<e.time<<",\"actor\":"<<e.actor<<",\"event\":"<<e.event<<",\"method\":\""<<life::method_name(e.method)<<"\",\"kind\":\""<<e.kind<<"\",\"result\":\""<<e.result<<"\",\"place\":"<<e.place<<",\"partner\":"<<e.partner<<",\"object\":"<<e.object<<",\"parent\":"<<e.parent<<",\"interaction\":\""<<life::interaction_name(e.interaction)<<"\",\"score\":"<<e.score<<",\"moral\":"<<e.moral<<",\"risk\":"<<e.risk<<",\"operations\":"<<e.operations<<",\"slots\":"<<e.slots<<"}\n";
            });
        }
        const auto started=std::chrono::steady_clock::now();world.run_seconds(seconds);const auto finished=std::chrono::steady_clock::now();
        auto summary=world.summary_json();summary.pop_back();summary+=",\"mode\":\""+std::string(reference?"reference":"indexed")+"\",\"setup_seconds\":"+std::to_string(std::chrono::duration<double>(started-setup_start).count())+",\"wall_seconds\":"+std::to_string(std::chrono::duration<double>(finished-started).count())+"}";
        std::cout<<summary<<'\n';if(options.contains("--summary")){auto out=output(options["--summary"]);out<<summary<<'\n';}
        if(options.contains("--save"))world.save(options["--save"]);
        if(options.contains("--community-state")){auto out=output(options["--community-state"]);out<<world.community_report_json()<<'\n';}
        if(options.contains("--social-state")){auto out=output(options["--social-state"]);out<<world.social_report_json()<<'\n';}
        if(options.contains("--actors")){
            auto out=output(options["--actors"]);out<<"actor,home,household,place,energy,water,sleep_pressure,fatigue,damage,leisure,social,deficit,desire,money,food,violations,failures,critical_seconds,phase,method\n";
            for(const auto& a:world.state().actors)out<<a.id<<','<<a.home<<','<<a.household<<','<<a.place<<','<<a.body.energy<<','<<a.body.water<<','<<a.body.sleep<<','<<a.body.fatigue<<','<<a.body.damage<<','<<a.leisure<<','<<a.social<<','<<a.desire.deficit<<','<<a.desire.value<<','<<a.money<<','<<a.food<<','<<a.violations<<','<<a.failures<<','<<a.critical_seconds<<','<<unsigned(a.action.phase)<<','<<life::method_name(a.action.method)<<'\n';
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
