        for (auto &[id, a] : s.actors) {
            if (! s.accounts.contains(a.account)) s.accounts[a.account] = 0;
            if (! has(at(at(raw, "actors"), id), "wellbeing")) a.wellbeing = static_cast < double >(s.accounts.at(a.account));
            for (auto &[n, x] : a.needs) {
                x.active = x.value <= x.activation;
                x.critical_active = x.value <= x.critical;
            }
        }
        s.genesis_money = 0;
        for (const auto &[id, b] : s.accounts) {
            if (b < 0 || b > max_money || s.genesis_money > max_money - b) throw InputError("money exceeds supported range");
            s.genesis_money += b;
        }
        return s;
    }
}
