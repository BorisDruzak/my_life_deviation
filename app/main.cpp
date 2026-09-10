#include "npc/controller.hpp"
#include "npc/math.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
using namespace npc;
namespace {
    const char * usage = R"(NPC World — автономное ядро WORLD-0.5.0

  world_sim validate --scenario examples/household.json [--data data]
  world_sim run --scenario examples/household.json --minutes 1440 --out out/day
  world_sim resume --snapshot out/day/state.json --scenario examples/household.json
                   --minutes 1440 --out out/next
  world_sim view --snapshot out/day/state.json --actor n0 [--data data]

Параметры run/resume:
  --check-every N       Полная проверка инвариантов каждые N минут (по умолчанию 60).
  --checkpoint-every N  Промежуточный checkpoint.json, 0 — выключено.
  --controllers on|off  Включить контроллеры из файла сценария (on).
  --overwrite          Разрешить заменить файлы в существующем каталоге результата.

Журнал events.jsonl — всеведущий отладочный журнал, НЕ данные для игрока.
Контроллеры используют только World::view(actor). Графики, сети и Unreal нет.
)";
    Minute integer(const std::string & s, const char * name, Minute maximum = 10000000) {
        Minute x = 0;
        auto[p, e] = std::from_chars(s.data(), s.data() + s.size(), x);
        if (e != std::errc {} || p != s.data() + s.size() || x < 0 || x > maximum) throw InputError(std::string("invalid ") + name);
        return x;
    }
    void line(std::ofstream & out, const Json & j) {
        out << canonical(j) << '\n';
        if (! out) throw InputError("log write failed");
    }
}
int main(int argc, char * * argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "help") {
            std::cout << usage;
            return 0;
        }
        const std::string operation = argv[1];
        if (operation == "version") {
            std::cout << rules_version << '\n';
            return 0;
        }
        if (operation != "validate" && operation != "run" && operation != "resume" && operation != "view") throw InputError("unknown operation; use --help");
        std::map < std::string, std::string > options;
        bool overwrite = false;
        const std::set < std::string > allowed = {
            "--data", "--scenario", "--snapshot", "--actor", "--minutes", "--out", "--check-every", "--checkpoint-every", "--controllers"
        };
        for (int i = 2; i < argc; ++ i) {
            std::string key = argv[i];
            if (key == "--overwrite") {
                overwrite = true;
                continue;
            }
            if (! allowed.contains(key) || i + 1 >= argc) throw InputError("unknown/incomplete option: " + key);
            if (options.contains(key)) throw InputError("duplicate option: " + key);
            options[key] = argv[++ i];
        }
        auto required =[&](std::string key) {
            auto it = options.find(key);
            if (it == options.end()) throw InputError("required option: " + key);
            return it->second;
        };
        auto setting =[&](std::string key, std::string fallback) {
            auto it = options.find(key);
            return it == options.end() ? fallback : it->second;
        };
        auto config = Config::load(setting("--data", "data"));
        Json scenario = Object {};
        if (options.contains("--scenario")) scenario = read_json(options.at("--scenario"));
        World world =(operation == "resume" || operation == "view") ? World::restore(config, read_json(required("--snapshot"))) : World::from_scenario(config,
        at(scenario, "world"));
        world.check_invariants();
        if (operation == "validate") {
            std::cout << "VALID " << rules_version << " actors=" << world.debug_state().actors.size() << " items=" << world.debug_state().items.size() << '\n';
            return 0;
        }
        if (operation == "view") {
            std::cout << canonical(world.view(required("--actor"))) << '\n';
            return 0;
        }
        const Minute length = integer(required("--minutes"), "minutes");
        const Minute every = integer(setting("--check-every", "60"), "check-every");
        const Minute checkpoint = integer(setting("--checkpoint-every", "0"), "checkpoint-every");
        const auto controllers_setting = setting("--controllers", "on");
        if (controllers_setting != "on" && controllers_setting != "off") throw InputError("controllers must be on or off");
        const bool controllers_enabled = controllers_setting == "on";
        Json controllers = has(scenario, "controllers") ? at(scenario, "controllers") : Json(Object {});
        if (! controllers.is_object()) throw InputError("controllers must be an object");
        for (const auto & kv : controllers.as_object()) if (! world.debug_state().actors.contains(std::string(kv.key()))) throw InputError("controller references unknown actor");
        std::map < Minute, std::vector < Command >> script;
        if (has(scenario, "script")) for (const auto & v : at(scenario, "script").as_array()) {
            auto time = get < Minute >(v, "at");
            if (time < 0) throw InputError("negative script time");
            script[time].push_back(decode < Command >(at(v, "command")));
        }
        auto path = std::filesystem::path(required("--out"));
        if (! overwrite && std::filesystem::exists(path) && ! std::filesystem::is_empty(path)) throw InputError("output directory is not empty; select another directory or use --overwrite");
        std::filesystem::create_directories(path);
        std::ofstream events(path / "events.jsonl", std::ios::binary), decisions(path / "decisions.jsonl", std::ios::binary),
        samples(path / "samples.csv", std::ios::binary);
        if (! events || ! decisions || ! samples) throw InputError("cannot create output logs");
        samples << "minute,actor,satiety,rest,social,leisure,fatigue,health,stress,money,place\n";
        write_json(path / "initial_state.json", world.snapshot());
        const auto begin = world.debug_state().time;
        auto event_cursor = world.debug_state().ledger.size();
        const auto event_begin = event_cursor;
        const auto started = std::chrono::steady_clock::now();
        std::size_t checkpoints = 0;
        std::map < std::string, std::int64_t > event_counts;
        auto sample =[&]() {
            for (const auto &[id, a] : world.debug_state().actors) samples << world.debug_state().time << ',' << id << ',' << a.needs.at("N01").value << ',' << a.needs.at("N02").value << ',' << a.needs.at("N03").value << ',' << a.needs.at("N04").value << ',' << a.fatigue << ',' << a.health << ',' << a.stress << ',' << world.debug_state().accounts.at(a.account) << ',' << a.position.place << '\n';
        };
        sample();
        for (Minute k = 0; k < length; ++ k) {
            const auto now = world.debug_state().time;
            std::set < Id > scripted;
            // A snapshot may already contain commands queued at this boundary.
            for (const auto & q : world.debug_state().pending) if (q.type != "A22") scripted.insert(q.actor);
            if (auto it = script.find(now); it != script.end()) for (const auto & q : it->second) {
                world.submit(q);
                if (q.type != "A22") scripted.insert(q.actor);
            }
            if (controllers_enabled) for (const auto & kv : controllers.as_object()) {
                const auto id = std::string(kv.key());
                if (scripted.contains(id) || ! world.can_decide(id)) continue;
                auto d = decide(config, world.view(id), kv.value());
                if (d.command) {
                    line(decisions, describe(d));
                    world.submit(* d.command);
                } else if (! d.candidates.empty()) line(decisions, describe(d));
            }
            world.advance();
            while (event_cursor < world.debug_state().ledger.size()) {
                const auto & e = world.debug_state().ledger[event_cursor ++];
                ++ event_counts[e.type];
                line(events, encode(e));
            }
            if (every > 0 && world.debug_state().time % every == 0) world.check_invariants();
            if (checkpoint > 0 && world.debug_state().time % checkpoint == 0) {
                write_json(path / "checkpoint.json", world.snapshot());
                ++ checkpoints;
            }
            if (world.debug_state().time % 60 == 0) sample();
        }
        world.check_invariants();
        sample();
        events.flush();
        decisions.flush();
        samples.flush();
        if (! events || ! decisions || ! samples) throw InputError("final log flush failed");
        const auto elapsed = std::chrono::duration < double >(std::chrono::steady_clock::now() - started).count();
        auto snapshot = world.snapshot();
        write_json(path / "state.json", snapshot);
        std::filesystem::create_directories(path / "views");
        for (auto &[id, a] : world.debug_state().actors) write_json(path / "views" /(id + ".json"), world.view(id));
        std::size_t living = 0;
        for (auto &[id, a] : world.debug_state().actors) if (a.alive) ++ living;
        Json summary = Object {
            {
                "version", std::string(rules_version)
            }, {
                "config_hash", config.hash
            }, {
                "begin", begin
            }, {
                "end", world.debug_state().time
            }, {
                "actors", world.debug_state().actors.size()
            }, {
                "living", living
            }, {
                "items", world.debug_state().items.size()
            }, {
                "events_generated", event_cursor - event_begin
            }, {
                "event_counts", encode(event_counts)
            }, {
                "wall_seconds", elapsed
            }, {
                "checkpoints", checkpoints
            }, {
                "state_hash", str(snapshot, "state_hash")
            }, {
                "controller_kind", "bounded_local_policy_not_universal_HTN"
            }, {
                "invariants", "passed"
            }
        };
        write_json(path / "summary.json", summary);
        std::cout << canonical(summary) << '\n';
        return 0;
    } catch (const std::exception & e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
