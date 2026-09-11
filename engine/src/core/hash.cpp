#include "mld/hash.hpp"
#include <array>
#include <bit>
#include <vector>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace mld {
    std::string sha256(std::string_view bytes) {
        static constexpr std::uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        std::array<std::uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
        std::vector<unsigned char> data(bytes.begin(),bytes.end());
        const std::uint64_t bits=std::uint64_t(data.size())*8;
        data.push_back(0x80);
        while(data.size()%64!=56)data.push_back(0);
        for(int i=7;i>=0;--i)data.push_back(static_cast<unsigned char>(bits>>(8*i)));
        for(std::size_t off=0;off<data.size();off+=64){
            std::uint32_t w[64]{};
            for(int i=0;i<16;++i)for(int j=0;j<4;++j)w[i]=(w[i]<<8)|data[off+i*4+j];
            for(int i=16;i<64;++i){auto a=w[i-15],b=w[i-2];
                w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10));
            }
            auto [a,b,c,d,e,f,g,z]=h;
            for(int i=0;i<64;++i){auto t1=z+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
                auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
                z=g;
                g=f;
                f=e;
                e=d+t1;
                d=c;
                c=b;
                b=a;
                a=t1+t2;
            }
            h[0]+=a;
            h[1]+=b;
            h[2]+=c;
            h[3]+=d;
            h[4]+=e;
            h[5]+=f;
            h[6]+=g;
            h[7]+=z;
        }
        std::ostringstream out;
        out<<std::hex<<std::setfill('0');
        for(auto x:h)out<<std::setw(8)<<x;
        return out.str();
    }
    static std::string ascii(std::string_view s){for(unsigned char c:s)if(c<32||c>126||c=='"'||c=='\\')throw std::invalid_argument("RNG key must be canonical ASCII identifier");
        return std::string(s);
    }

    std::uint64_t seeded(std::uint64_t seed,std::string_view catalog,std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t retry){
        std::ostringstream hex;
        hex<<std::hex<<std::setw(16)<<std::setfill('0')<<seed;
        const auto key="[\""+hex.str()+"\",\"0.1.0\",\""+ascii(catalog)+"\",\""+ascii(stage)+"\",\""+std::to_string(entity)+"\",\""+std::to_string(draw)+"\",\""+std::to_string(retry)+"\"]";
        return std::stoull(sha256(key).substr(0,16),nullptr,16);
    }

    std::uint64_t uniform(std::uint64_t seed,std::string_view cat,std::string_view stage,std::uint64_t entity,std::uint64_t draw,std::uint64_t bound){
        if(bound==0)throw std::invalid_argument("zero random bound");
        // Accept exactly [0, floor(2^64 / bound) * bound), with no overflow at 2^64.
        const auto rem=(std::uint64_t(0)-bound)%bound;
        for(std::uint64_t retry=0;;++retry){auto x=seeded(seed,cat,stage,entity,draw,retry);
            if(rem==0||x<=UINT64_MAX-rem)return x%bound;
        }
    }
}
