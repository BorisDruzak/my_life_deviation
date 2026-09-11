#include "mld/world.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
using namespace mld;
static std::uint64_t number(const std::string& s){std::uint64_t x=0;
    auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),x);
    if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument("invalid unsigned integer: "+s);
    return x;
}
static std::string read_file(const std::string& p){std::ifstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("cannot open "+p);
    return {std::istreambuf_iterator<char>(f),{}};
}
static void write_file(const std::string& p,const std::string& b){std::ofstream f(p,std::ios::binary);
    if(!f||!f.write(b.data(),std::streamsize(b.size())))throw std::runtime_error("cannot write "+p);
}
int main(int argc,char** argv){
    try{
        Config c;
        bool indexed=true;
        std::uint32_t seconds=86400;
        std::string load,save,out,trace,dump;
        bool config_changed=false;
        for(int i=1;i<argc;++i){std::string arg=argv[i];
            auto value=[&](){if(++i>=argc)throw std::invalid_argument("missing value for "+arg);
                return std::string(argv[i]);
            };
            if(arg=="--seed"){c.seed=number(value());
                config_changed=true;
            }
            else if(arg=="--npcs"){auto n=number(value());
                if(n>1024)throw std::invalid_argument("population >1024");
                c.population=std::uint32_t(n);
                config_changed=true;
            }
            else if(arg=="--days"){auto d=number(value());
                if(d>31)throw std::invalid_argument("days >31");
                seconds=std::uint32_t(d*86400);
            }
            else if(arg=="--seconds"){auto t=number(value());
                if(t>31*86400)throw std::invalid_argument("seconds >31 days");
                seconds=std::uint32_t(t);
            }
            else if(arg=="--scarce"){c.scarce=true;
                config_changed=true;
            }
            else if(arg=="--temptation"){c.temptation=true;config_changed=true;}
            else if(arg=="--norm"){auto s=value();
                std::size_t used=0;
                c.theft_norm_override=std::stod(s,&used);
                if(used!=s.size()||!std::isfinite(c.theft_norm_override)||c.theft_norm_override<0||c.theft_norm_override>1)throw std::invalid_argument("norm must be [0,1]");
                config_changed=true;
            }
            else if(arg=="--reference")indexed=false;
            else if(arg=="--load")load=value();
            else if(arg=="--save")save=value();
            else if(arg=="--out")out=value();
            else if(arg=="--trace")trace=value();
            else if(arg=="--dump-npcs")dump=value();
            else if(arg=="--help"){std::cout<<"mld_sim --seed 42 --npcs 128 --days 7 [--scarce | --temptation] [--norm 0..1] [--reference]\n        [--load file] [--save file] [--out summary.json] [--trace events.csv] [--dump-npcs actors.csv]\nModel: 0.6.0-lab1, partial BEHAVIOR-0.3 laboratory; no full institutions or UE5.\n";
                return 0;
            }
            else throw std::invalid_argument("unknown argument: "+arg);
        }
        if(!load.empty()&&config_changed)throw std::invalid_argument("snapshot owns configuration; overrides are not migration");
        auto start=std::chrono::steady_clock::now();
        World world=load.empty()?World(c,indexed):World::restore(read_file(load),indexed);
        auto generated=std::chrono::steady_clock::now();
        std::ofstream log;
        if(!trace.empty()){log.open(trace);
            if(!log)throw std::runtime_error("cannot open trace");
            log<<"time_ms,event,actor,target,value\n";
            world.log_to(&log);
        }
        world.advance(seconds);
        auto simulated=std::chrono::steady_clock::now();
        auto summary=world.summary();
        summary.erase(summary.rfind('}'));
        summary+=",\"generation_wall_seconds\":"+std::to_string(std::chrono::duration<double>(generated-start).count())+",\"simulation_wall_seconds\":"+std::to_string(std::chrono::duration<double>(simulated-generated).count())+",\"search_mode\":\""+(indexed?std::string("indexed"):std::string("reference"))+"\"\n}\n";
        std::cout<<summary;
        if(!out.empty())write_file(out,summary);
        if(!save.empty())write_file(save,world.snapshot());
        if(!dump.empty()){std::ofstream f(dump);
            if(!f)throw std::runtime_error("cannot open NPC dump");
            f<<"id,energy,water,sleep,fatigue,leisure,social,desire,place,action,money,food\n";
            for(auto& n:world.state().npcs)f<<n.id<<','<<n.body.energy<<','<<n.body.water<<','<<n.body.sleep<<','<<n.body.fatigue<<','<<n.leisure<<','<<n.social<<','<<n.desire.value<<','<<n.place<<','<<action_name(n.action.type)<<','<<n.money<<','<<n.food+n.unowned_food<<'\n';
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"mld_sim: "<<e.what()<<'\n';
        return 2;
    }
}
