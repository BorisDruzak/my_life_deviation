"""NORM-MEMORY-0.1 equation reference, NOT a C++ patch or a simulation.

Models only selected pure equations and known-source replacement. It does not
implement perception, uncertain provenance grouping, cognition, or SelfModel.
All numerical defaults are proposed game-profile choices.
"""
from __future__ import annotations
from dataclasses import dataclass
import math
from typing import Iterable


def finite(x: float) -> float:
    if not isinstance(x,(int,float)) or not math.isfinite(x):
        raise ValueError('expected finite number')
    return float(x)


def unit(x: float) -> float:
    x=finite(x)
    if not 0<=x<=1:
        raise ValueError('value outside [0,1]')
    return x


def nonnegative(x: float) -> float:
    x=finite(x)
    if x<0:
        raise ValueError('negative value')
    return x


@dataclass(frozen=True)
class BinaryEstimate:
    positive: float=0
    negative: float=0
    prior_positive: float=1
    prior_negative: float=1
    explicit_prior: bool=False

    def __post_init__(self):
        nonnegative(self.positive); nonnegative(self.negative)
        if nonnegative(self.prior_positive)==0 or nonnegative(self.prior_negative)==0:
            raise ValueError('priors must be positive')

    @property
    def known(self) -> bool:
        return self.explicit_prior or self.positive+self.negative>0

    @property
    def p(self) -> float:
        return (self.prior_positive+self.positive)/(self.prior_positive+self.prior_negative+self.positive+self.negative)

    @property
    def coverage(self) -> float:
        n=self.positive+self.negative
        return n/(n+4)


def approval(counts: tuple[float,float,float], priors=(1.,1.,1.)) -> tuple[float,...]:
    if len(counts)!=3 or len(priors)!=3:
        raise ValueError('three outcomes required')
    for p in priors:
        if nonnegative(p)==0: raise ValueError('priors must be positive')
    totals=[nonnegative(c)+p for c,p in zip(counts,priors)]
    return tuple(x/sum(totals) for x in totals)


def evidence_weight(perception: float, interpretation: float, reliability: float, dose: float) -> float:
    return math.prod(unit(x) for x in (perception,interpretation,reliability,dose))


def decay(weight: float, days: float, half_life_days: float=30) -> float:
    weight=nonnegative(weight); days=nonnegative(days); half=nonnegative(half_life_days)
    if half==0: raise ValueError('positive half life required')
    return weight*math.exp2(-days/half)


def personal_update(old: float, target: float, plasticity: float, quality: float, dose: float) -> float:
    old,target,plasticity,quality,dose=map(unit,(old,target,plasticity,quality,dose))
    alpha=-math.expm1(-.02*plasticity*quality*dose)
    return old+alpha*(target-old)


def sanction_cost(seen: float|None, classified: float|None, reacted: float|None, severity: float|None) -> float|None:
    values=(seen,classified,reacted,severity)
    for x in values:
        if x is not None: unit(x)
    return None if any(x is None for x in values) else math.prod(values)


def audience_value(group: float, seen: float, approved: float, disapproved: float, approval_value: float, disapproval_value: float) -> float:
    group,seen,approved,disapproved=map(unit,(group,seen,approved,disapproved))
    if approved+disapproved>1+1e-12: raise ValueError('inconsistent probabilities')
    return group*seen*(approved*nonnegative(approval_value)-disapproved*nonnegative(disapproval_value))


def group_significance(goal_links: Iterable[tuple[float,float]]) -> float:
    return max((unit(i)*unit(r) for i,r in goal_links),default=0.)


def norm_tension(group: float, applicability: float, confidence: float, loss: float) -> float:
    return math.prod(unit(x) for x in (group,applicability,confidence,loss))


def hysteresis(value: float, active: bool, on: float=.12, off: float=.07) -> bool:
    value,on,off=map(unit,(value,on,off))
    if off>=on: raise ValueError('off must be below on')
    return value>off if active else value>=on


def buffer_cost(money: float, reserve: float, price: float, importance: float) -> float:
    money,reserve,price=map(nonnegative,(money,reserve,price)); importance=unit(importance)
    if price>money: raise ValueError('unaffordable immediate purchase')
    return importance*(max(0,reserve-(money-price))-max(0,reserve-money))/max(1,reserve)


def pv(gain: float, probability: float, hours: float, discount: float) -> float:
    gain=finite(gain); probability=unit(probability); hours=nonnegative(hours); discount=nonnegative(discount)
    return gain*probability*math.exp(-discount*hours)


def squash(raw: float) -> float:
    raw=finite(raw)
    return raw/(1+abs(raw))


def combined_acceptance(specific_p: float, specific_confidence: float, norm_or_prior_p: float,
                        norm_confidence: float, self_p: float, self_confidence: float) -> float:
    p,c,n,nc,s,sc=map(unit,(specific_p,specific_confidence,norm_or_prior_p,norm_confidence,self_p,self_confidence))
    context=c*p+(1-c)*n
    w=.25*(1-c)*(1-nc)*sc
    return (1-w)*context+w*s


class EvidenceBook:
    """Only the special case of explicitly known common roots, not rumor inference."""
    def __init__(self):
        self._entries: dict[int,tuple[int,float,float,float]]={}
        self._deliveries: set[int]=set()

    def apply(self, known_root: int, revision: int, y: float, weight: float, at_days: float, delivery: int) -> str:
        if min(known_root,revision,delivery)<=0: raise ValueError('positive IDs required')
        y,weight=map(unit,(y,weight)); at_days=nonnegative(at_days)
        new=(revision,y,weight,at_days)
        if delivery in self._deliveries: return 'duplicate_delivery'
        old=self._entries.get(known_root)
        if old and revision<old[0]:
            self._deliveries.add(delivery); return 'stale'
        if old and revision==old[0]:
            if old!=new: raise ValueError('different content at same revision')
            self._deliveries.add(delivery); return 'duplicate_source'
        self._entries[known_root]=new; self._deliveries.add(delivery)
        return 'replaced' if old else 'added'

    def estimate(self, now_days: float) -> BinaryEstimate:
        now_days=nonnegative(now_days)
        pos=neg=0.
        for root in sorted(self._entries):
            _,y,w,at=self._entries[root]
            if now_days<at: raise ValueError('read before evidence time')
            effective=decay(w,now_days-at)
            pos+=effective*y; neg+=effective*(1-y)
        return BinaryEstimate(pos,neg)


@dataclass(frozen=True)
class Term:
    key: str
    amount: float
    probability: float=1
    hours: float=0
    present_value: bool=False

    def __post_init__(self):
        if not self.key: raise ValueError('key required')
        finite(self.amount); unit(self.probability); nonnegative(self.hours)
        if self.present_value and (self.probability!=1 or self.hours!=0):
            raise ValueError('present-valued term cannot be discounted or weighted again')


class Ledger:
    def __init__(self): self._terms: dict[str,Term]={}
    def insert(self, term: Term) -> None:
        if term.key in self._terms: raise ValueError('duplicate consequence key')
        self._terms[term.key]=term
    def value(self, discount: float) -> float:
        discount=nonnegative(discount)
        return math.fsum(t.amount if t.present_value else pv(t.amount,t.probability,t.hours,discount)
                         for _,t in sorted(self._terms.items()))
    def score(self, discount: float) -> float:
        return squash(self.value(discount))
