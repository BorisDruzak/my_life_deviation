#include "life/seed.hpp"
#include <bit>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace life {
namespace {
constexpr std::uint32_t constants[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
std::string quote_id(std::string_view s){
    // Semantic keys are ASCII identifiers, therefore already NFC; names never enter this API.
    if(s.empty())throw std::invalid_argument("empty seed key component");
    for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'||c==':'))throw std::invalid_argument("seed key must be an ASCII semantic identifier");
    return "\""+std::string(s)+"\"";
}
}
std::array<std::uint8_t,32> sha256(std::string_view input){
    std::vector<std::uint8_t> data(input.begin(),input.end());
    const std::uint64_t bits=std::uint64_t(data.size())*8;
    data.push_back(0x80);
    while(data.size()%64!=56)data.push_back(0);
    for(int i=7;i>=0;--i)data.push_back(std::uint8_t(bits>>(i*8)));
    std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(std::size_t offset=0;offset<data.size();offset+=64){
        std::array<std::uint32_t,64> w{};
        for(std::size_t i=0;i<16;++i){for(int j=0;j<4;++j)w[i]=(w[i]<<8)|data[offset+4*i+j];}
        for(std::size_t i=16;i<64;++i){
            const auto x=w[i-15],y=w[i-2];
            w[i]=w[i-16]+(std::rotr(x,7)^std::rotr(x,18)^(x>>3))+w[i-7]+(std::rotr(y,17)^std::rotr(y,19)^(y>>10));
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(std::size_t i=0;i<64;++i){
            const auto t1=v+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    std::array<std::uint8_t,32> result{};
    for(std::size_t i=0;i<8;++i)for(int j=0;j<4;++j)result[4*i+j]=std::uint8_t(h[i]>>(24-8*j));
    return result;
}
std::string hex_sha256(std::string_view bytes){
    constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(64);
    for(auto b:sha256(bytes)){result.push_back(digits[b>>4]);result.push_back(digits[b&15]);}return result;
}
std::uint64_t Seed::raw(std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t retry)const{
    std::ostringstream seed;seed<<std::hex<<std::setfill('0')<<std::setw(16)<<value;
    const std::string key="["+quote_id(seed.str())+","+quote_id(generator)+","+quote_id(catalog)+","+quote_id(stage)+","+quote_id(std::to_string(entity))+","+quote_id(std::to_string(draw))+","+quote_id(std::to_string(retry))+"]";
    auto hash=sha256(key);std::uint64_t result=0;for(int i=0;i<8;++i)result=(result<<8)|hash[i];return result;
}
std::uint64_t Seed::integer(std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t bound)const{
    if(bound==0)throw std::invalid_argument("zero random bound");
    constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
    const auto remainder=(maximum%bound+1)%bound;
    for(std::uint64_t retry=0;retry<1000000;++retry){const auto x=raw(stage,entity,draw,retry);if(remainder==0||x<=maximum-remainder)return x%bound;}
    throw std::runtime_error("seed rejection limit exceeded");
}
double Seed::uniform(std::string_view stage,std::uint64_t entity,std::uint64_t draw)const{
    return (double(raw(stage,entity,draw)>>12)+.5)/4503599627370496.0;
}
}
