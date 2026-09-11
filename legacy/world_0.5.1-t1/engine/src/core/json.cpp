#include <boost/json/src.hpp>
#include "npc/json.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace npc {
    namespace {
        // Boost.JSON accepts repeated keys by replacement. Configuration/snapshots must not:
        // two spellings (including escaped Unicode) of one key would make review ambiguous.
        class UniqueKeys {
            std::string_view text;
            std::size_t p = 0;
            void space() {
                while (p < text.size() && std::isspace(static_cast < unsigned char >(text[p]))) ++ p;
            }
            std::string key() {
                std::size_t start = p ++;
                while (p < text.size()) {
                    char c = text[p ++];
                    if (c == '\\') {
                        ++ p;
                        continue;
                    }
                    if (c == '"') break;
                }
                auto j = boost::json::parse(text.substr(start, p - start));
                return std::string(j.as_string().data(), j.as_string().size());
            }
            void value() {
                space();
                if (p >= text.size()) return;
                const char c = text[p];
                if (c == '{') {
                    ++ p;
                    space();
                    std::set < std::string > keys;
                    if (text[p] == '}') {
                        ++ p;
                        return;
                    }
                    while (true) {
                        space();
                        auto k = key();
                        if (! keys.insert(k).second) throw InputError("duplicate JSON key: " + k);
                        space();
                        ++ p;
                        value();
                        space();
                        if (text[p ++] == '}') break;
                    }
                } else if (c == '[') {
                    ++ p;
                    space();
                    if (text[p] == ']') {
                        ++ p;
                        return;
                    }
                    while (true) {
                        value();
                        space();
                        if (text[p ++] == ']') break;
                    }
                } else if (c == '"') {
                    static_cast < void >(key());
                } else while (p < text.size() && text[p] != ',' && text[p] != ']' && text[p] != '}' && ! std::isspace(static_cast < unsigned char >(text[p]))) ++ p;
            }
            public : explicit UniqueKeys(std::string_view s) : text(s) {} void check() {
                value();
            }
        };
    }
    Json parse_json(std::string_view text) {
        boost::json::parse_options opts;
        opts.max_depth = 128;
        opts.numbers = boost::json::number_precision::precise;
        boost::system::error_code ec;
        auto j = boost::json::parse(text, ec, {}, opts);
        if (ec) throw InputError("JSON: " + ec.message());
        UniqueKeys(text).check();
        return j;
    }
    Json read_json(const std::filesystem::path & path) {
        std::ifstream f(path, std::ios::binary);
        if (! f) throw InputError("cannot read: " + path.string());
        f.seekg(0, std::ios::end);
        auto size = f.tellg();
        if (size < 0 || size > std::streamoff(256 * 1024 * 1024)) throw InputError("file exceeds 256 MiB limit");
        f.seekg(0);
        std::string data(static_cast < std::size_t >(size), '\0');
        if (! f.read(data.data(), size)) throw InputError("short read");
        return parse_json(data);
    }
    static Json sorted(const Json & j) {
        if (j.is_object()) {
            std::map < std::string, Json > m;
            for (auto & kv : j.as_object()) m.emplace(std::string(kv.key()), sorted(kv.value()));
            return encode(m);
        }
        if (j.is_array()) {
            Array a;
            for (auto & x : j.as_array()) a.push_back(sorted(x));
            return a;
        }
        return j;
    }
    std::string canonical(const Json & j) {
        return boost::json::serialize(sorted(j));
    }
    void write_json(const std::filesystem::path & path, const Json & j) {
        if (! path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        // A new target is installed by rename; failure keeps the existing target intact.
        auto tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (! f) throw InputError("cannot write: " + tmp.string());
            f << canonical(j) << '\n';
            f.flush();
            if (! f) throw InputError("write failed");
        }
        std::error_code ec;
#ifdef _WIN32
        if (! MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) ec = std::error_code(static_cast < int >(GetLastError()),
        std::system_category());
#else
        std::filesystem::rename(tmp, path, ec);
#endif
        if (ec) {
            std::filesystem::remove(tmp);
            throw InputError("atomic rename failed: " + ec.message());
        }
    }
    const Json & at(const Json & j, std::string_view k) {
        if (! j.is_object()) throw InputError("expected object");
        const auto * p = j.as_object().if_contains(k);
        if (! p) throw InputError("missing field: " + std::string(k));
        return * p;
    }
    bool has(const Json & j, std::string_view k) {
        return j.is_object() && j.as_object().if_contains(k) != nullptr;
    }
}
