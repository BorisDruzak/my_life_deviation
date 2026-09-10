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
                if (actor.position.kind != "at") return {0, "", false};
                if (target == actor.position.place) return {};
                using Node = std::tuple < double, Id, Id >;
                std::priority_queue < Node, std::vector < Node >, std::greater < Node >> q;
                std::map < Id, double > d;
                d[actor.position.place] = 0;
                q.emplace(0, actor.position.place, "");
                while (! q.empty()) {
                    auto[cost, p, first] = q.top(); q.pop();
                    if (cost > d.at(p)) continue;
                    if (p == target) return {cost, first, true};
                    for (const auto &[id, e] : edges) if (e.from == p) {
                        auto n = cost + static_cast < double >(e.minutes);
                        if (! d.contains(e.to) || n < d.at(e.to)) { d[e.to] = n;
