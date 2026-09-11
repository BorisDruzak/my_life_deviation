#pragma once
#include "npc/model.hpp"
namespace npc {
    class Config {
        public : Json parameters;
        Json catalog;
        std::string hash;
        std::map < Id, Json > types;
        std::map < Id, Json > recipes;
        std::map < Id, Json > actions;
        static Config load(const std::filesystem::path & directory);
        static Config from_json(Json parameters, Json catalog);
        double number(std::initializer_list < std::string_view > path) const;
        bool enabled(std::string_view profile, std::string_view flag) const;
        const Json & item_type(const Id & id) const;
        void validate() const;
    };
    Actor make_actor(const Id & id, const Id & place, const Config & config);
    Item make_item(const Id & id, const Id & type, const Id & owner, const Placement & placement, const Config & config);
    void validate_state(const Config & config, const State & state, bool restored = false);
    State scenario_state(const Config & config, const Json & scenario);
}
