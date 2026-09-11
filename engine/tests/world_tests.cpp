#include "mld/world.hpp"
#include "check.hpp"
#include <sstream>
using namespace mld;
int main(){
    test("SHA256 empty",[]{check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");});
    test("SHA256 abc",[]{check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");});
    test("seed canonical reproducibility",[]{check(seeded(42,"catalog","history",3)==seeded(42,"catalog","history",3));check(seeded(42,"catalog","history",3)!=seeded(42,"catalog","history",4));});
    test("population creation",[]{auto s=generate({});check(s.npcs.size()==128);validate(s);});
    test("100 seed social invariants",[]{for(unsigned seed=0;seed<100;++seed){Config c;c.seed=seed;auto s=generate(c);check(s.npcs.size()==128);validate(s);}});
    test("invalid profile rejected",[]{Config c;c.profile="full-institutions";rejects([&]{generate(c);});});
    test("same seed same state",[]{check(semantic_hash(generate({}))==semantic_hash(generate({})));});
    test("different seeds differ",[]{Config b;b.seed=43;check(semantic_hash(generate({}))!=semantic_hash(generate(b)));});
    test("snapshot complete and tamper rejection",[]{auto s=generate({});auto b=encode(s);check(encode(decode(b))==b);b.back()^=1;rejects([&]{decode(b);});});

    test("closed loop advances and eats",[]{Config c;c.population=16;World w(c);w.advance(86400);check(w.state().counters.eaten>0);check(w.state().counters.drunk>0);check(w.state().now==86400000);});
    test("reference indexed identical",[]{Config c;c.population=16;World a(c,true),b(c,false);a.advance(7200);b.advance(7200);check(a.snapshot()==b.snapshot());});
    test("restore continuation",[]{Config c;c.population=16;World a(c);a.advance(371);auto b=World::restore(a.snapshot());a.advance(7200);b.advance(7200);check(a.snapshot()==b.snapshot());});
    test("zero advance stable",[]{Config c;c.population=16;World a(c);a.advance(1);auto before=a.snapshot();a.advance(0);check(a.snapshot()==before);});
    test("controller budget",[]{World w;auto v=w.view(0);v.sensed.fill(.5);auto c=choose(v);check(c.used_operations<=operations(v.cognition));check(c.context_used<=context_slots(v.cognition));});
    test("same view hidden food",[]{auto s=generate({});World a(s);s.places[1].food+=10;s.counters.initial_food+=10;World b(s);auto x=choose(a.view(0)),y=choose(b.view(0));check(x.type==y.type&&x.target==y.target&&x.score==y.score);});

    test("initial zero advance is identity",[]{World a;auto s=a.snapshot();a.advance(0);check(a.snapshot()==s);});
    test("contact retry uses known refusal",[]{World w;auto v=w.view(0);Knowledge k=*v.knowledge;v.knowledge=&k;v.sensed.fill(0);v.sensed[5]=.9;v.seen_people.clear();v.seen_people.push_back(k.people.front().person);k.people.front().retry=v.now+60000;check(choose(v).type!=Act::Talk);});
    test("known taboo changes choice not physics",[]{World w;auto v=w.view(0);Knowledge k=*v.knowledge;v.knowledge=&k;v.place=1;v.food=0;v.money=0;v.sensed.fill(0);v.sensed[0]=1;k.property_norm=0;k.sanction=0;for(auto& p:k.places)if(p.place==v.home){p.food=Truth::False;p.retry=60000;}check(choose(v).type==Act::Take);k.property_norm=1;check(choose(v).type!=Act::Take);});

    test("minimal context can execute familiar bound procedure",[]{World w;auto v=w.view(0);v.cognition.fill(.01);v.food=1;v.sensed.fill(0);v.sensed[0]=1;check(context_slots(v.cognition)==2);check(choose(v).type==Act::Eat);});
    test("route preserves known procedure across intermediate node",[]{World w;auto v=w.view(0);v.sensed.fill(.1);v.sensed[5]=.9;v.place=0;v.money=4;v.intent={Act::Drink,2,1,true};auto c=choose(v);check(c.type==Act::Move&&c.target==2&&c.procedure==Act::Drink);});
    test("waiting forecast does not exceed known conversation benefit",[]{World w;auto v=w.view(0);Knowledge k=*v.knowledge;v.knowledge=&k;v.place=3;v.seen_people.clear();v.sensed.fill(.1);v.sensed[5]=.9;v.sensed[4]=.2;k.expectation[std::size_t(Act::Talk)][5].mean=0;auto c=choose(v);check(c.type==Act::Play);});

    test("last portion atomically transferred once",[]{
        Config c;c.population=8;auto s=generate(c);s.counters.initial_food+=1-s.places[1].food;s.places[1].food=1;
        for(auto& n:s.npcs)n.next_review=30000;
        for(Id id:{0u,1u}){auto& n=s.npcs[id];n.place=1;n.action.type=Act::Exchange;n.action.start=0;n.action.end=1000;n.action.target=1;n.action.id=++s.sequence;}
        auto money=s.npcs[0].money+s.npcs[1].money;World w(s);w.advance(1);
        check(w.state().counters.transfers==1&&w.state().counters.failures==1);
        check(w.state().npcs[0].money+w.state().npcs[1].money==money-1);
        check(w.state().places[1].food==0);validate(w.state());
    });
    test("eating does not charge money",[]{
        Config c;c.population=8;auto s=generate(c);for(auto& n:s.npcs)n.next_review=30000;
        auto& n=s.npcs[0];n.action.type=Act::Eat;n.action.start=0;n.action.end=1000;n.action.target=n.place;n.action.id=++s.sequence;
        auto money=n.money;World w(s);w.advance(1);check(w.state().counters.eaten==1&&w.state().npcs[0].money==money);
    });
    test("physical take not blocked by moral norm",[]{
        Config c;c.population=8;auto s=generate(c);for(auto& n:s.npcs)n.next_review=30000;
        auto& n=s.npcs[0];n.memory.property_norm=1;n.place=1;n.action.type=Act::Take;n.action.end=1000;n.action.target=1;n.action.id=++s.sequence;
        auto money=n.money;World w(s);w.advance(1);check(w.state().npcs[0].unowned_food==1&&w.state().npcs[0].money==money);check(w.state().counters.unauthorized==1);
    });
    test("restored completion has one effect",[]{
        Config c;c.population=8;auto s=generate(c);for(auto& n:s.npcs)n.next_review=30000;
        auto& n=s.npcs[0];n.action.type=Act::Relief;n.action.end=1000;n.action.target=n.home;n.action.id=++s.sequence;
        World a(s);auto b=World::restore(a.snapshot());a.advance(1);b.advance(1);check(a.snapshot()==b.snapshot()&&a.state().counters.reliefs==1);
    });
    test("logging does not change the world",[]{Config c;c.population=8;World a(c),b(c);std::ostringstream log;b.log_to(&log);a.advance(4000);b.advance(4000);check(a.snapshot()==b.snapshot()&&!log.str().empty());});
    test("snapshot truncation rejected",[]{auto blob=encode(generate({}));blob.resize(blob.size()/2);rejects([&]{decode(blob);});});
    test("invalid pending action rejected",[]{auto s=generate({});s.npcs[0].action.type=Act::Move;s.npcs[0].action.target=99999;s.npcs[0].action.end=1000;rejects([&]{World w(s);});});
    test("invalid parent rejected before ancestry traversal",[]{auto s=generate({});s.persons[0].parent=99999;rejects([&]{validate(s);});});
    test("nonfinite learning state rejected",[]{auto s=generate({});s.npcs[0].memory.expectation[9][4].variance=NAN;rejects([&]{decode(encode(s));});});
    test("one-second versus chunked execution",[]{Config c;c.population=8;World a(c),b(c);a.advance(101);for(int i=0;i<101;++i)b.advance(1);check(a.snapshot()==b.snapshot());});
    test("temptation offers opportunity rather than scripted theft",[]{Config c;c.population=8;c.temptation=true;c.theft_norm_override=0;World low(c);c.theft_norm_override=1;World high(c);low.advance(7200);high.advance(7200);check(low.state().counters.unauthorized>0);check(high.state().counters.unauthorized==0);});
    test("legal alternatives survive an empty home source",[]{Config c;c.population=8;c.temptation=true;c.theft_norm_override=1;World world(c);world.advance(86400);for(const auto& n:world.state().npcs)check(n.body.damage==0);check(world.state().counters.wages>0&&world.state().counters.eaten>0);});
    return result();
}
