        f("relations", relations);
        f("drives", drives);
        f("beliefs", beliefs);
        f("inbox", inbox);
        f("latches", latches);
        f("health", health);
        f("fatigue", fatigue);
        f("pain", pain);
        f("impairment", impairment);
        f("stress", stress);
        f("fear", fear);
        f("selfesteem", selfesteem);
        f("selfesteem_base", selfesteem_base);
        f("wellbeing", wellbeing);
        f("capacity_kg", capacity_kg);
        f("base_speed", base_speed);
        f("attention", attention);
        f("alive", alive);
        f("capable", capable);
        f("heavy_allowed", heavy_allowed);
        f("active_action", active_action);
        f("horizon", horizon);
        f("search_width", search_width);
        f("outcome_counts", outcome_counts);
    }
};
struct Place {
    Id id = {};
    std::string name = {};
    Id owner = {};
    bool private_space = false;
    bool public_entry = true;
    std::int64_t capacity = 20;
    std::vector<std::vector<Minute>> open_windows = {{0,1440}};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("name", name);
        f("owner", owner);
        f("private_space", private_space);
        f("public_entry", public_entry);
        f("capacity", capacity);
        f("open_windows", open_windows);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("name", name);
        f("owner", owner);
        f("private_space", private_space);
        f("public_entry", public_entry);
        f("capacity", capacity);
        f("open_windows", open_windows);
    }
};
struct Edge {
    Id id = {};
    Id from = {};
    Id to = {};
    Minute minutes = 1;
    bool open = true;
    Id key = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("from", from);
        f("to", to);
        f("minutes", minutes);
        f("open", open);
        f("key", key);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("from", from);
        f("to", to);
        f("minutes", minutes);
        f("open", open);
        f("key", key);
    }
};
struct Placement {
    std::string kind = "at";
    Id ref = {};
    template<class F> void visit(F&& f) {
        f("kind", kind);
        f("ref", ref);
    }
    template<class F> void visit(F&& f) const {
        f("kind", kind);
        f("ref", ref);
    }
};
struct Item {
    Id id = {};
    Id type = {};
    Id owner = {};
    Placement placement = {};
    double condition = 1;
    std::int64_t version = 1;
    std::int64_t total_units = 0;
    std::int64_t remaining_units = 0;
    bool open = true;
    bool locked = false;
    Id key = {};
    bool important = false;
    bool secured = false;
    Id created_by = "genesis";
    template<class F> void visit(F&& f) {
        f("id", id);
        f("type", type);
        f("owner", owner);
        f("placement", placement);
        f("condition", condition);
        f("version", version);
        f("total_units", total_units);
        f("remaining_units", remaining_units);
        f("open", open);
        f("locked", locked);
        f("key", key);
        f("important", important);
        f("secured", secured);
        f("created_by", created_by);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("type", type);
        f("owner", owner);
        f("placement", placement);
        f("condition", condition);
        f("version", version);
        f("total_units", total_units);
        f("remaining_units", remaining_units);
        f("open", open);
        f("locked", locked);
        f("key", key);
        f("important", important);
        f("secured", secured);
        f("created_by", created_by);
    }
};
struct Shop {
    Id id = {};
    Id place = {};
    Id account = {};
    std::int64_t slots = 1;
    std::map<Id,Money> stock = {};
    template<class F> void visit(F&& f) {
        f("id", id);
