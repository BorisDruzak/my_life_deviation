                "minutes", background_wait.empty() ? 15 : 1
            }
        }, d.goal);
        return d;
    }
    Json describe(const Decision & d) {
        Array candidates;
        for (const auto & c : d.candidates) candidates.push_back(Object {
            {"method", c.method}, {"score", c.score}, {"expected_minutes", c.expected_minutes}, {"assumptions", encode(c.assumptions)}, {"first", c.first ? encode(* c.first) : Json(nullptr)}
        });
        return Object {
            {"actor", d.actor}, {"time", d.time}, {"goal", d.goal}, {"command", d.command ? encode(* d.command) : Json(nullptr)}, {"candidates", candidates}
        };
    }
}
