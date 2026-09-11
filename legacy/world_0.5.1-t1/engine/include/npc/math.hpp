#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace npc::math {
    double clip(double x, double low = 0, double high = 1);
    double half_decay(double value, double minutes, double half_life);
    double satiety(double n, double drain_per_hour, double nutrition, double units, double total);
    double repetition(double old, double exposure);
    double pleasure(double rate, double exposure, double interest, double novelty, double repeat);
    double fatigue(double old, double load, double recovery, double food_deficit, double sleep_deficit);
    double health(double old, double fatigue, double food_deficit, bool rest);
    double pressure(double old, double intensity, bool relieved);
    double skill(double old, double fit);
    double interest(double old, double exposure, double repeat, double unpleasant);
    double social(double affection, double tension, double participation, double novelty, double repeat, double quality = 1);
    double affection(double old, double attention, double compatibility, double repeat, double quality = 1);
    double ema(double old, double target, double half_life);
    double stale(double initial, double prior, double age, double half_life);
    double keyed_sample(std::string_view seed, std::string_view event, std::string_view observer, std::string_view channel);
    std::string sha256(std::string_view data);
}
