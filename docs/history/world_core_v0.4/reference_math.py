"""Arithmetic reference for WORLD-0.4; NOT a world engine or planner.

Pure functions only. Time integration, transactional actions, knowledge isolation,
resource arbitration and real replay are specified elsewhere and are not implemented.
"""
from __future__ import annotations
import hashlib
import json
import math
from typing import Iterable


def clip(x: float, lo: float = 0.0, hi: float = 1.0) -> float:
    if not math.isfinite(x) or lo > hi:
        raise ValueError('Invalid numeric state')
    return min(hi, max(lo, x))


def need_step(n: float, drain_hour: float, gain: float = 0.0) -> float:
    if not 0 <= n <= 100 or drain_hour < 0:
        raise ValueError('Invalid need input')
    return clip(n - drain_hour / 60 + gain, 0, 100)


def low_latch(value: float, active: bool, critical: float, on: float, target: float) -> bool:
    if not 0 < critical < on < target <= 100:
        raise ValueError('Invalid thresholds')
    if value <= on:
        return True
    if value >= target:
        return False
    return active


def high_latch(value: float, active: bool, off: float, on: float) -> bool:
    if not 0 <= off < on <= 1:
        raise ValueError('Invalid thresholds')
    return True if value >= on else False if value <= off else active


def eat_segment(satiety: float, remaining: int, ticks: int,
                total_units: int = 15, full_gain: float = 35.0) -> tuple[float, int]:
    if not 0 <= remaining <= total_units or total_units <= 0 or ticks < 0:
        raise ValueError('Invalid portion')
    for _ in range(ticks):
        used = min(1, remaining)
        satiety = need_step(satiety, 3.2, full_gain * used / total_units)
        remaining -= used
    return satiety, remaining


def fatigue_step(f: float, food_deficit: float, sleep_deficit: float,
                 load_hour: float, recovery_hour: float) -> float:
    return clip(f + (load_hour * (1 + .5 * food_deficit + .5 * sleep_deficit) - recovery_hour) / 60)


def health_step(h: float, f: float, food_deficit: float, rest_ok: bool,
                injury: float = 0.0) -> float:
    return clip(h + .01 * (1-h) * rest_ok / 60
                - .005 * food_deficit**2 / 60
                - .002 * max(0, (f-.9)/.1)**2 / 60 - injury)


def repetition_step(r: float, exposure: float) -> float:
    if r < 0 or not 0 <= exposure <= 1:
        raise ValueError('Invalid exposure')
    return r * math.exp(-1/720) + exposure/30


def pleasure(rate: float, exposure: float, interest: float, novelty: float, r: float) -> float:
    return rate * exposure * (.5 + interest) / (1 + novelty * r)


def leisure_segment(r: float, ticks: int, interest: float = .8,
                    novelty: float = .7, rate: float = 6/15) -> tuple[float, float]:
    gain = 0.0
    for _ in range(ticks):
        gain += pleasure(rate, 1, interest, novelty, r)
        r = repetition_step(r, 1)
    return gain, r


def drive_step(d: float, q: float, trigger: float = 1.0, success: bool = False) -> float:
    if q == 0:
        return 0.0
    return clip(d + q * trigger / 720 - .6 * success)


def decay(x: float, minutes: int, half_life: float) -> float:
    if minutes < 0 or half_life <= 0:
        raise ValueError('Invalid time')
    return x * 2**(-minutes/half_life)


def stress_step(s: float, relief: float = 0, impulses: Iterable[float] = ()) -> float:
    return clip(clip(decay(s, 1, 120) - relief) + sum(impulses))


def ema_step(previous: float, target: float, half_life: float = 10080) -> float:
    return previous + (1-math.exp(-math.log(2)/half_life))*(target-previous)


def affection_step(a: float, c: float, w: float = 1, quality: float = 1, r: float = 0) -> float:
    k = .04*w*c*quality/(60*(1+r))
    return 1 - (1-a)*math.exp(-k)


def social_step(a: list[float], tension: list[float], weights: list[float],
                quality: float = 1, novelty: float = 0, r: float = 0) -> float:
    if len(a) != len(tension) or len(a) != len(weights):
        raise ValueError('Incompatible vectors')
    if not weights:
        return 0.0
    if min(weights) < 0 or sum(weights) > 1+1e-12:
        raise ValueError('Invalid attention allocation')
    # Non-participating fraction does not yield a full social base rate.
    participation = sum(weights)
    if participation <= 0:
        return 0.0
    abar = sum(x*w for x,w in zip(a,weights))/participation
    xbar = sum(x*w for x,w in zip(tension,weights))/participation
    return participation*(18+12*abar)*(1-.5*xbar)*quality/(60*(1+novelty*r))


def stale_confidence(p0: float, prior: float, age: int, half_life: float) -> float:
    return prior + (p0-prior)*2**(-age/half_life)


def notice_probability(gate: bool, visibility: float, attention: float,
                       channel: float, perfect: bool = False) -> float:
    if not gate or channel <= 0:
        return 0.0
    return 1.0 if perfect else clip(visibility*attention*channel)


def keyed_sample(seed: str, event_uid: str, observer: str, channel: str) -> float:
    b = json.dumps([seed,event_uid,observer,channel],ensure_ascii=False,separators=(',',':')).encode('utf-8')
    z = int.from_bytes(hashlib.sha256(b).digest(),'big') >> (256-53)
    return z/2**53


def overlap(a: tuple[int,int], b: tuple[int,int]) -> bool:
    if a[0] >= a[1] or b[0] >= b[1]:
        raise ValueError('Empty/negative interval')
    return max(a[0],b[0]) < min(a[1],b[1])


def transfer_arithmetic(buyer: int, seller: int, price: int) -> tuple[int,int]:
    if min(buyer,seller,price)<0 or buyer<price:
        raise ValueError('Insufficient or invalid money')
    return buyer-price, seller+price
