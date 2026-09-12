#include "life/world.hpp"
#include "life/archive.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace life {
std::string World::hash()const{Writer writer;writer(state_);return hex_sha256(writer.data);}
void World::save(const std::string& path)const{
    validate();Writer writer;writer(state_);
    const std::string data="LIFE-SAVE-0.10.0-r2\n"+hex_sha256(writer.data)+"\n"+writer.data;
    auto temporary=std::filesystem::path(path);temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot create save");out.write(data.data(),std::streamsize(data.size()));out.close();if(!out)throw std::runtime_error("save write failed");}
    // Never delete the prior save before a successful rename; a failed replacement leaves it intact.
    std::filesystem::rename(temporary,path);
}
World World::load(const std::string& path,bool indexed){
    const auto size=std::filesystem::file_size(path);if(size>256*1024*1024)throw std::runtime_error("save exceeds size limit");
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot open save");
    std::string data((std::istreambuf_iterator<char>(in)),{});
    const std::string magic="LIFE-SAVE-0.10.0-r2\n";
    if(!data.starts_with(magic)||data.size()<magic.size()+65||data[magic.size()+64]!='\n')throw std::runtime_error("unsupported save version");
    std::string_view payload(data.data()+magic.size()+65,data.size()-magic.size()-65);
    if(hex_sha256(payload)!=data.substr(magic.size(),64))throw std::runtime_error("save checksum mismatch");
    State state;Reader reader(payload);reader(state);if(!reader.finished())throw std::runtime_error("trailing archive data");
    World result(std::move(state),indexed);result.validate();return result;
}
}
