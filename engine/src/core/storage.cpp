#include "mld/world.hpp"
#include <bit>
#include <type_traits>
namespace mld {
    class Writer {
        public:
        std::string data;
        template<class... T>void operator()(T&... x){(write(x),...);
        }
        template<class T> void write(T& x){
            if constexpr(std::is_enum_v<T>){auto v=std::underlying_type_t<T>(x);
                write(v);
            }
            else if constexpr(std::is_same_v<T,bool>){data.push_back(x?1:0);
            }
            else if constexpr(std::is_integral_v<T>){using U=std::make_unsigned_t<T>;
                auto v=static_cast<U>(x);
                for(int i=int(sizeof(T))-1;i>=0;--i)data.push_back(char(v>>(i*8)));
            }
            else if constexpr(std::is_same_v<T,double>){if(!std::isfinite(x))throw std::invalid_argument("non-finite snapshot");
                auto v=std::bit_cast<std::uint64_t>(x);
                write(v);
            }
            else if constexpr(std::is_same_v<T,std::string>){auto n=std::uint64_t(x.size());
                write(n);
                data+=x;
            }
            else if constexpr(requires {x.io(*this);})x.io(*this);
            else if constexpr(requires {x.resize(0);}){auto n=std::uint64_t(x.size());
                write(n);
                for(auto& v:x)write(v);
            }
            else for(auto& v:x)write(v);
        }
    };
    class Reader {
        const std::string& data;
        std::size_t pos=0;
        unsigned char byte(){if(pos>=data.size())throw std::invalid_argument("truncated snapshot");
            return static_cast<unsigned char>(data[pos++]);
        }
        public:
        explicit Reader(const std::string& s):data(s){}
        bool done()const{return pos==data.size();
        }
        template<class... T>void operator()(T&... x){(read(x),...);
        }
        template<class T>void read(T& x){
            if constexpr(std::is_enum_v<T>){std::underlying_type_t<T> v{};
                read(v);
                x=static_cast<T>(v);
            }
            else if constexpr(std::is_same_v<T,bool>){auto v=byte();
                if(v>1)throw std::invalid_argument("invalid bool");
                x=v!=0;
            }
            else if constexpr(std::is_integral_v<T>){using U=std::make_unsigned_t<T>;
                U v=0;
                for(std::size_t i=0;i<sizeof(T);++i)v=(v<<8)|byte();
                if constexpr(std::is_signed_v<T>)x=std::bit_cast<T>(v);
                else x=v;
            }
            else if constexpr(std::is_same_v<T,double>){std::uint64_t v=0;
                read(v);
                x=std::bit_cast<double>(v);
                if(!std::isfinite(x))throw std::invalid_argument("non-finite snapshot");
            }
            else if constexpr(std::is_same_v<T,std::string>){std::uint64_t n=0;
                read(n);
                if(n>data.size()-pos)throw std::invalid_argument("string length");
                x=data.substr(pos,n);
                pos+=n;
            }
            else if constexpr(requires {x.io(*this);})x.io(*this);
            else if constexpr(requires {x.resize(0);}){std::uint64_t n=0;
                read(n);
                if(n>1000000||n>data.size()-pos)throw std::invalid_argument("container length");
                x.resize(n);
                for(auto& v:x)read(v);
            }
            else for(auto& v:x)read(v);
        }
    };
    std::string encode(State s){Writer w;
        w(s);
        return std::string("MLD-0.6-lab1\n")+catalog_hash()+"\n"+sha256(w.data)+"\n"+w.data;
    }

    State decode(const std::string& bytes){
        const std::string prefix=std::string("MLD-0.6-lab1\n")+catalog_hash()+"\n";
        if(!bytes.starts_with(prefix)||bytes.size()<prefix.size()+65||bytes[prefix.size()+64]!='\n')throw std::invalid_argument("incompatible snapshot");
        const auto payload=bytes.substr(prefix.size()+65);
        if(sha256(payload)!=bytes.substr(prefix.size(),64))throw std::invalid_argument("snapshot checksum");
        State s;
        Reader r(payload);
        r(s);
        if(!r.done())throw std::invalid_argument("trailing snapshot fields");
        validate(s);
        return s;
    }
    std::string semantic_hash(const State& s){return sha256(encode(s));
    }
}
