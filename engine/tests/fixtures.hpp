#pragma once
#include "npc/world.hpp"
#include "npc/math.hpp"
#include "test.hpp"
namespace testdata {
    using namespace npc;
    inline Config config(bool perfect = true) {
        auto c = Config::load(std::filesystem::path(NPC_SOURCE_DIR) / "game/rules");
        c.parameters.as_object()["perception"].as_object()["perfect_observation"] = perfect;
        return Config::from_json(c.parameters, c.catalog);
    }
    inline State state(const Config & c) {
        State s;
        s.config_hash = c.hash;
        for (auto id : {
            "shop", "home", "park", "work"
        }) {
            Place p;
            p.id = id;
            p.name = id;
            s.places[id] = p;
        }
        s.places["home"].private_space = true;
        for (auto[id, from, to, d] : std::vector < std::tuple < std::string, std::string, std::string, int >> {
            {
                "shop_home", "shop", "home", 10
            }, {
                "home_shop", "home", "shop", 10
            }, {
                "shop_park", "shop", "park", 5
            }, {
                "park_shop", "park", "shop", 5
            }, {
                "shop_work", "shop", "work", 7
            }, {
                "work_shop", "work", "shop", 7
            }
        }) {
            Edge e;
            e.id = id;
            e.from = from;
            e.to = to;
            e.minutes = d;
            s.edges[id] = e;
        }
        for (auto id : {
            "a", "b", "c"
        }) {
            s.actors[id] = make_actor(id, "shop", c);
            s.actors[id].known_methods.insert("R02");
            s.actors[id].known_methods.insert("R03");
        }
        s.accounts = {
            {
                "a", 80
            }, {
                "b", 50
            }, {
                "c", 40
            }, {
                "store", 100
            }, {
                "employer", 1000
            }
        };
        for (auto &[id, a] : s.actors) {
            a.wellbeing = static_cast < double >(s.accounts.at(a.account));
        }
        auto add =[&](const std::string & id, const std::string & type, const std::string & owner, const std::string & kind,
        const std::string & ref) {
            s.items[id] = make_item(id, type, owner, {
                kind, ref
            }, c);
        };
        add("meal", "I01", "store", "at", "shop");
        add("tool", "I05", "a", "inventory", "a");
        add("kit", "I06", "a", "inventory", "a");
        add("ingredients", "I04", "a", "inventory", "a");
        add("rope", "I07", "a", "inventory", "a");
        add("book", "I08", "a", "inventory", "a");
        add("ball", "I09", "a", "inventory", "a");
        add("phone_a", "I10", "a", "inventory", "a");
        add("phone_b", "I10", "b", "inventory", "b");
        add("box", "I12", "a", "at", "shop");
        s.items["box"].open = false;
        add("bed", "F01", "a", "at", "shop");
        add("kitchen", "F02", "a", "at", "shop");
        add("target", "F03", "a", "at", "shop");
        s.items["target"].condition = .2;
        add("workplace", "F04", "employer", "at", "work");
        add("cargo", "F05", "a", "at", "shop");
        Shop sh;
        sh.id = "store";
        sh.place = "shop";
        sh.account = "store";
        sh.stock["meal"] = 30;
        s.shops[sh.id] = sh;
        Contract ct;
        ct.id = "employment";
        ct.worker = "a";
        ct.employer_account = "employer";
        ct.workplace = "workplace";
        ct.windows = {
            {
                0, 1440
            }
        };
        ct.next_payout = 61;
        ct.payout_period = 1440;
        ct.rate_per_hour = 37;
        s.contracts[ct.id] = ct;
        Permission pm;
        pm.resource = "workplace";
        pm.authority = "employer";
        pm.grantees.insert("a");
        s.permissions[pm.resource] = pm;
        for (auto &[id, p] : s.actors) {
            for (const auto &[edge, e] : s.edges) {
                Evidence k;
                k.id = "genesis-edge-" + id + "-" + edge;
                k.source = "genesis";
                k.roots = {
                    k.id
                };
                k.statement = {
                    edge, "edge", Object {
                        {
                            "from", e.from
                        }, {
                            "to", e.to
                        }, {
                            "minutes", e.minutes
                        }
                    }, true, 0, - 1
                };
                p.beliefs.push_back(k);
            }
        }
        s.genesis_money = 0;
        for (const auto &[id, b] : s.accounts) s.genesis_money += b;
        return s;
    }
    inline Command cmd(const std::string & actor, std::int64_t seq, const std::string & type, Json args = Object {}) {
        Command c;
        c.actor = actor;
        c.sequence = seq;
        c.type = type;
        c.args = std::move(args);
        return c;
    }
    inline std::size_t events(const World & w, const std::string & type) {
        std::size_t n = 0;
        for (auto & e : w.debug_state().ledger) if (e.type == type) ++ n;
        return n;
    }
    inline void do_action(World & w, const Command & c, Minute ticks = 1) {
        w.submit(c);
        w.run(ticks);
        w.check_invariants();
    }
    inline const Receipt & receipt(const World & w, const Command & c) {
        return w.debug_state().receipts.at(command_key(c));
    }
    inline Json item_args(std::string id) {
        return Object {
            {
                "item", std::move(id)
            }
        };
    }
}
