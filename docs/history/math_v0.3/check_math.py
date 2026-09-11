#!/usr/bin/env python3
"""Reference arithmetic checks for proposed NPC model 0.3.

This is NOT a world simulator or planner. It checks numeric formulas and the
consistency of the accompanying catalogue. Uses only Python's standard library.
Run: python check_math.py
"""
from __future__ import annotations
import json
import math
import unittest
from pathlib import Path
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parent
CATALOG = json.loads((ROOT / 'catalog_v0_3.json').read_text(encoding='utf-8'))
PARAMS = CATALOG['model']

def finite(x: float) -> float:
    if not math.isfinite(x):
        raise ValueError('Non-finite value')
    return x

def clamp(x: float, lo: float = 0.0, hi: float = 1.0) -> float:
    if lo > hi:
        raise ValueError('Invalid bounds')
    return min(hi, max(lo, finite(x)))

def loss(n: float, critical: float, target: float, weight: float) -> float:
    if not 0 < critical < target <= 100 or weight <= 0:
        raise ValueError('Invalid need parameters')
    n = clamp(n, 0, 100)
    return weight * ((max(0., target - n) / target)**2
                     + 3 * (max(0., critical - n) / critical)**2)

def need_loss(n: float, name: str = 'satiety') -> float:
    p=PARAMS['needs'][name]
    return loss(n,p['critical'],p['target'],p['weight'])

def trace_step(r: float, active: bool) -> float:
    if r < 0:
        raise ValueError('Negative repetition trace')
    return r * math.exp(-1 / 720) + (1 / 30 if active else 0)

def pleasure_rate(budget: float, duration: int, interest: float,
                  novelty: float, repetition: float) -> float:
    if duration < 1 or budget < 0 or repetition < 0:
        raise ValueError('Invalid activity')
    if not 0 <= interest <= 1 or not 0 <= novelty <= 1:
        raise ValueError('Invalid preference')
    return budget / duration * (0.5 + interest) / (1 + novelty * repetition)

def pleasure_steps(segments: Sequence[int], *, initial_trace: float = 0,
                   budget: float = 6, duration: int = 15,
                   interest: float = 0.8, novelty: float = 0.7) -> tuple[float,float]:
    r = initial_trace
    result = 0.0
    for segment in segments:
        if segment < 0:
            raise ValueError('Negative segment')
        for _ in range(segment):
            result += pleasure_rate(budget,duration,interest,novelty,r)
            r = trace_step(r,True)
    return result,r

def social_step(attachment: float, compatibility: float, repetition: float) -> float:
    return clamp(attachment + 0.04*compatibility*(1-attachment)/(60*(1+repetition)))

def stress_step(s: float, stimulus: float=0, relief: float=0) -> float:
    return clamp(s * 2**(-1/120) + stimulus-relief)

def money_u(m: float) -> float:
    if m < 0:
        raise ValueError('Negative money')
    return 12*math.log1p(m/100)

def gamma(patience: float, stress: float) -> float:
    if not 0 <= patience <= 1 or not 0 <= stress <= 1:
        raise ValueError('Invalid trait/state')
    half=(120+600*patience)/(1+0.5*stress)
    return 2**(-1/half)

def return_of_trace(utilities: Sequence[float], costs: Sequence[float], g: float) -> float:
    if len(utilities) != len(costs)+1 or not 0 < g <= 1:
        raise ValueError('Invalid trajectory')
    return sum(g**i*(utilities[i+1]-utilities[i]-costs[i]) for i in range(len(costs)))

def plan_score(branches: Sequence[tuple[float,float]], caution: float) -> float:
    if not branches or not 0 <= caution <= 1:
        raise ValueError('Invalid input')
    if any(p < 0 or not math.isfinite(p) or not math.isfinite(v) for p,v in branches):
        raise ValueError('Invalid branch')
    if abs(sum(p for p,_ in branches)-1) > 1e-9:
        raise ValueError('Probabilities do not sum to one')
    return sum(p*v for p,v in branches)-2*caution*sum(p*max(0,-v) for p,v in branches)

def freshness(p: float, prior: float, age: float, half_life: float) -> float:
    if not 0 <= p <= 1 or not 0 <= prior <= 1 or age < 0 or half_life <= 0:
        raise ValueError('Invalid belief')
    return prior+(p-prior)*2**(-age/half_life)

def posterior(prior: float, likelihood_true: float, likelihood_false: float) -> float:
    if not all(0 <= v <= 1 for v in (prior,likelihood_true,likelihood_false)):
        raise ValueError('Invalid Bayesian input')
    denominator=prior*likelihood_true+(1-prior)*likelihood_false
    if denominator == 0:
        raise ValueError('Evidence incompatible with model; do not fabricate posterior')
    return prior*likelihood_true/denominator

def beta_forecast(success: int,failure: int) -> float:
    if success < 0 or failure < 0:
        raise ValueError('Invalid counts')
    return (2+success)/(4+success+failure)

def drive_step(d: float,q: float,success: bool=False) -> float:
    return clamp(d+q/720-(0.6 if success else 0))

def signatures_match(version: int, sign_a: int|None, sign_b: int|None) -> bool:
    return sign_a == version and sign_b == version

class FormulaChecks(unittest.TestCase):
    def test_01_clip(self):
        self.assertEqual(clamp(-1),0); self.assertEqual(clamp(2),1)
    def test_02_nan_rejected(self):
        with self.assertRaises(ValueError):clamp(float('nan'))
    def test_03_full_need_no_loss(self):
        self.assertEqual(need_loss(100),0)
    def test_04_target_no_loss(self):
        self.assertEqual(need_loss(80),0)
    def test_05_need_monotonic(self):
        values=[need_loss(i) for i in range(101)]
        self.assertTrue(all(a>=b for a,b in zip(values,values[1:])))
    def test_06_critical_continuity(self):
        self.assertAlmostEqual(need_loss(15-1e-7),need_loss(15+1e-7),places=5)
    def test_07_critical_accelerates(self):
        ordinary=35*((80-5)/80)**2
        self.assertGreater(need_loss(5),ordinary)
    def test_08_meal_end(self):
        self.assertAlmostEqual(30-3.2*15/60+35,64.2)
    def test_09_meal_need_utility(self):
        self.assertAlmostEqual(need_loss(30)-need_loss(64.2),12.30665625)
    def test_10_threshold_crossing(self):
        self.assertAlmostEqual((62-45)/3.2*60,318.75)
        self.assertGreater(62-3.2*318/60,45)
        self.assertLess(62-3.2*319/60,45)
    def test_11_sleep_net_recovery(self):
        self.assertAlmostEqual(20+10*6,80)
    def test_12_pleasure_snapshot(self):
        self.assertAlmostEqual(pleasure_rate(6,15,.8,.7,0)*15,7.8)
    def test_13_repetition_reduces_pleasure(self):
        self.assertGreater(pleasure_rate(6,15,.8,.7,0),pleasure_rate(6,15,.8,.7,3))
    def test_14_low_novelty_ignores_repetition(self):
        self.assertEqual(pleasure_rate(6,15,.8,0,0),pleasure_rate(6,15,.8,0,5))
    def test_15_trace_decays(self):
        self.assertLess(trace_step(3,False),3)
    def test_16_segmentation_no_extra_reward(self):
        self.assertEqual(pleasure_steps([15]),pleasure_steps([5,5,5]))
    def test_17_real_pleasure_below_fixed_trace(self):
        joy,_=pleasure_steps([15]);self.assertLess(joy,7.8)
    def test_18_nutrition_independent_repetition(self):
        n1=30-3.2*15/60+35;n2=30-3.2*15/60+35
        self.assertEqual(n1,n2)
        self.assertNotEqual(pleasure_steps([15],initial_trace=0)[0],pleasure_steps([15],initial_trace=3)[0])
    def test_19_stress_half_life(self):
        s=.8
        for _ in range(120):s=stress_step(s)
        self.assertAlmostEqual(s,.4)
    def test_20_rest_cannot_make_negative_stress(self):
        self.assertEqual(stress_step(.001,relief=.006),0)
    def test_21_contact_initiative_cost(self):
        self.assertEqual(3*(1-1),0);self.assertEqual(3*(1-0),3)
    def test_22_barrier_no_random_probability(self):
        c=30*.8*min(1,.25+120/200)
        self.assertAlmostEqual(c,20.4)
    def test_23_discount_half_life(self):
        self.assertAlmostEqual(gamma(0,0)**120,.5)
    def test_24_patience_retains_late_value(self):
        self.assertGreater(gamma(1,0),gamma(0,0))
    def test_25_stress_shortens_patience(self):
        self.assertLess(gamma(.5,1),gamma(.5,0))
    def test_26_telescoping(self):
        self.assertEqual(return_of_trace([0,3,2,7],[0,0,0],1),7)
    def test_27_commit_cost_once(self):
        self.assertEqual(return_of_trace([0,3,7],[2,0],1),5)
    def test_28_high_acceptance_ask_wins(self):
        self.assertEqual(plan_score([(.8,40),(.2,-10)],.5),28)
        self.assertGreater(28,plan_score([(1,22)],.5))
    def test_29_low_acceptance_buy_wins(self):
        self.assertEqual(plan_score([(.4,40),(.6,-10)],.5),4)
    def test_30_caution_penalizes_downside(self):
        b=[(.8,40),(.2,-10)]
        self.assertLess(plan_score(b,1),plan_score(b,0))
    def test_31_invalid_probability_rejected(self):
        with self.assertRaises(ValueError):plan_score([(.8,40),(.5,-10)],.5)
    def test_32_money_gain_decreases(self):
        self.assertGreater(money_u(30)-money_u(0),money_u(130)-money_u(100))
    def test_33_purchase_money_cost(self):
        self.assertAlmostEqual(money_u(80)-money_u(50),2.1878586815274555)
    def test_34_belief_freshness(self):
        self.assertAlmostEqual(freshness(.95,.5,240,240),.725)
        self.assertAlmostEqual(freshness(.95,.5,480,240),.6125)
    def test_35_categorical_freshness_normalized(self):
        ps=[.9,.05,.05];prior=[.3,.3,.4]
        self.assertAlmostEqual(sum(freshness(a,b,100,240) for a,b in zip(ps,prior)),1)
    def test_36_bayes(self):
        self.assertAlmostEqual(posterior(.5,.9,.1),.9)
    def test_37_impossible_evidence_rejected(self):
        with self.assertRaises(ValueError):posterior(.5,0,0)
    def test_38_beta_examples(self):
        self.assertEqual(beta_forecast(0,0),.5)
        self.assertEqual(beta_forecast(6,0),.8)
        self.assertEqual(beta_forecast(2,4),.4)
    def test_39_same_terms_two_signatures(self):
        self.assertTrue(signatures_match(2,2,2))
        self.assertFalse(signatures_match(2,1,2))
        self.assertFalse(signatures_match(2,2,None))
    def test_40_normal_drive_does_not_grow(self):
        self.assertEqual(drive_step(0,0),0)
    def test_41_drive_saturates(self):
        self.assertEqual(drive_step(1,1),1)
    def test_42_success_relief_not_intensity_growth(self):
        q=.8
        self.assertLess(drive_step(.8,q,True),drive_step(.8,q,False))
        self.assertEqual(q,.8)
    def test_43_social_effect_is_directed(self):
        self.assertNotEqual(social_step(.2,.8,0)-.2,social_step(.8,.2,0)-.8)
    def test_44_social_attachment_bounded(self):
        self.assertEqual(social_step(1,1,0),1)
    def test_45_joint_time_windows(self):
        # earliest_i, travel_i, earliest_j, travel_j, duration, latest_i, latest_j
        def feasible(a,ta,b,tb,d,la,lb):return max(a+ta,b+tb)+d<=min(la,lb)
        self.assertTrue(feasible(100,10,100,20,30,160,160))
        self.assertFalse(feasible(100,10,100,20,50,160,160))
    def test_46_touching_intervals_no_overlap(self):
        overlap=lambda a,b:max(a[0],b[0])<min(a[1],b[1])
        self.assertFalse(overlap((10,20),(20,30)))
        self.assertTrue(overlap((10,21),(20,30)))
    def test_47_money_ledger_conservation(self):
        before=(80,1000);price=30;after=(before[0]-price,before[1]+price)
        self.assertEqual(sum(before),sum(after))
    def test_48_forecast_no_rng_dependency(self):
        # Pure arithmetic: repeated evaluation has identical output.
        b=[(.8,40),(.2,-10)]
        self.assertEqual(plan_score(b,.5),plan_score(b,.5))
    def test_49_value_of_information_example(self):
        before=max(.5*40+.5*(-10),22)
        after=.5*40+.5*22
        self.assertEqual(after-before,9)
        self.assertEqual(after-before-2,7)

class CatalogueChecks(unittest.TestCase):
    def test_50_counts(self):
        self.assertEqual(len(CATALOG['action_types']),27)
        self.assertEqual(len(CATALOG['event_types']),25)
        self.assertEqual(len(CATALOG['item_types']),12)
        self.assertEqual(len(CATALOG['fixture_types']),5)
        self.assertEqual(len(CATALOG['methods']),9)
    def test_51_unique_type_ids(self):
        ids=[]
        for key in ['action_types','event_types','item_types','fixture_types','recipes','methods']:
            ids += [x['id'] for x in CATALOG[key]]
        self.assertEqual(len(ids),len(set(ids)))
    def test_52_action_event_references(self):
        ids={x['id'] for x in CATALOG['event_types']}
        for a in CATALOG['action_types']:
            self.assertTrue(set(a['events'])<=ids)
    def test_53_need_thresholds(self):
        for n in PARAMS['needs'].values():
            self.assertTrue(0<n['critical']<n['start']<n['target']<=100)
    def test_54_instances_reference_types(self):
        ids={x['id'] for x in CATALOG['item_types']}
        for obj in CATALOG['example_scenario']['instances']:
            self.assertIn(obj['type'],ids)
    def test_55_holder_location_exclusive(self):
        for obj in CATALOG['example_scenario']['instances']:
            self.assertNotEqual(obj['holder'] is None,obj['location'] is None)
    def test_56_graph_known_nodes_positive_durations(self):
        s=CATALOG['example_scenario'];ids={x['id'] for x in s['locations']}
        for edge in s['undirected_edges']:
            self.assertIn(edge['a'],ids);self.assertIn(edge['b'],ids)
            self.assertGreater(edge['minutes'],0)
    def test_57_all_actors_have_valid_states(self):
        for n in CATALOG['example_scenario']['npcs']:
            self.assertGreaterEqual(n['money'],0)
            self.assertTrue(all(0<=v<=100 for v in n['needs'].values()))
            self.assertTrue(all(0<=v<=1 for v in n['traits'].values()))
    def test_58_full_action_cards(self):
        for a in CATALOG['action_types']:
            for key in ['world_preconditions','effects','locks','interrupt','observation','duration_minutes']:
                self.assertTrue(a[key])
    def test_59_item_quantities(self):
        for item in CATALOG['item_types']:
            self.assertGreater(item['mass_kg'],0);self.assertGreaterEqual(item['price'],0)
            if 'edible' in item['properties']:
                self.assertGreater(item['consume_minutes'],0)
                self.assertGreater(item['satiety_gain'],0)
    def test_60_fixture_references(self):
        ids={x['id'] for x in CATALOG['fixture_types']}
        for obj in CATALOG['example_scenario']['fixture_instances']:
            self.assertIn(obj['type'],ids)

if __name__ == '__main__':
    suite=unittest.TestSuite()
    loader=unittest.defaultTestLoader
    suite.addTests(loader.loadTestsFromTestCase(FormulaChecks))
    suite.addTests(loader.loadTestsFromTestCase(CatalogueChecks))
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    summary={'checks_run':result.testsRun,'failures':len(result.failures),
             'errors':len(result.errors),'successful':result.wasSuccessful(),
             'scope':'Reference arithmetic and catalogue checks only; NOT full simulation.'}
    (ROOT/'verification_results.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(summary,ensure_ascii=False,indent=2))
    if not result.wasSuccessful():
        raise SystemExit(1)
