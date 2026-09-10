        f("resource", resource);
        f("action", action);
        f("amount", amount);
        f("capacity", capacity);
        f("until", until);
    }
};
struct Job {
    Id id = {};
    Id recipe = {};
    Id creator = {};
    Id place = {};
    Id target = {};
    Id fixture = {};
    std::vector<Id> tools = {};
    std::vector<Id> materials = {};
    std::vector<Id> outputs = {};
    Minute worked = 0;
    Minute required = 1;
    std::string status = "paused";
    Id active_action = {};
    double recorded_loss_kg = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("recipe", recipe);
        f("creator", creator);
        f("place", place);
        f("target", target);
        f("fixture", fixture);
        f("tools", tools);
        f("materials", materials);
        f("outputs", outputs);
        f("worked", worked);
        f("required", required);
        f("status", status);
        f("active_action", active_action);
        f("recorded_loss_kg", recorded_loss_kg);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("recipe", recipe);
        f("creator", creator);
        f("place", place);
        f("target", target);
        f("fixture", fixture);
        f("tools", tools);
        f("materials", materials);
        f("outputs", outputs);
        f("worked", worked);
        f("required", required);
        f("status", status);
        f("active_action", active_action);
        f("recorded_loss_kg", recorded_loss_kg);
    }
};
struct Proposal {
    Id id = {};
    std::int64_t version = 1;
    Id author = {};
    std::set<Id> participants = {};
    std::set<Id> signatures = {};
    Json terms = Object{};
    Minute created = 0;
    Minute expires = 0;
    std::string state = "offered";
    Id root = {};
    bool executed = false;
    std::vector<Json> prior_versions = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("version", version);
        f("author", author);
        f("participants", participants);
        f("signatures", signatures);
        f("terms", terms);
        f("created", created);
        f("expires", expires);
        f("state", state);
        f("root", root);
        f("executed", executed);
        f("prior_versions", prior_versions);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("version", version);
        f("author", author);
        f("participants", participants);
        f("signatures", signatures);
        f("terms", terms);
        f("created", created);
        f("expires", expires);
        f("state", state);
        f("root", root);
        f("executed", executed);
        f("prior_versions", prior_versions);
    }
};
struct Obligation {
    Id id = {};
    Id root = {};
    Id debtor = {};
    Id creditor = {};
    std::string kind = {};
    Id item = {};
    Id target = {};
    Money amount = 0;
    Money remaining = 0;
    Minute due = 0;
    bool important = false;
    std::string status = "pending";
    Minute fulfilled_at = -1;
    Minute breached_at = -1;
    std::set<Id> acknowledged = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("item", item);
        f("target", target);
        f("amount", amount);
        f("remaining", remaining);
        f("due", due);
        f("important", important);
        f("status", status);
        f("fulfilled_at", fulfilled_at);
        f("breached_at", breached_at);
        f("acknowledged", acknowledged);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("item", item);
        f("target", target);
        f("amount", amount);
        f("remaining", remaining);
        f("due", due);
        f("important", important);
