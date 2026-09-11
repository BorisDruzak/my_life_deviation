#include "test.hpp"
#include "npc/math.hpp"
#include "npc/json.hpp"
using namespace npc;
TEST(M01_food_complete) {
    double x = 30;
    for (int i = 0; i < 15; ++ i) x = math::satiety(x, 3.2, 35, 1, 15);
    NEAR(x, 64.2, 1e-9);
}
TEST(M02_food_seven) {
    double x = 30;
    for (int i = 0; i < 7; ++ i) x = math::satiety(x, 3.2, 35, 1, 15);
    NEAR(x, 45.96, 1e-9);
}
TEST(M03_repeat_nutrition) {
    NEAR(math::satiety(30, 3.2, 35, 1, 15), 32.28, 1e-9);
    CHECK(math::pleasure(.4, 1, .5, .5, 5) < math::pleasure(.4, 1, .5, .5, 0));
}
TEST(M04_fatigue) {
    NEAR(math::fatigue(.2, .02, .04, 0, 0), .19966666666666666, 1e-12);
}
TEST(M05_decay) {
    NEAR(math::half_decay(.8, 120, 120), .4, 1e-12);
    NEAR(math::stale(.95, .5, 1440, 1440), .725, 1e-12);
}
TEST(M06_group_attention) {
    NEAR(math::social(.5, 0, 1, .5, 0), .4, 1e-12);
    NEAR(math::social(.5, 0, .5, .5, 0), .2, 1e-12);
}
TEST(M07_drive) {
    NEAR(math::pressure(.5, 0, false), 0, 1e-12);
    NEAR(math::pressure(.8, .5, true), .2006944444444445, 1e-12);
}
TEST(M08_ema) {
    double b = 100;
    for (int t = 0; t < 10080; ++ t) b = math::ema(b, 200, 10080);
    NEAR(b, 150, 1e-8);
}
TEST(M09_hash_vectors) {
    CHECK(math::sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(math::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
TEST(M10_keyed_sample) {
    auto a = math::keyed_sample("s", "ev", "a", "v");
    CHECK(a >= 0 && a < 1);
    CHECK(a == math::keyed_sample("s", "ev", "a", "v"));
    CHECK(a != math::keyed_sample("s", "ev2", "a", "v"));
}
TEST(M11_json_canonical) {
    CHECK(canonical(parse_json("{\"z\":2,\"a\":1}")) == "{\"a\":1,\"z\":2}");
    THROWS(decode < int >(parse_json("1.5")));
    THROWS(decode < bool >(parse_json("1")));
}
TEST(M12_clip_invalid) {
    THROWS(math::clip(std::numeric_limits < double >::quiet_NaN()));
    THROWS(parse_json("{bad}"));
}
