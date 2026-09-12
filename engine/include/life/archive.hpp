#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace life {
// Explicit little-endian fields; no struct padding, pointer addresses, or native binary layout.
class Writer {
    template<class T> void one(const T& x){
        if constexpr(std::is_same_v<T,bool>)data.push_back(x?'\1':'\0');
        else if constexpr(std::is_enum_v<T>)one(static_cast<std::underlying_type_t<T>>(x));
        else if constexpr(std::is_integral_v<T>){auto n=static_cast<std::make_unsigned_t<T>>(x);for(std::size_t i=0;i<sizeof(T);++i)data.push_back(char((n>>(8*i))&255));}
        else if constexpr(std::is_same_v<T,double>){static_assert(std::numeric_limits<double>::is_iec559);if(!std::isfinite(x))throw std::runtime_error("non-finite save scalar");one(std::bit_cast<std::uint64_t>(x));}
        else if constexpr(std::is_same_v<T,std::string>){one(std::uint64_t(x.size()));data+=x;}
        else if constexpr(requires{ x.first;x.second; }){one(x.first);one(x.second);}
        else if constexpr(requires(T& y){ y.fields(*this); }){const_cast<T&>(x).fields(*this);}
        else if constexpr(requires(T& y){ y.resize(0); }){one(std::uint64_t(x.size()));for(const auto& y:x)one(y);}
        else {for(const auto& y:x)one(y);}
    }
public:
    std::string data;
    template<class... T> void operator()(const T&... x){(one(x),...);}
};
class Reader {
    std::string_view data_;std::size_t pos_=0,allocated_=0;
    std::uint64_t count(){std::uint64_t n;one(n);if(n>1000000||n>data_.size()-pos_)throw std::runtime_error("invalid archive count");return n;}
    template<class T> void one(T& x){
        if constexpr(std::is_same_v<T,bool>){std::uint8_t v;one(v);if(v>1)throw std::runtime_error("invalid bool");x=v!=0;}
        else if constexpr(std::is_enum_v<T>){std::underlying_type_t<T> v;one(v);x=static_cast<T>(v);}
        else if constexpr(std::is_integral_v<T>){if(data_.size()-pos_<sizeof(T))throw std::runtime_error("truncated archive");std::make_unsigned_t<T> n=0;for(std::size_t i=0;i<sizeof(T);++i)n|=std::make_unsigned_t<T>(static_cast<unsigned char>(data_[pos_++]))<<(8*i);x=std::bit_cast<T>(n);}
        else if constexpr(std::is_same_v<T,double>){std::uint64_t n;one(n);x=std::bit_cast<double>(n);if(!std::isfinite(x))throw std::runtime_error("non-finite load scalar");}
        else if constexpr(std::is_same_v<T,std::string>){auto n=count();allocated_+=std::size_t(n);if(allocated_>256*1024*1024)throw std::runtime_error("archive allocation limit");x=std::string(data_.substr(pos_,n));pos_+=n;}
        else if constexpr(requires{ x.first;x.second; }){one(x.first);one(x.second);}
        else if constexpr(requires{ x.fields(*this); }){x.fields(*this);}
        else if constexpr(requires(T& y){ y.resize(0); }){auto n=count();allocated_+=std::size_t(n)*sizeof(typename T::value_type);if(allocated_>256*1024*1024)throw std::runtime_error("archive allocation limit");x.resize(n);for(auto& y:x)one(y);}
        else {for(auto& y:x)one(y);}
    }
public:
    explicit Reader(std::string_view data):data_(data){}
    template<class... T> void operator()(T&... x){(one(x),...);}
    bool finished()const{return pos_==data_.size();}
};
}
