#include "mld/internal.hpp"
#include <tuple>
namespace mld {
    double clamp(double x,double lo,double hi){if(!std::isfinite(x)||lo>hi)throw std::invalid_argument("non-finite/range");
        return std::clamp(x,lo,hi);
    }
    void require(double x,double lo,double hi){if(!std::isfinite(x)||x<lo||x>hi)throw std::invalid_argument("value outside contract");
    }
    double relax(double x,double target,double seconds,double tau){require(seconds,0,1e15);
        require(tau,1e-15,1e15);
        require(x,-1e6,1e6);
        require(target,-1e6,1e6);
        return seconds==0?x:target+(x-target)*std::exp(-seconds/tau);
    }
    double reservoir(double x,double a,double b,double leak,double h){require(x);
        for(double v:{a,b,leak,h})require(v,0,1e15);
        if(h==0)return x;
        double rate=a+b;
        return rate==0?clamp(x-leak*h):clamp(relax(x,(a-leak)/rate,h,1/rate));
    }

    Body advance_body(const Body& b,const Biology& m,const PhysicalInput& in,const Emotions& e,double seconds){
        require(seconds,0,1e12);
        for(double x:{b.energy,b.water,b.sleep,b.fatigue,b.damage,b.pain,b.oxygen,b.activation})require(x);
        require(b.temp,-1,1);
        for(double x:{in.load,in.oxygen,in.pain,in.disease,in.safety})require(x);
        require(in.temp,-1,1);
        for(double x:{m.energy_use,m.water_use,m.endurance,m.recovery})require(x,.5,1.5);
        require(m.sleep_need,.75,1.5);
        require(m.pain_sensitivity,0,1.5);
        for(double x:e)require(x);
        if(seconds==0)return b;
        const double h=seconds/3600,load=in.load,sleep=in.asleep?1:0;
        Body n=b;
        n.energy=clamp(b.energy-h*.035*m.energy_use*(1+load+.4*std::max(0.,-b.temp)+.15*b.activation));
        n.water=clamp(b.water-h*.045*m.water_use*(1+1.5*load+2*std::max(0.,b.temp)+.15*b.activation));
        double quality=(1-.5*b.pain)*(1-.5*std::abs(b.temp))*(1-.3*b.activation);
        n.sleep=clamp(b.sleep+h*(.045*m.sleep_need*(1-sleep)-.09*m.recovery*quality*sleep));
        n.fatigue=clamp(b.fatigue+h*(.4*load*load*(1+.5*b.oxygen+.3*b.damage)/m.endurance-.25*m.recovery*(1-load)*(.4+.6*sleep)*b.energy*b.water));
        n.oxygen=relax(b.oxygen,in.oxygen,seconds,in.oxygen>b.oxygen?20:10);
        double pain=clamp(m.pain_sensitivity*in.pain);
        n.pain=relax(b.pain,pain,seconds,pain>b.pain?2:30);
        n.temp=relax(b.temp,in.temp,seconds,1800);
        auto sq=[](double x){x=clamp(x);
            return x*x;
        };
        n.damage=clamp(b.damage+h*(in.disease+.03*sq((.1-b.energy)/.1)+.04*sq((.1-b.water)/.1)+.04*sq((std::abs(b.temp)-.7)/.3)+12*sq((b.oxygen-.7)/.3)-.005*m.recovery*b.energy*b.water*(1+.5*sleep)));
        double activation=clamp((1-.5*sleep)*(.1+.5*load+.65*std::max(e[0],e[2])+.3*e[1]+.2*e[4]+.15*e[7]+.4*b.oxygen-.1*in.safety));
        n.activation=relax(b.activation,activation,seconds,activation>b.activation?5:45);
        return n;
    }

    Emotions emotion_targets(const Appraisal& a){
        const double z=a.significance,v=a.harm,b=a.near,k=a.control;
        for(double x:{z,v,b,k,a.uncertain,a.obstacle,a.blame,a.loss,a.progress,a.gain,a.surprise,a.novelty,a.understand,a.disgust,a.condemn,a.violation,a.responsibility,a.question})require(x);
        require(a.pleasant,-1,1);
        return {z*v*b*(.35+.65*(1-k)),z*v*(1-.8*b)*(.35+.65*a.uncertain)*(1-.5*k),z*std::max(a.obstacle,v*b)*a.blame*(.4+.6*k),z*a.loss*(.4+.6*(1-k)),z*clamp(.5*std::max(0.,a.pleasant)+.3*a.progress+.2*a.gain),z*a.surprise,z*a.disgust,z*clamp(.55*a.novelty*(.3+.7*a.understand)+.3*std::max(0.,a.pleasant)+.15*a.question)*(1-.7*v*b),z*a.condemn,z*a.violation*a.responsibility};
    }

    std::array<double,8> capacities(const Body& b,const Biology& m,bool asleep,bool conscious){
        static constexpr double weights[8][7]={{.35,.10,.15,.10,.25,.20,.20},{.60,.15,.20,.10,.50,.25,.45},{.50,.10,.20,.10,.35,.20,.40},{.30,.10,.15,.05,.25,.15,.25},{.40,.20,.20,.10,.30,.20,.30},{.45,.15,.15,.10,.30,.20,.45},{.40,.10,.15,.10,.25,.15,.50},{.45,.15,.15,.10,.35,.20,.35}};
        std::array<double,7> d={clamp((b.sleep-.3)/.7),b.fatigue,clamp((.5-b.water)/.5),clamp((.35-b.energy)/.35),b.pain,std::abs(b.temp),clamp((b.activation-.65)/.35)};
        std::array<double,8> c{};
        double gate=(!asleep&&conscious)?clamp((.95-b.oxygen)/.35):0;
        for(int j=0;j<8;++j){require(m.intellect[j]);
            c[j]=m.intellect[j]*gate;
            for(int i=0;i<7;++i)c[j]*=1-weights[j][i]*d[i];
        }return c;
    }
    std::array<double,8> body_signals(const Body& b){std::array<double,8> d={clamp((.65-b.energy)/.65),clamp((.70-b.water)/.70),clamp((b.sleep-.20)/.80),b.fatigue,b.pain,clamp((-b.temp-.20)/.80),clamp((b.temp-.20)/.80),b.oxygen};
        for(double& x:d)x=clamp(x*(1+.5*x));
        return d;
    }
    int context_slots(const std::array<double,8>& c,bool gate){return gate?std::clamp(int(std::floor(2+6*c[2]+4*c[1])),2,12):0;
    }
    int operations(const std::array<double,8>& c,bool gate){return gate?4+int(std::floor(28*c[4])):0;
    }

    Desire advance_desire(Desire d,double trigger,double inhibition,double seconds){
        for(double x:{d.deficit,d.satiation,d.value})require(x);
        require(d.background,0,.4);
        require(d.sensitivity,0,.8);
        require(d.rate,0,.1);
        require(trigger,-.5,.5);
        require(inhibition);
        require(seconds,0,1e12);
        if(seconds==0)return d;
        // Reference Jacobi step: V uses OLD D/R; only D/R transitions are exact on long intervals.
        double target=clamp((d.background+d.sensitivity*d.deficit+trigger)*(1-d.satiation)-inhibition);
        d.value=relax(d.value,target,seconds,target>d.value?30:300);
        d.deficit=1-(1-d.deficit)*std::exp(-d.rate*seconds/3600);
        d.satiation*=std::exp(-.08664339756999316*seconds/3600);
        return d;
    }
    void satisfy(Desire& d,double s){require(s);
        d.deficit*=1-s;
        d.satiation+=s*(1-d.satiation);
    }

    double pleasure(double b,double innate,double acquired,double relation,double context,double involvement,double sat,double sensitivity,double cost){
        require(b,-.5,.5);
        require(innate,-.5,.5);
        require(acquired,-.5,.5);
        require(relation,-.25,.25);
        require(context,-.25,.25);
        require(involvement);
        require(sat);
        require(sensitivity,0,.8);
        require(cost);
        double raw=b+innate+acquired+relation+context;
        return involvement*clamp((1-sensitivity*sat)*std::max(0.,raw)+std::min(0.,raw)-cost,-1,1);
    }

    void learn_association(Stats& s,std::optional<double> y,double learning,double q,double memory,double dose,bool conscious){
        for(double x:{learning,q,memory,dose})require(x);
        if(!y||!conscious)return;
        require(*y,-1,1);
        double a=.25*learning*q*dose*(.6+.4*memory);
        if(a==0)return;
        double err=*y-s.mean;
        s.mean+=a*err;
        s.variance=(1-a)*(s.variance+a*err*err);
        s.count+=q*dose;
    }

    static std::vector<Feature> select(std::vector<Feature> f){
        for(auto x:f)require(x.exposure);
        std::sort(f.begin(),f.end(),[](auto a,auto b){return a.id<b.id;});
        std::vector<Feature> unique;
        for(auto x:f){if(!unique.empty()&&unique.back().id==x.id)unique.back().exposure=std::max(x.exposure,unique.back().exposure);
            else unique.push_back(x);
        }
        std::sort(unique.begin(),unique.end(),[](auto a,auto b){return a.exposure!=b.exposure?a.exposure>b.exposure:a.id<b.id;});
        if(unique.size()>16)unique.resize(16);
        return unique;
    }
    static double weight_at(const Weight& w,Time now){if(now<w.updated)throw std::invalid_argument("reaction time regression");
        return w.value*std::exp(-std::log(2.)*double(now-w.updated)/(365.*86400000));
    }

    std::array<double,6> Reactions::effects(std::vector<Feature> f,Time now) const {
        auto chosen=select(std::move(f));
        std::sort(chosen.begin(),chosen.end(),[](auto a,auto b){return a.id<b.id;});
        std::array<double,6> sum{};
        for(auto x:chosen)for(std::uint8_t j=0;j<6;++j){auto it=std::lower_bound(weights.begin(),weights.end(),std::pair{x.id,j},[](const Weight& w,auto k){return std::pair{w.feature,w.channel}<k;});
            if(it!=weights.end()&&it->feature==x.id&&it->channel==j)sum[j]+=x.exposure*weight_at(*it,now);
        }
        static constexpr double scales[6]={.5,.5,.5,.3,.3,.5};
        for(int j=0;j<6;++j)sum[j]=scales[j]*clamp(sum[j],-1,1);
        return sum;
    }

    std::size_t Reactions::learn(std::uint64_t episode,std::vector<Feature> f,const std::array<std::optional<double>,6>& u,double plasticity,double dose,Time now,std::size_t address_properties){
        require(plasticity);
        require(dose);
        for(auto x:u)if(x)require(*x,-1,1);
        if(episode<=last_episode)return 0;
        last_episode=episode;
        if(plasticity*dose==0)return 0;
        auto chosen=select(std::move(f));
        std::vector<int> channels;
        for(int j=0;j<6;++j)if(u[j]&&std::abs(*u[j])>=.1)channels.push_back(j);
        std::sort(channels.begin(),channels.end(),[&](int a,int b){return std::abs(*u[a])!=std::abs(*u[b])?std::abs(*u[a])>std::abs(*u[b]):a<b;});
        if(channels.size()>2)channels.resize(2);
        std::size_t created=0;
        for(std::size_t i=0;i<std::min<std::size_t>(2,chosen.size());++i){if(chosen[i].exposure<.25)continue;
            for(int j:channels){auto has=std::find_if(weights.begin(),weights.end(),[&](auto w){return w.feature==chosen[i].id&&w.channel==j;});
                if(has==weights.end()&&weights.size()+address_properties<512){weights.push_back({chosen[i].id,std::uint8_t(j),0,now,now});
                    ++created;
                }}}
        std::sort(weights.begin(),weights.end(),[](auto a,auto b){return std::tie(a.feature,a.channel)<std::tie(b.feature,b.channel);});
        auto update_features=chosen;
        std::sort(update_features.begin(),update_features.end(),[](auto a,auto b){return a.id<b.id;});
        for(int j=0;j<6;++j){if(!u[j])continue;
            double prediction=0,norm=0;
            std::vector<std::pair<std::size_t,double>> active;
            for(auto feature:update_features){
                auto it=std::lower_bound(weights.begin(),weights.end(),std::pair{feature.id,std::uint8_t(j)},[](const Weight& w,auto key){return std::pair{w.feature,w.channel}<key;});
                if(it==weights.end()||it->feature!=feature.id||it->channel!=j)continue;
                auto& w=*it;
                w.value=weight_at(w,now);
                w.updated=now;
                w.active=now;
                prediction+=feature.exposure*w.value;
                norm+=feature.exposure*feature.exposure;
                active.emplace_back(std::size_t(it-weights.begin()),feature.exposure);
            }
            double error=*u[j]-clamp(prediction,-1,1),eta=.02*plasticity*dose;
            for(auto [k,x]:active)weights[k].value=clamp(weights[k].value+eta*x/std::max(1.,norm)*error,-1,1);
        }return created;
    }

    std::optional<Fact> infer(const Rule& r,const std::vector<Fact>& facts,Id object,int& budget){
        if(r.conditions.size()>6)throw std::invalid_argument("rule conditions > 6");
        require(r.mastery);
        if(budget<=0)return {};
        --budget;
        if(r.mastery<.6)return {};
        double confidence=r.mastery;
        for(auto condition:r.conditions){
            bool found=false;
            for(const auto& fact:facts){
                if(fact.predicate!=condition.predicate||fact.object!=object)continue;
                require(fact.confidence);
                if(fact.truth!=condition.required||fact.truth==Truth::Unknown||fact.truth==Truth::Conflict)return {};
                confidence=std::min(confidence,fact.confidence);found=true;
            }
            if(!found)return {};
        }
        return Fact{r.conclusion,object,Truth::True,confidence,r.id};
    }
    double moral_cost(double w,double severity){require(w);
        require(severity);
        return clamp(w*severity);
    }
    double expected_sanction(double a,double b,double c,double severity){for(double x:{a,b,c,severity})require(x);
        return a*b*c*severity;
    }
    Dependence dependence(const std::vector<DependenceInput>& inputs){double all=0,known=0,sum=0;
        for(auto i:inputs){require(i.importance);
            all+=i.importance;
            if(i.contribution)require(*i.contribution);
            if(i.substitution)require(*i.substitution);
            if(i.contribution&&i.substitution){known+=i.importance;
                sum+=i.importance**i.contribution*(1-*i.substitution);
            }}
        if(all==0)return {0,0};
        return {known>0?std::optional<double>(sum/known):std::nullopt,known/all};
    }
}
