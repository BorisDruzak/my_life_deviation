#include "mld/world.hpp"
namespace mld {
    Choice choose(const View& view) {
        if (!view.knowledge) throw std::invalid_argument("missing subjective model");
        if (view.place == none || view.cognition[4] == 0) return {};
        const auto& knowledge = *view.knowledge;
        int budget = operations(view.cognition), used = 0;
        const int slots = context_slots(view.cognition);
        std::array<double, motives> importance{};
        double norm = 0;
        for (std::size_t j = 0; j < motives; ++j) {
            importance[j] = clamp(.02 + .5 * view.sensed[j]);
            norm += importance[j];
        }
        for (auto& weight : importance) weight /= std::max(1., norm);
        struct Candidate { Act type; Id target; int motive; double priority; bool continuation; };
        std::vector<Candidate> candidates;
        candidates.reserve(16);
        auto add = [&](Act action, Id target, int motive, bool continuation = false) {
            if (knowledge.ways[std::size_t(action)] && (view.sensed[motive] > .025 || continuation))
            candidates.push_back({action, target, motive, view.sensed[motive], continuation});
        };
        // A familiar intention survives intermediate navigation nodes. It is not a new plan.
        if (view.intent.active)
        add(view.intent.procedure, view.intent.destination, view.intent.motive, true);
        if (view.food > 0) add(Act::Eat, view.place, 0);
        else for (const auto& place : knowledge.places) {
            if (place.place != 1 && place.place != view.home) continue;
            if (place.food == Truth::True) {
                if (place.free_food || view.money >= 1) add(Act::Exchange, place.place, 0);
                if (!place.free_food) add(Act::Take, place.place, 0);
            } else if (view.now >= place.retry) add(Act::Inspect, place.place, 0);
        }
        add(Act::Drink, 2, 1);
        add(Act::Sleep, view.home, 2);
        add(Act::Rest, view.home, 3);
        add(Act::Play, view.place, 4);
        bool has_contact = false;
        for (Id person : view.seen_people) {
            auto it = std::lower_bound(knowledge.people.begin(), knowledge.people.end(), person,
            [](const auto& relation, Id id) { return relation.person < id; });
            if (it != knowledge.people.end() && it->person == person && it->retry <= view.now) {
                add(Act::Talk, person, 5);
                has_contact = true;
            }
        }
        if (!has_contact) add(view.place == 3 ? Act::Wait : Act::Move, 3, 5);
        add(Act::Relief, view.home, 6);
        add(Act::Work, 4, 7);
        std::stable_sort(candidates.begin(), candidates.end(), [](auto a, auto b) {
            if (a.continuation != b.continuation) return a.continuation;
            return a.priority != b.priority ? a.priority > b.priority
            : std::pair{a.type, a.target} < std::pair{b.type, b.target};
        });
        candidates.erase(std::unique(candidates.begin(), candidates.end(), [](auto a, auto b) {
            return a.type == b.type && a.target == b.target;
        }), candidates.end());
        if (candidates.size() > 8) candidates.resize(8);
        // One focus plus familiar, already-bound procedures, one memory chunk each.
        const int limit = std::min({1 + int(std::floor(5 * view.cognition[5])),
            int(candidates.size()), std::max(0, slots - 1)});
        Choice best;
        best.score = 0;
        bool incumbent = false;
        int considered = 0;
        const bool emergency = std::max({view.sensed[0], view.sensed[1],
            view.sensed[2], view.sensed[3]}) >= .85;
        for (const auto& candidate : candidates) {
            if (considered >= limit || budget < 2) break;
            budget -= 2;
            used += 2;
            ++considered;
            const Act source = candidate.type == Act::Exchange || candidate.type == Act::Take
            ? Act::Eat : candidate.type;
            const auto& prediction = knowledge.expectation[std::size_t(source)];
            double score = 0;
            for (std::size_t j = 0; j < motives; ++j) {
                double expected = prediction[j].mean;
                if (candidate.type == Act::Inspect && j == 0)
                expected = knowledge.expectation[std::size_t(Act::Eat)][0].mean * .45;
                if (candidate.motive == 5 && j == 5 &&
                (candidate.type == Act::Wait || candidate.type == Act::Move))
                expected = .5 * knowledge.expectation[std::size_t(Act::Talk)][5].mean;
                score += importance[j] * std::min(view.sensed[j] * 1.8, std::max(0., expected));
            }
            double moral = 0, price = 0;
            if (candidate.type == Act::Take) {
                moral = moral_cost(knowledge.property_norm, 1);
                price += knowledge.sanction;
            }
            if (candidate.type == Act::Exchange) {
                auto it = std::find_if(knowledge.places.begin(), knowledge.places.end(),
                [&](auto place) { return place.place == candidate.target; });
                if (it != knowledge.places.end() && !it->free_food) price += .03;
            }
            // Catalogue costs are model parameters; CPU load never changes them.
            const bool long_action = candidate.type == Act::Talk || candidate.type == Act::Work ||
            candidate.type == Act::Play || candidate.type == Act::Relief;
            double time_cost = candidate.type == Act::Sleep ? .04 : (long_action ? .012 : .003);
            const bool local = candidate.type == Act::Eat || candidate.type == Act::Play ||
            candidate.type == Act::Talk || candidate.type == Act::Wait;
            Act execute = candidate.type;
            Id target = candidate.target;
            if (!local && view.place != target) {
                execute = Act::Move;
                target = view.place == 0 ? candidate.target : 0;
                time_cost += .003;
            }
            score -= price + time_cost + moral;
            const double switching_cost = incumbent && !emergency
            ? .10 + .30 * view.cognition[7] + .20 * .5 : 0;
            if (candidate.continuation || score > best.score + switching_cost) {
                best = {execute, target, score, moral, prediction[candidate.motive].mean,
                    0, 0, std::uint8_t(candidate.motive), candidate.type, candidate.target};
                incumbent = candidate.continuation;
            }
        }
        best.used_operations = used;
        best.context_used = 1 + considered;
        return best;
    }

    bool accept_contact(const View& view) {
        if (view.current == Act::Sleep || view.current == Act::Move ||
        (view.current == Act::Talk && view.in_contact) ||
        view.current == Act::Relief || view.current == Act::Work) return false;
        if (std::max({view.sensed[0], view.sensed[1], view.sensed[2]}) >= .85) return false;
        return view.sensed[5] > .15;
    }
} // namespace mld
