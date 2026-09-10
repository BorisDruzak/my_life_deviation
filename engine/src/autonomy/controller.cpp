#include "npc/controller.hpp"
#include "npc/math.hpp"
#include <algorithm>
#include <queue>
namespace npc {
    namespace {
        struct Route {
            double minutes = 0;
            Id first;
            bool possible = true;
        };
        struct Local {
            const Config & config;
            Actor actor;
            Minute time;
            Money money;
            const Json & view;
            std::int64_t sequence = 1;
            std::map < Id, Json > inventory;
            std::map < Id, Evidence > known_items, stores, proposals;
            std::map < Id, Edge > edges;
            Local(const Config & c, const Json & v) : config(c), actor(decode < Actor >(at(v, "self"))), time(get < Minute >(v,
            "time")), money(get < Money >(v, "money")), view(v) {
                for (const auto & r : at(v, "receipts").as_array()) {
                    auto id = str(r, "id");
                    auto at = id.rfind('@');
                    if (at != std::string::npos) sequence = std::max(sequence, static_cast < std::int64_t >(std::stoll(id.substr(at + 1)) + 1));
                }
                for (const auto & kv : at(v, "inventory").as_object()) inventory[std::string(kv.key())] = kv.value();
                for (const auto & e : actor.beliefs) {
                    if (e.confidence < .5) continue;
                    auto insert_latest =[&](auto & map, const Id & id) {
                        auto it = map.find(id);
                        if (it == map.end() || it->second.learned_at <= e.learned_at) map[id] = e;
                    };
                    const auto & s = e.statement;
                    if (s.predicate == "item_seen") insert_latest(known_items, s.subject);
                    if (s.predicate == "shop_stock") insert_latest(stores, s.subject);
                    if (s.predicate == "edge" && has(s.arguments, "from") && has(s.arguments, "to") && has(s.arguments, "minutes")) {
                        Edge edge;
                        edge.id = s.subject;
                        edge.from = str(s.arguments, "from");
                        edge.to = str(s.arguments, "to");
                        edge.minutes = get < Minute >(s.arguments, "minutes");
                        if (edge.minutes > 0) edges[edge.id] = edge;
                    }
                    if ((s.predicate == "proposal" || s.predicate == "proposal_update") && has(s.arguments, "proposal")) insert_latest(proposals,
                    str(s.arguments, "proposal"));
                }
            }
            Command cmd(const std::string & type, Json args, const std::string & intent) const {
                Command q;
                q.actor = actor.id;
                q.sequence = sequence;
                q.type = type;
                q.args = std::move(args);
                q.intent = intent;
                return q;
            }
            Route route(const Id & target) const {
                if (actor.position.kind != "at") return {
                    0, "", false
                };
                if (target == actor.position.place) return {};
                using Node = std::tuple < double, Id, Id >;
                std::priority_queue < Node, std::vector < Node >, std::greater < Node >> q;
                std::map < Id, double > d;
                d[actor.position.place] = 0;
                q.emplace(0, actor.position.place, "");
                while (! q.empty()) {
                    auto[cost, p, first] = q.top();
                    q.pop();
                    if (cost > d.at(p)) continue;
                    if (p == target) return {
                        cost, first, true
                    };
                    for (const auto &[id, e] : edges) if (e.from == p) {
                        auto n = cost + static_cast < double >(e.minutes);
                        if (! d.contains(e.to) || n < d.at(e.to)) {
                            d[e.to] = n;
                            q.emplace(n, e.to, first.empty() ? id : first);
                        }
                    }
                }
                return {
                    0, "", false
                };
            }
            bool recently_rejected(const std::string & type, const std::string & field, const Id & id, Minute within = 30) const {
                for (const auto & r : at(view, "receipts").as_array()) if (has(r, "request") && text(r, "status") == "rejected" && time - get_or < Minute >(r,
                "received", 0) < within) {
                    const auto & request = at(r, "request");
                    if (text(request, "type") == type && text(at(request, "args"), field) == id) return true;
                }
                return false;
            }
            bool purchased_since(const Id & item, Minute since) const {
                for (const auto & b : actor.beliefs) if (b.statement.predicate == "event" && b.learned_at >= since && text(b.statement.arguments,
                "type") == "purchase" && has(b.statement.arguments, "data") && text(at(b.statement.arguments, "data"), "item") == item) return true;
                return false;
            }
            bool owns_type(const std::string & type) const {
                for (auto &[id, j] : inventory) if (str(j, "type") == type) return true;
                return false;
            }
            Relation relation(const Id & id) const {
                auto it = actor.relations.find(id);
                return it == actor.relations.end() ? Relation {} : it->second;
            }
        };
        void choose(Decision & d, const Local & x) {
            std::stable_sort(d.candidates.begin(), d.candidates.end(),[](const Candidate & a, const Candidate & b) {
                return a.score > b.score;
            });
            if (d.candidates.size() > static_cast < std::size_t >(x.actor.search_width)) d.candidates.resize(static_cast < std::size_t >(x.actor.search_width));
            if (! d.candidates.empty() && d.candidates.front().first) d.command = d.candidates.front().first;
        }
        void acquire(Decision & d, const Local & x, const std::string & type, Minute due) {
            if (x.owns_type(type)) {
                d.goal = "already_available";
                return;
            }
            const double delay_price = .02 + .08 *(1 - x.actor.traits.at("patience"));
            for (const auto &[shop, b] : x.stores) {
                auto place = str(b.statement.arguments, "place");
                auto route = x.route(place);
                if (! route.possible || x.time + route.minutes + 3 > due) continue;
                Minute last_failure = - 1;
                for (const auto & r : at(x.view, "receipts").as_array()) if (has(r, "request") && text(r, "status") == "rejected") {
                    const auto & request = at(r, "request");
                    if (text(request, "type") == "A06" && text(at(request, "args"), "shop") == shop) last_failure = std::max(last_failure,
                    get_or < Minute >(r, "received", 0));
                }
                if (last_failure >= b.learned_at) {
                    Candidate refresh;
                    refresh.method = "refresh_after_failure";
                    refresh.score = 1;
                    refresh.expected_minutes = route.minutes + 2;
                    refresh.assumptions = {
                        "execution contradicted the previous stock belief"
                    };
                    refresh.first = route.first.empty() ? x.cmd("A03", Object {
                        {
                            "place", place
                        }, {
                            "scope", "stock"
                        }
                    }, d.goal) : x.cmd("A02", Object {
                        {
                            "edge", route.first
                        }
                    }, d.goal);
                    d.candidates.push_back(refresh);
                    continue;
                }
                Minute wait = 0;
                if (has(b.statement.arguments, "open_windows")) {
                    const auto windows = decode < std::vector < std::vector < Minute >> >(at(b.statement.arguments, "open_windows"));
                    bool found = false;
                    for (Minute offset = 0; offset <= 1440; ++ offset) {
                        bool fits = true;
                        for (Minute dt = 0; dt < 3; ++ dt) {
                            auto t =(x.time + static_cast < Minute >(route.minutes) + offset + dt) % 1440;
                            bool open = false;
                            for (const auto & w : windows) if (w.size() == 2 && w[0] <= t && t < w[1]) open = true;
                            if (! open) fits = false;
                        }
                        if (fits) {
                            wait = offset;
                            found = true;
                            break;
                        }
                    }
                    if (! found || x.time + route.minutes + wait + 3 > due) continue;
                }
                for (const auto & offer : at(b.statement.arguments, "offers").as_array()) {
                    if (str(offer, "type") != type) continue;
                    auto item = str(offer, "item");
                    auto price = get < Money >(offer, "price");
                    if (price > x.money || x.purchased_since(item, b.learned_at) || x.recently_rejected("A06", "item", item)) continue;
                    Candidate p;
                    p.method = "buy";
                    p.expected_minutes = route.minutes + static_cast < double >(wait) + 3;
                    // Bounded policy score: not presented as the complete utility/HTN model from v0.3.
                    p.score = 30 - 10 * std::log1p(static_cast < double >(price) / std::max(1.0, static_cast < double >(x.money))) - delay_price * p.expected_minutes;
                    p.assumptions = {
                        "stock from " + b.id, "availability is checked only on execution"
                    };
                    p.first = ! route.first.empty() ? x.cmd("A02", Object {
                        {
                            "edge", route.first
                        }
                    }, d.goal) : wait > 0 ? x.cmd("A01", Object {
                        {
                            "minutes", std::min < Minute >(30, wait)
                        }
                    }, d.goal) : x.cmd("A06", Object {
                        {
                            "shop", shop
                        }, {
                            "item", item
                        }
                    }, d.goal);
                    d.candidates.push_back(p);
                }
            }
            for (const auto &[item, b] : x.known_items) {
                if (text(b.statement.arguments, "type") != type) continue;
                auto holder = text(b.statement.arguments, "holder");
                if (holder.empty() || holder == x.actor.id) continue;
                bool addressed = false;
                for (const auto &[pid, e] : x.proposals) {
                    const auto & terms = at(e.statement.arguments, "terms");
                    if (text(terms, "item") == item && text(terms, "borrower") == x.actor.id) addressed = true;
                }
                if (addressed || x.recently_rejected("A11", "to", holder)) continue;
                const auto r = x.relation(holder);
                const auto key = holder + "/loan_item/";
                const auto yes = x.actor.outcome_counts.contains(key + "accepted") ? x.actor.outcome_counts.at(key + "accepted") : 0;
                const auto no = x.actor.outcome_counts.contains(key + "declined") ? x.actor.outcome_counts.at(key + "declined") : 0;
                const double p =(2.0 + static_cast < double >(yes)) /(4.0 + static_cast < double >(yes + no));
                auto route = x.route(text(b.statement.arguments, "place"));
                if (! route.possible) continue;
                std::string channel = x.owns_type("I10") ? "digital" : "speech";
                const auto time =(channel == "digital" ? 3.0 : route.minutes + 2);
                if (x.time + time >= due) continue;
                Candidate c;
                c.method = "borrow";
                c.expected_minutes = time;
                c.score = p * 30 -(1 - p) *(5 + 10 * x.actor.traits.at("caution")) + 3 * r.trust - 3 *(1 - x.actor.traits.at("social_initiative")) - delay_price * time;
                c.assumptions = {
                    "holder from " + b.id, "estimated acceptance=" + std::to_string(p), "other actor decides independently"
                };
                if (channel == "speech" && ! route.first.empty()) c.first = x.cmd("A02", Object {
                    {
                        "edge", route.first
                    }
                }, d.goal);
                else c.first = x.cmd("A11", Object {
                    {
                        "to", holder
                    }, {
                        "channel", channel
                    }, {
                        "expires", x.time + 120
                    }, {
                        "terms", Object {
                            {
                                "kind", "loan_item"
                            }, {
                                "lender", holder
                            }, {
                                "borrower", x.actor.id
                            }, {
                                "item", item
                            }, {
                                "due", due
                            }
                        }
                    }
                }, d.goal);
                d.candidates.push_back(c);
            }
            // No known executable solution: inspect the nearest known shop, but not every minute.
            if (d.candidates.empty()) for (const auto &[shop, b] : x.stores) {
                if (x.time - b.learned_at < 60 || ! at(b.statement.arguments, "offers").as_array().empty()) continue;
                auto route = x.route(str(b.statement.arguments, "place"));
                if (! route.possible) continue;
                Candidate p;
                p.method = "inspect_store";
                p.score = - 2 - route.minutes;
                p.expected_minutes = route.minutes + 2;
                p.assumptions = {
                    "refresh stale stock"
                };
                p.first = route.first.empty() ? x.cmd("A03", Object {
                    {
                        "place", str(b.statement.arguments, "place")
                    }, {
                        "scope", "stock"
                    }
                }, d.goal) : x.cmd("A02", Object {
                    {
                        "edge", route.first
                    }
                }, d.goal);
                d.candidates.push_back(p);
            }
            choose(d, x);
        }
    }
    Decision decide(const Config & config, const Json & view, const Json & objective) {
        Local x(config, view);
        Decision d;
        d.actor = x.actor.id;
        d.time = x.time;
        const auto mode = text(objective, "mode", "household");
        if (mode != "household" && mode != "acquire" && mode != "repair" && mode != "passive") throw InputError("unknown controller mode");
        if (! x.actor.alive || ! x.actor.capable || ! x.actor.active_action.empty() || mode == "passive") {
            d.goal = "unavailable";
            return d;
        }
        if (x.actor.position.kind == "stranded") {
            d.goal = "resume_route";
            d.command = x.cmd("A02", Object {
                {
                    "edge", x.actor.position.edge
                }
            }, d.goal);
            return d;
        }
        // A pending social exchange does not outrank immediately available food or sleep.
        // Keep the availability/passive guards above this check; do not interrupt active actions.
        const auto & food = x.actor.needs.at("N01");
        const auto & sleep = x.actor.needs.at("N02");
        std::string food_item;
        for (auto &[id, i] : x.inventory) if (get_or < bool >(config.item_type(str(i, "type")), "edible", false) && get_or < std::int64_t >(i,
        "remaining_units", 0) > 0) {
            food_item = id;
            break;
        }
        if (food.value <= food.activation && ! food_item.empty()) {
            d.goal = "eat";
            d.command = x.cmd("A07", Object {
                {
                    "item", food_item
                }
            }, d.goal);
            return d;
        }
        if (sleep.value <= sleep.activation) {
            for (const auto &[id, b] : x.known_items) if (text(b.statement.arguments, "type") == "F01") {
                auto route = x.route(str(b.statement.arguments, "place"));
                if (! route.possible) continue;
                d.goal = "sleep";
                Minute duration = std::min < Minute >(720, std::max < Minute >(1, static_cast < Minute >(std::ceil((sleep.target - sleep.value) * 60 / std::max(1.0,
                sleep.sleep_gain)))));
                d.command = route.first.empty() ? x.cmd("A09", Object {
                    {
                        "bed", id
                    }, {
                        "minutes", duration
                    }
                }, d.goal) : x.cmd("A02", Object {
                    {
                        "edge", route.first
                    }
                }, d.goal);
                return d;
            }
        }
        // Reading is separate from delivery. No answer can be made using an unread body.
        for (const auto & m : at(view, "inbox").as_array()) if (! get < bool >(m, "read")) {
            d.goal = "read_message";
            d.command = x.cmd("A24", Object {
                {
                    "message", str(m, "id")
                }
            }, d.goal);
            return d;
        }
        // This is only an idle fallback, not an exclusive action or an inbox barrier.
        std::string background_wait;
        for (const auto &[pid, b] : x.proposals) {
            const auto & p = b.statement.arguments;
            auto state = text(p, "state");
            const auto & terms = at(p, "terms");
            auto kind = str(terms, "kind");
            auto signatures = get < std::set < Id >>(p, "signatures");
            if ((state == "offered" || state == "countered") && ! signatures.contains(x.actor.id) && get < Minute >(p, "expires") > x.time + 1) {
                bool ready = x.actor.needs.at("N01").value > 20 && x.actor.needs.at("N02").value > 15;
                const auto r = x.relation(text(p, "author"));
                double willingness = 8 * r.trust + 4 * r.affection - 3;
                if (kind == "loan_item" && text(terms, "lender") == x.actor.id) {
                    auto item = text(terms, "item");
                    ready = ready && x.inventory.contains(item) && text(x.inventory.at(item), "owner") == x.actor.id;
                } else if (kind == "loan_money" && text(terms, "lender") == x.actor.id) ready = ready && x.money >= get < Money >(terms,
                "amount") + 40;
                else if (kind == "joint" || kind == "private") {
                    auto route = x.route(str(terms, "place"));
                    ready = ready && route.possible && x.time + route.minutes + get < Minute >(terms, "minutes") <= get < Minute >(terms,
                    "end");
                    if (kind == "private") ready = false;
                } else if (kind == "repair") ready = ready && x.actor.known_methods.contains("R02");
                d.goal = "answer_proposal";
                d.command = x.cmd("A12", Object {
                    {
                        "proposal", pid
                    }, {
                        "version", get < std::int64_t >(p, "version")
                    }, {
                        "answer", ready && willingness > 0 ? "accept" : "decline"
                    }, {
                        "channel", x.owns_type("I10") ? "digital" : "speech"
                    }
                }, d.goal);
                return d;
            }
            if ((state == "offered" || state == "countered") && signatures.contains(x.actor.id) && get < Minute >(p, "expires") > x.time) {
                if (background_wait.empty()) background_wait = "await_response";
            }
            if (state == "accepted") {
                bool executed = false;
                for (const auto & r : at(view, "receipts").as_array()) if (has(r, "request")) {
                    const auto & q = at(r, "request");
                    const auto & args = at(q, "args");
                    if (text(args, "proposal") != pid) continue;
                    const auto status = text(r, "status");
                    // Legacy A13 requests do not carry a proposal version.
                    if (text(q, "type") == "A13" && status == "completed") executed = true;
                    // Own participation is terminal only for this exact signed version.
                    // A rejected readiness command never started participation and may be retried.
                    if (text(q, "type") == "A15" && (status == "completed" || status == "interrupted") &&
                        get_or<std::int64_t>(args, "version", 0) == get<std::int64_t>(p, "version")) executed = true;
                }
                if (executed) continue;
                if ((kind == "loan_item" || kind == "loan_money") && text(terms, "lender") == x.actor.id) {
                    auto to = str(terms, "borrower");
                    if (kind == "loan_item") {
                        auto item = str(terms, "item");
                        if (! x.inventory.contains(item) || x.recently_rejected("A13", "proposal", pid, 10)) continue;
                        d.goal = "fulfil_loan";
                        d.command = x.cmd("A13", Object {
                            {
                                "mode", kind
                            }, {
                                "to", to
                            }, {
                                "item", item
                            }, {
                                "proposal", pid
                            }
                        }, d.goal);
                        return d;
                    }
                }
                if (kind == "loan_item" && text(terms, "borrower") == x.actor.id && ! x.inventory.contains(text(terms, "item")) && x.time < get < Minute >(terms,
                "due")) {
                    background_wait = "await_handoff";
                    continue;
                }
                if (kind == "joint" && x.time + get < Minute >(terms, "minutes") <= get < Minute >(terms, "end")) {
                    auto route = x.route(str(terms, "place"));
                    if (! route.possible) continue;
                    d.goal = "attend_agreement";
                    if (! route.first.empty()) d.command = x.cmd("A02", Object {
                        {
                            "edge", route.first
                        }
                    }, d.goal);
                    else if (x.time >= get < Minute >(terms, "start")) d.command = x.cmd("A15", Object {
                        {
                            "proposal", pid
                        }, {
                            "version", get < std::int64_t >(p, "version")
                        }
                    }, d.goal);
                    return d;
                }
            }
        }
        if (mode == "acquire") {
            d.goal = "acquire_item";
            acquire(d, x, str(objective, "item_type"), get_or < Minute >(objective, "due", x.time + 1440));
            return d;
        }
        if (mode == "repair") {
            auto target = str(objective, "target");
            if (has(view, "own_jobs")) for (const auto & j : at(view, "own_jobs").as_array()) if (text(j, "target") == target) {
                if (text(j, "status") == "completed") {
                    d.goal = "task_completed";
                    return d;
                }
                d.goal = "resume_repair";
                d.command = x.cmd("A21", Object {
                    {
                        "job", str(j, "id")
                    }
                }, d.goal);
                return d;
            }
            std::string tool, kit;
            for (auto &[id, i] : x.inventory) {
                if (str(i, "type") == "I05") tool = id;
                if (str(i, "type") == "I06") kit = id;
            }
            if (tool.empty() || kit.empty()) {
                d.goal = "repair_inputs";
                acquire(d, x, tool.empty() ? "I05" : "I06", get_or < Minute >(objective, "due", x.time + 1440));
                return d;
            }
            d.goal = "repair";
            d.command = x.cmd("A21", Object {
                {
                    "recipe", "R02"
                }, {
                    "target", target
                }, {
                    "tools", Array {
                        tool
                    }
                }, {
                    "materials", Array {
                        kit
                    }
                }
            }, d.goal);
            return d;
        }
        const double projected_food = food.value - food.awake_drain * static_cast < double >(x.actor.horizon) / 60;
        if (food_item.empty() && projected_food <= food.activation) {
            d.goal = food.value <= food.activation ? "get_food" : "prepare_food";
            acquire(d, x, "I01", x.time + std::max < Minute >(30, x.actor.horizon));
            if (d.command) return d;
        }
        if (x.actor.fatigue >= .75 || x.actor.stress >= .6) {
            d.goal = "rest";
            d.command = x.cmd("A23", Object {
                {
                    "minutes", 60
                }
            }, d.goal);
            return d;
        }
        if (has(view, "employment")) for (const auto & j : at(view, "employment").as_array()) {
            auto contract = decode < Contract >(j);
            for (const auto & window : contract.windows) if (window[0] <= x.time % 1440 && x.time % 1440 < window[1]) {
                auto it = x.known_items.find(contract.workplace);
                if (it == x.known_items.end()) continue;
                auto route = x.route(str(it->second.statement.arguments, "place"));
                if (! route.possible) continue;
                d.goal = "work";
                d.command = route.first.empty() ? x.cmd("A10", Object {
                    {
                        "contract", contract.id
                    }, {
                        "minutes", std::min < Minute >(60, window[1] - x.time % 1440)
                    }
                }, d.goal) : x.cmd("A02", Object {
                    {
                        "edge", route.first
                    }
                }, d.goal);
                return d;
            }
        }
        if (x.actor.needs.at("N04").active) {
            for (auto &[id, i] : x.inventory) if (has(config.item_type(str(i, "type")), "pleasure_per_30_min")) {
                d.goal = "leisure";
                d.command = x.cmd("A16", Object {
                    {
                        "item", id
                    }, {
                        "minutes", 30
                    }
                }, d.goal);
                return d;
            }
        }
        d.goal = background_wait.empty() ? "idle" : background_wait;
        d.command = x.cmd("A01", Object {
            {
                "minutes", background_wait.empty() ? 15 : 1
            }
        }, d.goal);
        return d;
    }
    Json describe(const Decision & d) {
        Array candidates;
        for (const auto & c : d.candidates) candidates.push_back(Object {
            {
                "method", c.method
            }, {
                "score", c.score
            }, {
                "expected_minutes", c.expected_minutes
            }, {
                "assumptions", encode(c.assumptions)
            }, {
                "first", c.first ? encode(* c.first) : Json(nullptr)
            }
        });
        return Object {
            {
                "actor", d.actor
            }, {
                "time", d.time
            }, {
                "goal", d.goal
            }, {
                "command", d.command ? encode(* d.command) : Json(nullptr)
            }, {
                "candidates", candidates
            }
        };
    }
}
