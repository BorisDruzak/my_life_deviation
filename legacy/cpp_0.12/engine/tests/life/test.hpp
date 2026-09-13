#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace test {
struct Case { std::string group,name; std::function<void()> run; };
inline std::vector<Case>& cases(){ static std::vector<Case> c; return c; }
struct Register { Register(const char* g,const char* n,std::function<void()> f){ cases().push_back({g,n,std::move(f)}); } };
inline void check(bool condition,const char* expression,const char* file,int line){ if(!condition) throw std::runtime_error(std::string(file)+":"+std::to_string(line)+": "+expression); }
}
#define TEST(G,N) static void N(); static test::Register reg_##N(G,#N,N); static void N()
#define CHECK(X) test::check(bool(X),#X,__FILE__,__LINE__)
#define NEAR(A,B,E) CHECK(std::abs((A)-(B))<=(E))
#define THROWS(X) do { bool threw=false;try{ X; }catch(const std::exception&){threw=true;}CHECK(threw); }while(false)
