#pragma once
#include <boost/json.hpp>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace npc {
    using Json = boost::json::value;
    using Object = boost::json::object;
    using Array = boost::json::array;
    using Id = std::string;
    using Minute = std::int64_t;
    using Money = std::int64_t;
    inline constexpr char rules_version[] = "WORLD-0.5.0";
    inline constexpr Money max_money =(std::int64_t {
        1
    }
    << 50);
    inline constexpr Minute max_minute =(std::int64_t {
        1
    }
    << 40);
    struct InputError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    struct InvariantError : std::logic_error {
        using std::logic_error::logic_error;
    };
    Json parse_json(std::string_view text);
    Json read_json(const std::filesystem::path & path);
    std::string canonical(const Json & value);
    void write_json(const std::filesystem::path & path, const Json & value);
    const Json & at(const Json & value, std::string_view key);
    bool has(const Json & value, std::string_view key);
    template < class T > struct is_vector : std::false_type {};
    template < class T, class A > struct is_vector < std::vector < T, A >> : std::true_type {};
    template < class T > struct is_set : std::false_type {};
    template < class T, class C, class A > struct is_set < std::set < T, C, A >> : std::true_type {};
    template < class T > struct is_map : std::false_type {};
    template < class V, class C, class A > struct is_map < std::map < std::string, V, C, A >> : std::true_type {};
    template < class T > struct is_optional : std::false_type {};
    template < class T > struct is_optional < std::optional < T >> : std::true_type {};
    template < class T > Json encode(const T & value);
    template < class T > T decode(const Json & value);
    template < class T > Json encode(const T & value) {
        if constexpr(std::is_same_v < T, Json >) return value;
        else if constexpr(std::is_same_v < T, std::string >) return Json(value);
        else if constexpr(std::is_arithmetic_v < T >) {
            if constexpr(std::is_floating_point_v < T >) if (! std::isfinite(value)) throw InputError("non-finite JSON number");
            return Json(value);
        } else if constexpr(is_optional < T >::value) return value ? encode(* value) : Json(nullptr);
        else if constexpr(is_vector < T >::value || is_set < T >::value) {
            Array out;
            for (const auto & x : value) out.push_back(encode(x));
            return out;
        } else if constexpr(is_map < T >::value) {
            Object out;
            for (const auto &[k, v] : value) out[k] = encode(v);
            return out;
        } else {
            Object out;
            value.visit([&](const char * key, const auto & x) {
                out[key] = encode(x);
            });
            return out;
        }
    }
    template < class T > T decode(const Json & value) {
        if constexpr(std::is_same_v < T, Json >) return value;
        else if constexpr(std::is_same_v < T, std::string >) {
            if (! value.is_string()) throw InputError("expected string");
            return std::string(value.as_string());
        } else if constexpr(std::is_same_v < T, bool >) {
            if (! value.is_bool()) throw InputError("expected bool");
            return value.as_bool();
        } else if constexpr(std::is_integral_v < T >) {
            if (! value.is_int64() && ! value.is_uint64()) throw InputError("expected exact integer");
            if (value.is_int64()) {
                const auto n = value.as_int64();
                if constexpr(std::is_unsigned_v < T >) {
                    if (n < 0) throw InputError("negative unsigned integer");
                }
                if (static_cast < long double >(n) < static_cast < long double >(std::numeric_limits < T >::lowest()) || static_cast < long double >(n) > static_cast < long double >(std::numeric_limits < T >::max())) throw InputError("integer out of range");
                return static_cast < T >(n);
            }
            const auto n = value.as_uint64();
            if (static_cast < long double >(n) > static_cast < long double >(std::numeric_limits < T >::max())) throw InputError("integer out of range");
            return static_cast < T >(n);
        } else if constexpr(std::is_floating_point_v < T >) {
            if (! value.is_number()) throw InputError("expected number");
            const auto n = value.is_double() ? value.as_double() : value.is_int64() ? static_cast < double >(value.as_int64()) : static_cast < double >(value.as_uint64());
            if (! std::isfinite(n)) throw InputError("non-finite number");
            return static_cast < T >(n);
        } else if constexpr(is_optional < T >::value) {
            if (value.is_null()) return {};
            return T {
                decode < typename T::value_type >(value)
            };
        } else if constexpr(is_vector < T >::value) {
            if (! value.is_array()) throw InputError("expected array");
            T out;
            for (const auto & x : value.as_array()) out.push_back(decode < typename T::value_type >(x));
            return out;
        } else if constexpr(is_set < T >::value) {
            if (! value.is_array()) throw InputError("expected set array");
            T out;
            for (const auto & x : value.as_array()) if (! out.insert(decode < typename T::value_type >(x)).second) throw InputError("duplicate set entry");
            return out;
        } else if constexpr(is_map < T >::value) {
            if (! value.is_object()) throw InputError("expected object");
            T out;
            for (const auto & kv : value.as_object()) out.emplace(std::string(kv.key()), decode < typename T::mapped_type >(kv.value()));
            return out;
        } else {
            if (! value.is_object()) throw InputError("expected structured object");
            T out;
            std::set < std::string > allowed;
            out.visit([&](const char * key, auto & x) {
                allowed.insert(key);
                if (const auto * v = value.as_object().if_contains(key)) x = decode < std::decay_t < decltype(x) >>(* v);
            });
            for (const auto & kv : value.as_object()) if (! allowed.contains(std::string(kv.key()))) throw InputError("unknown field: " + std::string(kv.key()));
            return out;
        }
    }
    template < class T > T get(const Json & j, std::string_view key) {
        return decode < T >(at(j, key));
    }
    template < class T > T get_or(const Json & j, std::string_view key, T fallback) {
        return has(j, key) ? get < T >(j, key) : fallback;
    }
    inline std::string str(const Json & j, std::string_view key) {
        return get < std::string >(j, key);
    }
    inline std::string text(const Json & j, std::string_view key, std::string fallback = {}) {
        return get_or < std::string >(j, key, std::move(fallback));
    }
}
