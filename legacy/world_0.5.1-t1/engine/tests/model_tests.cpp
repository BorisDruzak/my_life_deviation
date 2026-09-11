#include "test.hpp"
#include "npc/config.hpp"
using namespace npc;
static Config cfg() {
    return Config::load(std::filesystem::path(NPC_SOURCE_DIR) / "game/rules");
}
TEST(V01_load_config) {
    auto c = cfg();
    CHECK(c.hash.size() == 64);
    CHECK(c.actions.size() == 32);
    CHECK(c.types.size() == 17);
}
TEST(V02_make_actor) {
    auto c = cfg();
    auto a = make_actor("a", "home", c);
    CHECK(a.needs.size() == 4);
    NEAR(a.needs.at("N01").awake_drain, 3.2, 1e-12);
    CHECK(a.position.place == "home");
}
TEST(V03_make_item) {
    auto c = cfg();
    auto i = make_item("food", "I01", "a", {
        "inventory", "a"
    }, c);
    CHECK(i.remaining_units == 15);
    THROWS(make_item("x", "unknown", "a", {
        "at", "home"
    }, c));
}
TEST(V04_unknown_field) {
    THROWS(decode < Actor >(parse_json("{\"id\":\"a\",\"bogus\":1}")));
}
TEST(V05_roundtrip_actor) {
    auto a = make_actor("a", "home", cfg());
    auto j = encode(a);
    CHECK(canonical(encode(decode < Actor >(j))) == canonical(j));
}
TEST(V06_bad_threshold) {
    auto c = cfg();
    auto p = c.parameters;
    p.as_object()["needs"].as_object()["N01"].as_object()["activation"] = 5;
    THROWS(Config::from_json(p, c.catalog));
}
