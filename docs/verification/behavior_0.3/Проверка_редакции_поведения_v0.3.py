#!/usr/bin/env python3
"""Local numerical/contract checks for BEHAVIOR-0.3, not a C++ NPC simulation.
Run directly with Python 3.10+; no third-party modules or network required.
A unittest method counts as one test, including its parameterized subcases.
"""
from __future__ import annotations

import hashlib
import json
import math
import random
import unicodedata
import unittest
from collections import defaultdict, deque
from pathlib import Path


def unit(x: float, name: str = "value") -> float:
    if not isinstance(x, (int, float)) or not math.isfinite(x) or not 0 <= x <= 1:
        raise ValueError(f"{name}: expected finite [0,1]")
    return float(x)


def nonnegative(x: float, name: str = "value") -> float:
    if not isinstance(x, (int, float)) or not math.isfinite(x) or x < 0:
        raise ValueError(f"{name}: expected finite nonnegative number")
    return float(x)


def clamp_signed(x: float) -> float:
    return min(1.0, max(-1.0, x))


def deficit(d: float, rate: float, hours: float) -> float:
    d, rate, hours = unit(d), nonnegative(rate), nonnegative(hours)
    if rate > 0.1:
        raise ValueError("rate exceeds profile range")
    if hours == 0 or rate == 0:
        return d
    return 1.0 - (1.0 - d) * math.exp(-rate * hours)


def refractory(r: float, rate: float, hours: float) -> float:
    r, rate, hours = unit(r), nonnegative(rate), nonnegative(hours)
    return r if hours == 0 or rate == 0 else r * math.exp(-rate * hours)


def satisfaction(d: float, r: float, amount: float, key: str,
                 applied: set[str]) -> tuple[float, float]:
    d, r, amount = unit(d), unit(r), unit(amount)
    if not isinstance(key, str) or not key:
        raise ValueError("a nonempty logical result key is required")
    if key in applied:
        return d, r
    applied.add(key)
    return d * (1.0 - amount), r + amount * (1.0 - r)


def context_trigger(primary: float, features: list[tuple[str, str, float, float]]) -> float:
    if not math.isfinite(primary) or not -1 <= primary <= 1:
        raise ValueError("invalid primary trigger")
    distinct: dict[tuple[str, str], tuple[float, float]] = {}
    for feature, source, exposure, weight in features:
        exposure = unit(exposure)
        if not math.isfinite(weight) or not -1 <= weight <= 1:
            raise ValueError("invalid W")
        key = feature, source
        if key in distinct:
            old_x, old_w = distinct[key]
            if old_w != weight:
                raise ValueError("conflicting weights for identical feature/source")
            exposure = max(exposure, old_x)
        distinct[key] = exposure, weight
    acquired = clamp_signed(sum(x * w for _, (x, w) in sorted(distinct.items())))
    return 0.5 * clamp_signed(primary + acquired)


def desire_target(base: float, sensitivity: float, d: float, trigger: float,
                  r: float, inhibition: float) -> float:
    base, sensitivity = unit(base), unit(sensitivity)
    if base > 0.4 or sensitivity > 0.8:
        raise ValueError("outside configured trait range")
    d, r, inhibition = unit(d), unit(r), unit(inhibition)
    if not math.isfinite(trigger) or not -0.5 <= trigger <= 0.5:
        raise ValueError("invalid context trigger")
    return min(1.0, max(0.0, (base + sensitivity * d + trigger) * (1-r) - inhibition))


def approach(value: float, target: float, seconds: float, tau: float) -> float:
    value, target = unit(value), unit(target)
    seconds, tau = nonnegative(seconds), nonnegative(tau)
    if tau == 0:
        raise ValueError("tau must be positive")
    if seconds == 0:
        return value
    return target + (value - target) * math.exp(-seconds / tau)


def dependency(rows: list[tuple[float, float | None, float | None]]) -> tuple[float | None, float, str]:
    total = assessed = result = 0.0
    for importance, contribution, replacement in rows:
        importance = unit(importance)
        total += importance
        if contribution is not None:
            contribution = unit(contribution)
        if replacement is not None:
            replacement = unit(replacement)
        if contribution is not None and replacement is not None:
            assessed += importance
            result += importance * contribution * (1 - replacement)
    if total == 0:
        return 0.0, 0.0, "нет_целей"
    if assessed == 0:
        return None, 0.0, "неизвестно"
    return result / assessed, assessed / total, "полно" if assessed == total else "частично"


def moral_resistance(groups: dict[str, list[float]]) -> float:
    return min(1.0, sum(max((unit(x) for x in values), default=0.0)
                        for values in groups.values()))


def expected_sanction(discovery: float, qualification: float,
                      reaction: float, severity: float) -> float:
    return math.prod(unit(x) for x in (discovery, qualification, reaction, severity))


def revise_norm(w: float, target: float, plasticity: float, quality: float, dose: float) -> float:
    w, target, plasticity, quality, dose = map(unit, (w, target, plasticity, quality, dose))
    alpha = -math.expm1(-0.02 * plasticity * quality * dose)
    return w + alpha * (target - w)


def seed_word(seed: int, version: str, catalog_hash: str, stage: str,
              entity: int, index: int, retry: int = 0) -> int:
    if not isinstance(seed, int) or not 0 <= seed < 2**64 or entity < 0 or index < 0 or retry < 0:
        raise ValueError("invalid seed or key indices")
    if len(catalog_hash) != 64 or any(c not in "0123456789abcdef" for c in catalog_hash):
        raise ValueError("catalog SHA-256 must be lowercase hex")
    values = [f"{seed:016x}", version, catalog_hash, stage, str(entity), str(index), str(retry)]
    values = [unicodedata.normalize("NFC", x) for x in values]
    payload = json.dumps(values, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")


def seed_integer(seed: int, stage: str, entity: int, upper: int, selection: int = 0) -> int:
    if not isinstance(upper, int) or not 1 <= upper <= 2**64:
        raise ValueError("invalid integer range")
    limit = (2**64 // upper) * upper
    i = 0
    while True:
        value = seed_word(seed, "0.1.0", "a" * 64, stage, entity, selection, i)
        if value < limit:
            return value % upper
        i += 1


def check_social_graph(n: int, edges: list[tuple[int, int]]) -> None:
    """Checks only acquaintance degree/connectivity, not full BIO generation."""
    if n < 8:
        raise ValueError("normal profile requires N>=8")
    if len(set(edges)) != len(edges):
        raise ValueError("duplicate edges")
    outgoing: dict[int, set[int]] = defaultdict(set)
    undirected: dict[int, set[int]] = defaultdict(set)
    for a, b in edges:
        if a == b or not 0 <= a < n or not 0 <= b < n:
            raise ValueError("invalid edge")
        outgoing[a].add(b)
        undirected[a].add(b)
        undirected[b].add(a)
    for a in range(n):
        if not 3 <= len(outgoing[a]) <= min(32, n-2):
            raise ValueError("invalid acquaintance degree")
    if n >= 64 and len(edges) / (n * (n-1)) > 0.25:
        raise ValueError("too dense")
    reached, queue = {0}, deque([0])
    while queue:
        for b in undirected[queue.popleft()]:
            if b not in reached:
                reached.add(b)
                queue.append(b)
    if len(reached) != n:
        raise ValueError("disconnected graph")


def check_parents(birth_years: dict[int, int], pairs: list[tuple[int, int]]) -> None:
    """Years suffice for fixtures; full generator must validate exact dates."""
    if len(set(pairs)) != len(pairs):
        raise ValueError("duplicate parental relation")
    parents: dict[int, set[int]] = defaultdict(set)
    for parent, child in pairs:
        if parent not in birth_years or child not in birth_years or parent == child:
            raise ValueError("invalid parent")
        gap = birth_years[child] - birth_years[parent]
        if not 18 <= gap <= 50:
            raise ValueError("invalid generational interval")
        parents[child].add(parent)
        if len(parents[child]) > 2:
            raise ValueError("more than two biological parents")
    # Strictly increasing birth dates already preclude a directed cycle.


class FormulaTests(unittest.TestCase):
    def test_01_deficit_accumulates(self):
        self.assertGreater(deficit(0.25, math.log(2)/72, 72), 0.25)
        self.assertAlmostEqual(deficit(0.25, math.log(2)/72, 72), 0.625)

    def test_02_deficit_saturated(self):
        self.assertEqual(deficit(1, 0.01, 1000), 1)

    def test_03_zero_rate(self):
        self.assertEqual(deficit(0.37, 0, 100), 0.37)

    def test_04_zero_interval_exact(self):
        for x in (0.0, 0.1, 0.432198721, 1.0):
            self.assertEqual(deficit(x, 0.01, 0).hex(), x.hex())
            self.assertEqual(approach(x, 0.7, 0, 30).hex(), x.hex())

    def test_05_invalid_inputs(self):
        for value in (float("nan"), float("inf"), -1.0):
            with self.assertRaises(ValueError):
                deficit(0.4, 0.01, value)
        with self.assertRaises(ValueError):
            approach(0.1, 0.2, 0, 0)
        with self.assertRaises(ValueError):
            deficit(float("nan"), 0, 0)

    def test_06_deficit_composition(self):
        self.assertAlmostEqual(deficit(deficit(0.2, 0.02, 4), 0.02, 7), deficit(0.2, 0.02, 11), delta=1e-12)

    def test_07_refractory_composition(self):
        self.assertAlmostEqual(refractory(refractory(0.8, 0.1, 3), 0.1, 7), refractory(0.8, 0.1, 10), delta=1e-12)

    def test_08_satisfaction_values(self):
        d, r = satisfaction(0.8, 0.2, 0.6, "one", set())
        self.assertAlmostEqual(d, 0.32)
        self.assertAlmostEqual(r, 0.68)

    def test_09_satisfaction_idempotency(self):
        keys: set[str] = set()
        first = satisfaction(0.8, 0.2, 0.6, "one", keys)
        self.assertEqual(first, satisfaction(*first, 0.6, "one", keys))

    def test_10_satisfaction_zero(self):
        self.assertEqual(satisfaction(0.5, 0.2, 0, "one", set()), (0.5, 0.2))

    def test_11_context_increases_desire(self):
        self.assertGreater(desire_target(0.1, 0.45, 0.1, 0.4, 0, 0), desire_target(0.1, 0.45, 0.1, 0, 0, 0))

    def test_12_unobserved_trigger_absent(self):
        self.assertEqual(context_trigger(0, []), 0)

    def test_13_duplicate_exposure_max(self):
        self.assertEqual(context_trigger(0, [("f", "s", 0.8, 0.4), ("f", "s", 0.4, 0.4)]),
                         context_trigger(0, [("f", "s", 0.8, 0.4)]))

    def test_14_feature_order(self):
        rows = [("a", "s", 0.4, 0.3), ("b", "s", 0.6, -0.2)]
        self.assertEqual(context_trigger(0.1, rows), context_trigger(0.1, rows[::-1]))

    def test_15_primary_training_not_W(self):
        primary = 0.0
        self.assertGreater(context_trigger(primary, [("f", "s", 1, 0.8)]), 0)
        # The prescribed primary-only target weakens, rather than reinforces,
        # a learned response with no independent primary stimulus.
        old_w = 0.8
        eta = 0.02 * 0.5
        updated = old_w + eta * (primary - old_w)
        incorrect = old_w + eta * (1.0 - old_w)
        self.assertLess(updated, old_w)
        self.assertGreater(incorrect, old_w)

    def test_16_refractory_inhibits(self):
        self.assertLess(desire_target(0.1, 0.45, 0.8, 0.2, 0.7, 0), desire_target(0.1, 0.45, 0.8, 0.2, 0, 0))

    def test_17_constant_target_composition(self):
        self.assertAlmostEqual(approach(approach(0.1, 0.8, 10, 30), 0.8, 20, 30), approach(0.1, 0.8, 30, 30), delta=1e-12)

    def test_18_random_bounds(self):
        rng = random.Random(1903)
        for _ in range(2000):
            d, r, s = (rng.random() for _ in range(3))
            for value in (deficit(d, rng.random()/10, rng.random()*10000),
                          desire_target(rng.random()*0.4, rng.random()*0.8, d, rng.random()-0.5, r, rng.random()),
                          *satisfaction(d, r, s, "x", set())):
                self.assertTrue(math.isfinite(value) and 0 <= value <= 1)

    def test_19_dependency_example(self):
        value, coverage, status = dependency([(1, 0.8, 0.25), (1, 0.4, 0.75)])
        self.assertAlmostEqual(value, 0.35)
        self.assertEqual((coverage, status), (1, "полно"))

    def test_20_unknown_replacement(self):
        self.assertEqual(dependency([(1, 1, None)]), (None, 0, "неизвестно"))

    def test_21_dependency_coverage(self):
        value, coverage, status = dependency([(1, 0.8, 0.25), (1, 1, None)])
        self.assertAlmostEqual(value, 0.6)
        self.assertEqual((coverage, status), (0.5, "частично"))

    def test_22_zero_goals(self):
        self.assertEqual(dependency([]), (0, 0, "нет_целей"))

    def test_23_alternative_reduces_dependency(self):
        self.assertLess(dependency([(1, 1, 0.9)])[0], dependency([(1, 1, 0.1)])[0])

    def test_24_taboo_finite(self):
        self.assertEqual(moral_resistance({"taboo": [1]}), 1)
        self.assertTrue(math.isfinite(moral_resistance({"taboo": [1]})))

    def test_25_moral_duplicates(self):
        self.assertAlmostEqual(moral_resistance({"a": [0.504, 0.45], "b": [0.18]}), 0.684)

    def test_26_sanction_example(self):
        self.assertAlmostEqual(expected_sanction(0.4, 0.8, 0.5, 0.6), 0.096)

    def test_27_unknown_sanction_not_zero(self):
        with self.assertRaises(ValueError):
            expected_sanction(None, 0.8, 0.5, 0.6)

    def test_28_norm_slow_update(self):
        self.assertAlmostEqual(revise_norm(0.8, 0.2, 0.5, 1, 1), 0.7940299002495008)

    def test_29_norm_no_basis(self):
        self.assertEqual(revise_norm(0.8, 0, 0.5, 0, 1), 0.8)

    def test_30_seed_reproducible(self):
        args = (42, "0.1.0", "a" * 64, "родство", 17, 0)
        self.assertEqual(seed_word(*args), seed_word(*args))
        self.assertNotEqual(seed_word(*args), seed_word(42, "0.1.0", "a" * 64, "знакомства", 17, 0))
        # A rejection retry and the next logical draw have distinct keys.
        self.assertNotEqual(seed_word(42, "0.1.0", "a"*64, "родство", 17, 0, 1),
                            seed_word(42, "0.1.0", "a"*64, "родство", 17, 1, 0))

    def test_31_seed_order_independent(self):
        forward = {i: seed_integer(42, "контакты", i, 97) for i in range(30)}
        reverse = {i: seed_integer(42, "контакты", i, 97) for i in reversed(range(30))}
        self.assertEqual(forward, reverse)

    def test_32_seed_unicode_normalization(self):
        self.assertEqual(seed_word(1, "v", "a"*64, "й", 0, 0), seed_word(1, "v", "a"*64, "и\u0306", 0, 0))

    def test_33_seed_range(self):
        for upper in (1, 7, 128, 2**63+1):
            self.assertTrue(0 <= seed_integer(19, "range", 5, upper) < upper)

    def test_34_social_valid_fixture(self):
        check_social_graph(8, [(a, (a+offset)%8) for a in range(8) for offset in (1,2,6,7)])

    def test_35_social_rejects_isolate_or_clique(self):
        with self.assertRaises(ValueError):
            check_social_graph(8, [])
        with self.assertRaises(ValueError):
            check_social_graph(8, [(a,b) for a in range(8) for b in range(8) if a != b])

    def test_36_social_rejects_disconnected(self):
        with self.assertRaises(ValueError):
            check_social_graph(8, [(a,b) for a in range(8) for b in range(8) if a != b and a//4 == b//4])

    def test_37_parent_valid_fixture(self):
        check_parents({0:1950, 1:1952, 2:1978, 3:2000}, [(0,2), (1,2), (2,3)])

    def test_38_parent_rejects_cycle_self_age(self):
        for pairs in ([(0,0)], [(0,1),(1,0)], [(1,2)]):
            with self.assertRaises(ValueError):
                check_parents({0:1950, 1:1980, 2:1985}, pairs)

    def test_39_shared_channel_limits(self):
        profile = json.loads((Path(__file__).parents[1] / "Профили/Параметры_поведения_v0.3.json").read_text(encoding="utf-8"))
        limits = profile["Лимиты"]
        self.assertEqual(limits["Признаки"] * limits["Каналы_W"], limits["Пары_W_на_эпизод"])
        self.assertEqual((limits["Новые_пары_W"], limits["Все_приобретённые_пары"]), (4, 512))

    def test_40_profile_no_institution_execution(self):
        profile = json.loads((Path(__file__).parents[1] / "Профили/Параметры_поведения_v0.3.json").read_text(encoding="utf-8"))
        self.assertEqual(profile["Институты"], {"Режим":"Ожидаемые санкции", "Фактическое_исполнение":False})


if __name__ == "__main__":
    unittest.main(verbosity=2)
