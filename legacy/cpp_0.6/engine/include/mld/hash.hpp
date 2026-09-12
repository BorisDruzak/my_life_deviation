#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace mld {
    std::string sha256(std::string_view bytes);
    std::uint64_t seeded(std::uint64_t seed,std::string_view catalog,std::string_view stage,std::uint64_t entity,std::uint64_t draw=0,std::uint64_t retry=0);
    std::uint64_t uniform(std::uint64_t seed,std::string_view catalog,std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t bound);
}
