#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace life {
std::array<std::uint8_t,32> sha256(std::string_view bytes);
std::string hex_sha256(std::string_view bytes);
struct Seed {
    std::uint64_t value=42;
    std::string generator="0.7.0",catalog;
    std::uint64_t raw(std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t retry=0) const;
    std::uint64_t integer(std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t bound) const;
    double uniform(std::string_view stage,std::uint64_t entity,std::uint64_t draw) const;
    template<class A> void fields(A& a) {a(value,generator,catalog);}
};
}
