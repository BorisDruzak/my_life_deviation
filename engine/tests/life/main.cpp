#include "test.hpp"
int main(int argc,char** argv){
 std::cout.setf(std::ios::unitbuf);
 const std::string group=argc>1?argv[1]:"all"; int count=0,failed=0;
 for(const auto& c:test::cases()) if(group=="all"||group==c.group||group==c.name){ ++count;try{c.run();std::cout<<"PASS "<<c.name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<c.name<<": "<<e.what()<<'\n';} }
 std::cout<<"RESULT "<<count<<" cases, "<<failed<<" failures\n";
 return count==0||failed!=0?1:0;
}
