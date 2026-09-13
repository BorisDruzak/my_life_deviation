#include "test.hpp"
#include "life/seed.hpp"
using namespace life;
TEST("seed", sha256_known_vectors){
 CHECK(hex_sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 CHECK(hex_sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 CHECK(hex_sha256(std::string(1000,'a'))=="41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}
TEST("seed", key_matches_python){ Seed s{42,"0.1.0",std::string(64,'0')}; CHECK(s.raw("history",7,3)==5656018312441759080ULL); }
TEST("seed", independent_keyed_draws){ Seed s{42,"0.1.0",std::string(64,'0')};auto a=s.raw("history",7,3);s.raw("history",7,999);CHECK(s.raw("history",7,3)==a);CHECK(s.raw("names",7,3)!=a);CHECK(s.raw("history",7,3,1)!=a); }
TEST("seed", rejection_and_open_uniform){Seed s{42,"0.1.0",std::string(64,'0')};for(int i=0;i<1000;++i){CHECK(s.integer("test",7,i,7)<7);double x=s.uniform("test",7,i);CHECK(x>0&&x<1);}THROWS(s.integer("test",1,0,0));}
