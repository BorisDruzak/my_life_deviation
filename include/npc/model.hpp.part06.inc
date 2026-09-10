    Json private_data = Object{};
    std::map<Id,Json> projections = {};
    template<class F> void visit(F&& f) {
        f("uid", uid);
        f("sequence", sequence);
        f("time", time);
        f("phase", phase);
        f("type", type);
        f("cause", cause);
        f("root", root);
        f("participants", participants);
        f("objects", objects);
        f("data", data);
        f("private_data", private_data);
        f("projections", projections);
    }
    template<class F> void visit(F&& f) const {
        f("uid", uid);
        f("sequence", sequence);
        f("time", time);
        f("phase", phase);
        f("type", type);
        f("cause", cause);
        f("root", root);
        f("participants", participants);
        f("objects", objects);
        f("data", data);
        f("private_data", private_data);
        f("projections", projections);
    }
};
struct Appraisal {
    Id observer = {};
    Id target = {};
    Id root = {};
    std::string type = {};
    double trust_delta = 0;
    double tension_delta = 0;
    double stress_delta = 0;
    double anger_delta = 0;
    double esteem_delta = 0;
    template<class F> void visit(F&& f) {
        f("observer", observer);
        f("target", target);
        f("root", root);
        f("type", type);
        f("trust_delta", trust_delta);
        f("tension_delta", tension_delta);
        f("stress_delta", stress_delta);
        f("anger_delta", anger_delta);
        f("esteem_delta", esteem_delta);
    }
    template<class F> void visit(F&& f) const {
        f("observer", observer);
        f("target", target);
        f("root", root);
        f("type", type);
        f("trust_delta", trust_delta);
        f("tension_delta", tension_delta);
        f("stress_delta", stress_delta);
        f("anger_delta", anger_delta);
        f("esteem_delta", esteem_delta);
    }
};
struct External {
    Id id = {};
    Minute at = 0;
    std::int64_t priority = 0;
    Id issuer = "scenario";
    std::string kind = {};
    Json args = Object{};
    bool applied = false;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("at", at);
        f("priority", priority);
        f("issuer", issuer);
        f("kind", kind);
        f("args", args);
        f("applied", applied);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("at", at);
        f("priority", priority);
        f("issuer", issuer);
        f("kind", kind);
        f("args", args);
        f("applied", applied);
    }
};
struct State {
    std::string version = rules_version;
    std::string config_hash = {};
    std::string profile = "baseline";
    std::string seed = "world-seed-1";
    Minute time = 0;
    std::map<Id,Place> places = {};
    std::map<Id,Edge> edges = {};
    std::map<Id,Actor> actors = {};
    std::map<Id,Item> items = {};
    std::map<Id,Money> accounts = {};
    std::map<Id,Shop> shops = {};
    std::map<Id,Permission> permissions = {};
    std::map<Id,Contract> contracts = {};
    std::map<Id,Action> actions = {};
    std::map<Id,Job> jobs = {};
    std::map<Id,Proposal> proposals = {};
    std::map<Id,Obligation> obligations = {};
    std::map<Id,Record> records = {};
    std::map<Id,Message> messages = {};
    std::map<Id,Reservation> reservations = {};
    std::map<Id,Receipt> receipts = {};
    std::vector<Command> pending = {};
    std::vector<External> external = {};
    std::vector<Event> ledger = {};
    std::map<Id,std::int64_t> event_counters = {};
    std::set<Id> delivered_evidence = {};
    std::map<Id,Appraisal> appraisal_history = {};
    std::map<Id,Minute> positive_cooldowns = {};
    Money genesis_money = 0;
    Money external_money = 0;
    std::int64_t next_event_sequence = 0;
    template<class F> void visit(F&& f) {
        f("version", version);
        f("config_hash", config_hash);
        f("profile", profile);
        f("seed", seed);
        f("time", time);
        f("places", places);
