#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
struct Test {
    std::string name;
    std::function < void() > fn;
};
inline std::vector < Test > & test_registry() {
    static std::vector < Test > t;
    return t;
}
struct Register {
    Register(std::string n, std::function < void() > f) {
        test_registry().push_back({
            std::move(n), std::move(f)
        });
    }
};
#define TEST(name) static void name(); static Register reg_##name(#name,name); static void name()
#define CHECK(x) do {if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" CHECK("+#x+")");} while(false)
#define NEAR(a,b,e) do {double aa=(a),bb=(b); if(!std::isfinite(aa)||!std::isfinite(bb)||std::abs(aa-bb)>(e)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" expected "+std::to_string(bb)+", got "+std::to_string(aa));} while(false)
#define THROWS(x) do {bool threw=false;try{static_cast<void>(x);}catch(const std::exception&){threw=true;}CHECK(threw);} while(false)
