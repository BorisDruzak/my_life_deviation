#include "npc/math.hpp"
#include "npc/json.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
namespace npc::math {
    double clip(double x, double low, double high) {
        if (! std::isfinite(x) || ! std::isfinite(low) || ! std::isfinite(high) || low > high) throw InputError("invalid bounded value");
        return std::clamp(x, low, high);
    }
    double half_decay(double v, double minutes, double h) {
        if (h <= 0 || minutes < 0) throw InputError("invalid decay interval");
        return v * std::exp2(- minutes / h);
    }
    double satiety(double n, double d, double q, double units, double total) {
        if (total <= 0 || units < 0 || units > total || d < 0 || q < 0) throw InputError("invalid portion");
        return clip(n - d / 60 + q * units / total, 0, 100);
    }
    double repetition(double old, double exposure) {
        return old * std::exp(- 1.0 / 720) + exposure / 30;
    }
    double pleasure(double rate, double exposure, double interest, double novelty, double repeat) {
        return rate * exposure *(.5 + interest) /(1 + novelty * repeat);
    }
    double fatigue(double old, double load, double recovery, double food, double sleep) {
        return clip(old +(load *(1 + .5 * food + .5 * sleep) - recovery) / 60);
    }
    double health(double h, double f, double food, bool rest) {
        return clip(h +(.01 *(1 - h) *(rest ? 1 : 0) - .005 * food * food - .002 * std::pow(std::max(0.0,(f - .9) / .1),
        2)) / 60);
    }
    double pressure(double d, double q, bool relieved) {
        return q == 0 ? 0 : clip(d + q / 720 -(relieved ? .6 : 0));
    }
    double skill(double old, double fit) {
        return clip(old + .03 / 60 * fit *(1 - old));
    }
    double interest(double old, double exposure, double repeat, double unpleasant) {
        const double v = clip(.8 + .2 /(1 + repeat) - .5 * unpleasant);
        return clip(old + .02 * exposure / 60 *(v - old));
    }
    double social(double a, double x, double participation, double novelty, double repeat, double quality) {
        return participation *(18 + 12 * a) *(1 - .5 * x) * quality /(60 *(1 + novelty * repeat));
    }
    double affection(double a, double attention, double c, double repeat, double quality) {
        return 1 -(1 - a) * std::exp(- .04 * attention * c * quality /(60 *(1 + repeat)));
    }
    double ema(double old, double target, double h) {
        if (h <= 0) throw InputError("invalid EMA period");
        return old +(1 - std::exp2(- 1 / h)) *(target - old);
    }
    double stale(double p, double prior, double age, double h) {
        return prior +(p - prior) * std::exp2(- age / h);
    }
    std::string sha256(std::string_view input) {
        static constexpr std::array < std::uint32_t, 64 > k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };
        std::array < std::uint32_t, 8 > h = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
        std::vector < std::uint8_t > bytes(input.begin(), input.end());
        const auto bits = static_cast < std::uint64_t >(bytes.size()) * 8;
        bytes.push_back(0x80);
        while (bytes.size() % 64 != 56) bytes.push_back(0);
        for (int i = 7; i >= 0; -- i) bytes.push_back(static_cast < std::uint8_t >(bits >>(i * 8)));
        for (std::size_t off = 0; off < bytes.size(); off += 64) {
            std::array < std::uint32_t, 64 > w {};
            for (int i = 0; i < 16; ++ i) w[i] =(std::uint32_t(bytes[off + 4 * i]) << 24) |(std::uint32_t(bytes[off + 4 * i + 1]) << 16) |(std::uint32_t(bytes[off + 4 * i + 2]) << 8) | bytes[off + 4 * i + 3];
            for (int i = 16; i < 64; ++ i) {
                auto s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^(w[i - 15] >> 3);
                auto s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^(w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            auto[a, b, c, d, e, f, g, hh] = h;
            for (int i = 0; i < 64; ++ i) {
                auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
                auto ch =(e & f) ^(~e & g);
                auto t1 = hh + s1 + ch + k[i] + w[i];
                auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
                auto maj =(a & b) ^(a & c) ^(b & c);
                auto t2 = s0 + maj;
                hh = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }
        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (auto x : h) out << std::setw(8) << x;
        return out.str();
    }
    double keyed_sample(std::string_view seed, std::string_view event, std::string_view observer, std::string_view channel) {
        Array parts {
            Json(seed), Json(event), Json(observer), Json(channel)
        };
        const auto digest = sha256(boost::json::serialize(parts));
        auto first = std::stoull(digest.substr(0, 14), nullptr, 16) >> 3;
        return static_cast < double >(first) / 9007199254740992.0;
    }
}
