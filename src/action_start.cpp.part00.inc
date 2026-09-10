#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        void need(bool ok, const char * why) {
            if (! ok) throw InputError(why);
        }
        Minute duration(const Json & j, Minute def, Minute maximum) {
            auto d = get_or < Minute >(j, "minutes", def);
            need(d >= 1 && d <= maximum, "duration out of range");
            return d;
        }
        bool portable(const Json & t) {
            return has(t, "mass_kg") && ! has(t, "slots");
        }
        void unique_ids(const std::vector < Id > & v) {
            std::set < Id > s(v.begin(), v.end());
            need(s.size() == v.size(), "duplicate object binding");
        }
        const std::map < std::string, std::set < std::string >> fields = {
            {
                "A01", {
                    "minutes"
                }
            }, {
                "A02", {
                    "edge"
                }
            }, {
                "A03", {
                    "item", "place", "scope"
                }
            }, {
                "A04", {
                    "item"
                }
            }, {
                "A05", {
                    "item", "container"
                }
            }, {
                "A06", {
                    "item", "shop"
                }
            }, {
                "A07", {
                    "item"
                }
            }, {
                "A08", {
                    "job", "recipe", "fixture", "materials"
                }
            }, {
                "A09", {
                    "bed", "minutes"
                }
            }, {
                "A10", {
                    "contract", "minutes"
                }
            }, {
                "A11", {
                    "to", "terms", "expires", "channel"
                }
            }, {
                "A12", {
                    "proposal", "version", "answer", "terms", "expires", "channel"
                }
            }, {
                "A13", {
                    "mode", "to", "item", "proposal", "obligation", "amount"
                }
            }, {
                "A14", {
                    "participants", "terms", "expires", "channel"
                }
            }, {
                "A15", {
                    "proposal", "version"
                }
            }, {
                "A16", {
                    "item", "minutes"
                }
            }, {
                "A17", {
                    "to", "operator", "evidence", "channel"
                }
            }, {
                "A18", {
                    "to", "question", "mode", "statement", "evidence", "channel", "fabricated"
                }
            }, {
                "A19", {
                    "to", "statement", "evidence", "channel", "fabricated"
                }
            }, {
                "A20", {
                    "item"
                }
            }, {
                "A21", {
                    "job", "recipe", "target", "tools", "materials"
                }
            }, {
                "A22", {
                    "proposal"
                }
            }, {
                "A23", {
                    "minutes"
                }
            }, {
                "A24", {
                    "message", "record", "version"
                }
            }, {
                "A25", {
                    "resource", "version", "grantee", "grant"
                }
            }, {
                "A26", {
                    "item", "open"
                }
            }, {
                "A27", {
                    "to", "obligation", "episode", "forceful", "channel"
                }
            }, {
                "A28", {
                    "statement", "evidence", "acl", "public", "fabricated"
                }
            }, {
                "A29", {
