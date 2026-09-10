                auto & j = state_.jobs.at(a.job);
                j.active_action = a.id;
                j.status = "active";
            }
        }
        if (c.type == "A15" || c.type == "A32") state_.proposals.at(a.proposal).executed = true;
        return true;
    }
}
