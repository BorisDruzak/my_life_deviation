#pragma once
#include <iostream>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <string>
inline int checks=0,failures=0;
inline void test(const std::string& name,const std::function<void()>& f){++checks;try{f();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
inline void check(bool b,const char* s="check"){if(!b)throw std::runtime_error(s);}
inline void near(double a,double b,double eps=1e-12){check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps,"numeric mismatch");}
inline void rejects(const std::function<void()>& f){bool ok=false;try{f();}catch(const std::exception&){ok=true;}check(ok,"expected rejection");}
inline int result(){std::cout<<"tests="<<checks<<" failed="<<failures<<'\n';return failures?1:0;}
