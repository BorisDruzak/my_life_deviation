"""Численная проверка проекта спецификации v0.1, НЕ реализация ядра НПС.
Запуск: python Проверка_формул_внутренней_симуляции.py
Проверяет отдельные формулы и инварианты, но не C++, мир или UE5.
"""
import hashlib
import json
import math
import random
import unittest
from pathlib import Path

WEIGHTS = {
    'наблюдательность': [.35,.10,.15,.10,.25,.20,.20],
    'внимание': [.60,.15,.20,.10,.50,.25,.45],
    'рабочая память': [.50,.10,.20,.10,.35,.20,.40],
    'запоминание': [.30,.10,.15,.05,.25,.15,.25],
    'скорость': [.40,.20,.20,.10,.30,.20,.30],
    'причинное мышление': [.45,.15,.15,.10,.30,.20,.45],
    'гибкость': [.40,.10,.15,.10,.25,.15,.50],
    'самоконтроль': [.45,.15,.15,.10,.35,.20,.35],
}

def clip(x, low=0.0, high=1.0):
    if not math.isfinite(x):
        raise ValueError('Значение должно быть конечным')
    return min(high, max(low, x))

def relax(x, target, dt, tau):
    if dt < 0 or tau <= 0 or not all(map(math.isfinite, [x,target,dt,tau])):
        raise ValueError('Недопустимое время или значение')
    if dt == 0:
        return x
    return target + (x-target) * math.exp(-dt/tau)

def capacities(base, deficits, gate=1.0):
    return {name: base * gate * math.prod(1-w*d for w,d in zip(row, deficits))
            for name,row in WEIGHTS.items()}

def context(memory, attention, gate=1.0):
    return 0 if gate == 0 else int(clip(math.floor(2+6*memory+4*attention),2,12))

def emotions(z=0,v=0,b=0,k=.5,n=1,p=0,a=0,l=0,r=0,o=0,rs=0,no=0,po=0,nk=0,os=0,nm=0,so=0):
    return {
        'страх': z*v*b*(.35+.65*(1-k)),
        'тревога': z*v*(1-.8*b)*(.35+.65*n)*(1-.5*k),
        'гнев': z*max(p,v*b)*a*(.4+.6*k),
        'печаль': z*l*(.4+.6*(1-k)),
        'радость': z*clip(.7*r+.3*o),
        'удивление': z*rs,
        'отвращение': z*nk,
        'интерес': z*no*(.3+.7*po)*(1-.7*v*b),
        'стыд': z*os,
        'вина': z*nm*so,
    }

def threshold(attention, commitment):
    return .06+.18*attention+.12*commitment

def should_switch(current, new, attention, commitment, dwell, emergency=False):
    return emergency or (new-current > threshold(attention,commitment)
                         and dwell >= 2+4*attention)

def switch_time(flexibility, attention, difference):
    return .25+1.25*(1-flexibility)+.75*(1-attention)+.75*difference

def food_water(e,w,dt,load=0,temp=0,activation=0,me=1,mw=1):
    h=dt/3600
    return (clip(e-h*.035*me*(1+load+.4*max(0,-temp)+.15*activation)),
            clip(w-h*.045*mw*(1+1.5*load+2*max(0,temp)+.15*activation)))

def sleep_pressure(s,dt,asleep=0,pain=0,temp=0,activation=0,need=1,recovery=1):
    quality=(1-.5*pain)*(1-.5*abs(temp))*(1-.3*activation)
    return clip(s+dt/3600*(.045*need*(1-asleep)-.09*recovery*quality*asleep))

def signal(deficit,sensitivity=1):
    return clip(sensitivity*deficit*(1+.5*deficit))

def sensation(signal_value,attention_share=0,anxiety=0):
    return clip(signal_value*(.35+.65*attention_share)*(1+.2*anxiety))

def learn(mean,variance,result,alpha):
    return mean+alpha*(result-mean), (1-alpha)*(variance+alpha*(result-mean)**2)

class FormulaChecks(unittest.TestCase):
    def test_zero_time(self):
        self.assertEqual(relax(.3,.9,0,5),.3)
        self.assertEqual(food_water(.8,.8,0),(.8,.8))
        self.assertEqual(sleep_pressure(.5,0),.5)

    def test_invalid_inputs(self):
        for dt,tau in [(-1,5),(1,0),(1,-2),(float('nan'),1)]:
            with self.assertRaises(ValueError): relax(.3,.5,dt,tau)
        with self.assertRaises(ValueError): clip(float('nan'))

    def test_relax_bounds_and_monotonicity(self):
        rng=random.Random(20260910)
        for _ in range(10000):
            x,target,dt,tau=rng.random(),rng.random(),rng.random()*100,rng.random()*100+.01
            y=relax(x,target,dt,tau)
            self.assertGreaterEqual(y,min(x,target)-1e-14)
            self.assertLessEqual(y,max(x,target)+1e-14)

    def test_relax_constant_target_partition(self):
        self.assertAlmostEqual(relax(relax(.2,.8,10,30),.8,20,30),relax(.2,.8,30,30),places=14)

    def test_fear_control_example(self):
        es=emotions(z=.8,v=.7,b=.9,k=.2,n=.6)
        self.assertAlmostEqual(es['страх'],.43848,places=12)
        self.assertAlmostEqual(es['тревога'],.1044288,places=12)
        self.assertEqual(es['гнев'],0)

    def test_no_appraisal_no_emotion(self):
        self.assertTrue(all(x==0 for x in emotions().values()))
        for reaction in [1,1.2,1.6]:
            self.assertTrue(all(clip(x*reaction)==0 for x in emotions().values()))

    def test_independent_fear_and_anger(self):
        es=emotions(z=1,v=1,b=1,k=.5,p=1,a=1)
        self.assertGreater(es['страх'],0)
        self.assertGreater(es['гнев'],0)
        self.assertGreater(sum(es.values()),1)

    def test_emotion_base_bounds(self):
        rng=random.Random(9)
        names='z v b k n p a l r o rs no po nk os nm so'.split()
        for _ in range(3000):
            es=emotions(**{k:rng.random() for k in names})
            self.assertTrue(all(0<=e<=1 for e in es.values()))

    def test_emotion_decay(self):
        e=.8
        for _ in range(1000):
            new=relax(e,0,1,30)
            self.assertLess(new,e)
            e=new
        self.assertLess(e,1e-12)

    def test_baseline_capacities(self):
        q=capacities(.5,[0]*7)
        self.assertTrue(all(v==.5 for v in q.values()))
        self.assertEqual(context(q['рабочая память'],q['внимание']),7)
        self.assertEqual(4+math.floor(28*q['скорость']),18)

    def test_sleep_deficit_example(self):
        q=capacities(.5,[1,0,0,0,0,0,0])
        self.assertAlmostEqual(q['внимание'],.2)
        self.assertAlmostEqual(q['рабочая память'],.25)
        self.assertAlmostEqual(q['скорость'],.3)
        self.assertEqual(context(q['рабочая память'],q['внимание']),4)
        self.assertEqual(4+math.floor(28*q['скорость']),12)

    def test_cognition_bounds_and_no_boost(self):
        rng=random.Random(2)
        for _ in range(3000):
            base=rng.random()
            q=capacities(base,[rng.random() for _ in range(7)],rng.random())
            self.assertTrue(all(0<=v<=base for v in q.values()))

    def test_zero_gate(self):
        self.assertTrue(all(v==0 for v in capacities(.8,[0]*7,0).values()))
        self.assertEqual(context(0,0,0),0)

    def test_context_total_bounds(self):
        for m in [0,.2,.5,.8,1]:
            for a in [0,.2,.5,.8,1]:
                self.assertTrue(2<=context(m,a)<=12)

    def test_focus_hysteresis(self):
        self.assertAlmostEqual(threshold(.5,.7),.234)
        self.assertFalse(should_switch(.46,.52,.5,.7,30))
        self.assertTrue(should_switch(.46,.82,.5,.7,4))
        self.assertFalse(should_switch(.46,.82,.5,.7,3))

    def test_emergency_preempts_dwell(self):
        self.assertTrue(should_switch(.9,.1,.8,.9,0,True))

    def test_switch_price(self):
        self.assertAlmostEqual(switch_time(.5,.5,.8),1.85)
        self.assertEqual(switch_time(1,1,0),.25)

    def test_food_water_resource_bounds(self):
        for seconds in [0,1,3600,86400,864000]:
            e,w=food_water(.8,.8,seconds,1,1,1)
            self.assertTrue(0<=e<=.8 and 0<=w<=.8)

    def test_heat_water_cold_food(self):
        e0,w0=food_water(.8,.8,3600)
        eh,wh=food_water(.8,.8,3600,temp=1)
        ec,wc=food_water(.8,.8,3600,temp=-1)
        self.assertEqual(e0,eh)
        self.assertLess(wh,w0)
        self.assertLess(ec,e0)
        self.assertEqual(w0,wc)

    def test_rest_not_sleep(self):
        self.assertGreater(sleep_pressure(.5,3600,0),.5)
        self.assertLess(sleep_pressure(.5,3600,1),.5)
        self.assertGreater(sleep_pressure(.5,3600,1,pain=1),sleep_pressure(.5,3600,1,pain=0))

    def test_sensation_no_phantom_signal(self):
        self.assertEqual(signal(0),0)
        self.assertEqual(sensation(0,1,1),0)
        self.assertLess(sensation(.8,0),sensation(.8,1))

    def test_perception_probability_partition(self):
        rate=.4
        self.assertAlmostEqual(1-math.exp(-rate*10),1-(math.exp(-rate*2)**5),places=14)
        self.assertEqual(1-math.exp(0),0)

    def test_learning_convex_and_finite(self):
        rng=random.Random(7)
        m,v=0,.25
        for _ in range(3000):
            result=rng.uniform(-1,1)
            alpha=rng.uniform(0,.2)
            m,v=learn(m,v,result,alpha)
            self.assertTrue(-1<=m<=1)
            self.assertTrue(0<=v<=1+1e-12)

    def test_slow_property_can_reverse(self):
        old=1
        new=old+.01*.5*1*abs(-1)*(-1-old)
        self.assertLess(new,old)
        self.assertEqual(new,.99)

    def test_stable_property_application(self):
        base=.5
        outputs=[clip(base*min(1.5,max(.5,1.2**.8))) for _ in range(100)]
        self.assertEqual(len(set(outputs)),1)

    def test_key_uniform_never_endpoints(self):
        for word in [0,1,2**32-2,2**32-1]:
            u=(word+.5)/4294967296
            self.assertTrue(0<u<1)

    def test_distinct_knowledge_and_truth(self):
        mean,var=0,.25
        for _ in range(500): mean,var=learn(mean,var,.8,.1)
        confidence=500/(500+4)*(1-var)
        self.assertGreater(confidence,.9)
        self.assertGreater(mean,.79)
        # Объективный иной результат намеренно не является входом обучения.

if __name__=='__main__':
    example=emotions(z=.8,v=.7,b=.9,k=.2,n=.6)
    print('Контрольный страх через 1 секунду:',format(relax(0,example['страх'],1,1),'.12f'))
    unittest.main(verbosity=2)
