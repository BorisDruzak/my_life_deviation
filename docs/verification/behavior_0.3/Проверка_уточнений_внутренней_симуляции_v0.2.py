"""Эталонные локальные расчёты и проверки спецификации 0.2.0.

Стандартная библиотека Python 3.10+. Это НЕ ядро НПС, не C++ и не интеграция
с миром. Наборы использованных событий здесь служат тестовым оракулом;
в ядре они должны опираться на ограниченную очередь/журнал v0.1.
Запуск из любого каталога: python <путь_к_этому_файлу>
"""
from __future__ import annotations
import inspect
import math
import random
import unittest
from dataclasses import dataclass, replace
from pathlib import Path

CHANNELS = ('pleasantness', 'leisure', 'social', 'activation', 'self')
SCALES = dict(zip(CHANNELS, (.5, .5, .5, .3, .3)))


def valid(x: float, low: float = 0, high: float = 1) -> float:
    if not math.isfinite(x) or not low <= x <= high:
        raise ValueError(f'Число вне диапазона [{low};{high}]: {x}')
    return x


def clip(x: float, low: float = -1, high: float = 1) -> float:
    if not math.isfinite(x):
        raise ValueError('Число должно быть конечным')
    return min(high, max(low, x))


@dataclass(frozen=True)
class Stats:
    mean: float = 0
    variance: float = .25
    count: float = 0

    @property
    def support(self) -> float:
        return self.count / (self.count + 4) * (1 - clip(self.variance, 0, 1))


def update_stats(s: Stats, result: float, alpha: float, count: float) -> Stats:
    valid(result, -1, 1); valid(alpha); valid(count, 0, math.inf)
    valid(s.mean, -1, 1); valid(s.variance, 0, 1); valid(s.count, 0, math.inf)
    if alpha == 0:
        return s
    error = result - s.mean
    return Stats(s.mean + alpha * error,
                 (1 - alpha) * (s.variance + alpha * error * error), s.count + count)


def associative(s: Stats, result: float | None, *, learning: float = .5,
                quality: float = 1, memory: float = .5, dose: float = 1,
                conscious: bool = True) -> Stats:
    for value in (learning, quality, memory, dose): valid(value)
    if result is None or not conscious:
        return s
    alpha = .25 * learning * quality * dose * (.6 + .4 * memory)
    return update_stats(s, result, alpha, quality * dose)


def pleasantness(base: float, innate: float = 0, *, learned: float = 0,
                 relationship: float = 0, context: float = 0, costs: float = 0,
                 engagement: float = 1, saturation: float = 0,
                 sensitivity: float = .4) -> float:
    for value in (base, innate, learned): valid(value, -.5, .5)
    for value in (relationship, context): valid(value, -.25, .25)
    for value in (costs, engagement, saturation): valid(value)
    valid(sensitivity, 0, .8)
    raw = base + innate + learned + relationship + context
    return engagement * clip((1-sensitivity*saturation)*max(raw,0)+min(raw,0)-costs)


def satiation(h: float, engagement: float, hours: float) -> float:
    valid(h); valid(engagement); valid(hours, 0, math.inf)
    if hours == 0: return h
    rate = .35 * engagement + .20 * (1-engagement)
    target = .35 * engagement / rate
    return target + (h-target)*math.exp(-rate*hours)


def reservoir(x: float, inflow: float, outflow: float, leak: float, hours: float) -> float:
    valid(x)
    for value in (inflow,outflow,leak,hours): valid(value, 0, math.inf)
    if hours == 0: return x
    rate = inflow + outflow
    if rate == 0: return clip(x-leak*hours,0,1)
    target = (inflow-leak)/rate
    return clip(target+(x-target)*math.exp(-rate*hours),0,1)


def leisure_rate(leisure: float, pleasure: float, direct: float = 0, need: float = 1) -> float:
    valid(leisure); valid(pleasure,-1,1); valid(direct,-.5,.5); valid(need,0,1.5)
    inflow=.8*max(pleasure,0)+max(direct,0)
    outflow=.3*max(-pleasure,0)+max(-direct,0)
    return inflow*(1-leisure)-outflow*leisure-.06*need


def emotion_targets(pleasure: float, *, significance: float = 1, progress: float = 0,
                    surprise_gain: float = 0, novelty: float = 0,
                    comprehensibility: float = .5, useful_question: float = 0,
                    immediate_harm: float = 0) -> tuple[float,float]:
    valid(pleasure,-1,1)
    for value in (significance,progress,surprise_gain,novelty,comprehensibility,
                  useful_question,immediate_harm): valid(value)
    joy=significance*clip(.5*max(pleasure,0)+.3*progress+.2*surprise_gain,0,1)
    interest=significance*clip(.55*novelty*(.3+.7*comprehensibility)+
                              .3*max(pleasure,0)+.15*useful_question,0,1)*(1-.7*immediate_harm)
    return joy,interest


@dataclass(frozen=True)
class Experience:
    event_id: str
    result: float | None
    presence: bool | None
    context: frozenset[str]
    quality: float = 1
    order_known: bool = True


def contrast_pair(known_absence: bool = True) -> tuple[Experience,Experience]:
    context=frozenset({'gym','evening'})
    return (Experience('plus',.6,True,context),
            Experience('minus',0,False if known_absence else None,context))


def causal_update(s: Stats, positive: Experience, negative: Experience, used: set[str], *,
                  learning: float = .5, causality: float = .5,
                  knowledge: int = 4) -> tuple[Stats,set[str]]:
    valid(learning); valid(causality); valid(knowledge,0,4)
    if type(knowledge) is not int: raise ValueError('Уровень знаний должен быть целым')
    for e in (positive,negative):
        valid(e.quality)
        if e.result is not None: valid(e.result,-1,1)
    if (positive.presence is not True or negative.presence is not False or
        positive.result is None or negative.result is None or
        not positive.order_known or not negative.order_known or
        positive.event_id == negative.event_id or
        positive.event_id in used or negative.event_id in used):
        return s,used
    union=positive.context | negative.context
    similarity=len(positive.context & negative.context)/len(union) if union else 0
    if similarity < .6: return s,used
    quality=min(positive.quality,negative.quality)*similarity
    alpha=.20*learning*quality*causality*(.2+.8*knowledge/4)
    if alpha == 0: return s,used
    result=clip(positive.result-negative.result)
    return update_stats(s,result,alpha,quality), used | {positive.event_id,negative.event_id}


class Reactions:
    """Разреженный численный оракул: не реализация хранения или очереди событий."""
    def __init__(self, weights: dict[tuple[str,str],float] | None = None):
        self.weights=dict(weights or {})
        if len(self.weights)>512: raise ValueError('Превышен лимит пар')
        for (feature,channel),weight in self.weights.items():
            if not feature or channel not in CHANNELS: raise ValueError('Неизвестный канал/признак')
            valid(weight,-1,1)
        self.decayed={key:0.0 for key in self.weights}
        self.last_active={key:0.0 for key in self.weights}
        self.used: set[str]=set()

    @staticmethod
    def select(features: dict[str,float]) -> dict[str,float]:
        for name,value in features.items():
            if not name: raise ValueError('Нужен идентификатор признака')
            valid(value)
        return dict(sorted(features.items(),key=lambda item:(-item[1],item[0]))[:16])

    def effects(self, features: dict[str,float]) -> dict[str,float]:
        selected=self.select(features)
        return {j:SCALES[j]*clip(math.fsum(selected[f]*self.weights.get((f,j),0)
                                          for f in sorted(selected))) for j in CHANNELS}

    def update(self, event_id: str, features: dict[str,float], teaching: dict[str,float], *,
               qualities: dict[str,float] | None = None, plasticity: float = .5,
               dose: float = 1, day: float = 0) -> tuple[int,int]:
        valid(plasticity); valid(dose); valid(day,0,math.inf)
        for j,y in teaching.items():
            if j not in CHANNELS: raise ValueError('Незарегистрированный канал')
            valid(y,-1,1)
        q={j:(qualities or {}).get(j,1) for j in teaching}
        for value in q.values(): valid(value)
        selected=self.select(features)
        if event_id in self.used: return 0,0
        self.used.add(event_id)
        if plasticity*dose == 0: return 0,0
        # Чисто локальные тесты используют полные множества событий; в ядре нужен bounded ledger.
        active={(f,j) for f in selected for j in CHANNELS if (f,j) in self.weights}
        for key in sorted(active):
            elapsed=day-self.decayed[key]
            if elapsed<0: raise ValueError('Время пошло назад')
            self.weights[key]*=math.exp(-math.log(2)*elapsed/365)
            self.decayed[key]=day
            self.last_active[key]=day
        new_features=[f for f in selected if selected[f]>=.25][:2]
        new_channels=sorted((j for j in teaching if abs(teaching[j])>=.1 and q[j]>=.2),
                            key=lambda j:(-q[j]*abs(teaching[j]),j))[:2]
        created=0
        for f in new_features:
            for j in new_channels:
                key=(f,j)
                if key in self.weights: continue
                if len(self.weights)>=512:
                    # В ядре это индекс слабых неактуальных пар, а не полный обход.
                    evictable=[k for k in self.weights if k not in active and
                               abs(self.weights[k])<.02 and day-self.last_active[k]>=30]
                    if not evictable: continue
                    victim=min(evictable,key=lambda k:(self.last_active[k],k))
                    for mapping in (self.weights,self.decayed,self.last_active): del mapping[victim]
                self.weights[key]=0
                self.decayed[key]=day
                self.last_active[key]=day
                created+=1
        snapshot=dict(self.weights)
        updates={}
        for j in sorted(teaching):
            if q[j]==0: continue
            keys=sorted((f,j) for f in selected if (f,j) in snapshot)
            predicted=clip(math.fsum(selected[f]*snapshot[(f,j)] for f,j in keys))
            error=teaching[j]-predicted
            normalizer=max(1,math.fsum(selected[f]**2 for f,j in keys))
            eta=.02*plasticity*q[j]*dose
            for f,_ in keys:
                updates[(f,j)]=clip(snapshot[(f,j)]+eta*selected[f]/normalizer*error)
        self.weights.update(updates)
        return created,len(updates)


def knowledge_catalog() -> tuple[dict[str,tuple[int,tuple[str,...]]],list[set[str]]]:
    rules={f'r{i}':(i,() if i==1 else (f'r{i-1}',)) for i in range(1,5)}
    return rules,[{f'r{i}' for i in range(1,level+1)} for level in range(1,5)]


def knowledge_depth(rules: dict[str,tuple[int,tuple[str,...]]], cores: list[set[str]],
                    mastery: dict[str,float]) -> int:
    if len(cores)!=4: raise ValueError('Нужно четыре уровня')
    previous: set[str]=set()
    for index,core in enumerate(cores,1):
        if (not core or not previous<core or not core<=rules.keys() or
            any(rules[r][0]>index for r in core) or
            not any(rules[r][0]==index for r in core)):
            raise ValueError('Невложенное, пустое или неизвестное ядро знаний')
        previous=core
    for value in mastery.values(): valid(value)
    visiting,done=set(),set()
    def validate_graph(r: str) -> None:
        if r in visiting: raise ValueError('Цикл предпосылок')
        if r in done: return
        if r not in rules: raise ValueError('Неизвестная предпосылка')
        level,prerequisites=rules[r]
        if type(level) is not int or not 1<=level<=4: raise ValueError('Неверный уровень')
        visiting.add(r)
        for parent in prerequisites: validate_graph(parent)
        visiting.remove(r);done.add(r)
    for r in rules: validate_graph(r)
    def usable(r: str) -> bool:
        return mastery.get(r,0)>=.6 and all(usable(p) for p in rules[r][1])
    return max([0]+[i+1 for i,core in enumerate(cores) if all(usable(r) for r in core)])


def learn_rule(g: float, *, content: bool = True, prerequisites: bool = True,
               learning: float = .5, quality: float = 1, dose: float = 1,
               causality: float = .5) -> float:
    for value in (g,learning,quality,dose,causality): valid(value)
    if not content: return g
    alpha=.25*learning*quality*dose*(.4+.6*causality)
    new=g+alpha*(1-g)
    return new if prerequisites else max(g,min(.59,new))


def new_reasoning_depth(causality: float, knowledge: int) -> int:
    valid(causality)
    if type(knowledge) is not int or not 0<=knowledge<=4: raise ValueError('Неверный уровень знаний')
    return min(1+math.floor(3*causality),knowledge)


def prediction(association: float, corrections: list[tuple[float,float,float|None,float|None]]) -> float:
    valid(association,-1,1)
    if len(corrections)>4: raise ValueError('Лимит извлечённых гипотез: 4')
    result=association
    for effect,support,current,usual in corrections:
        valid(effect,-1,1);valid(support)
        if current is None or usual is None: continue
        valid(current);valid(usual)
        result+=effect*support*(current-usual)
    return clip(result)

class Checks(unittest.TestCase):
    def test_association_at_zero_causality(self):
        state = Stats()
        for _ in range(20):
            state = associative(state, .8, learning=.5, quality=1, memory=.5, dose=1)
        self.assertAlmostEqual(state.mean, .7027386763275446)
        self.assertGreater(state.mean, .7)

    def test_association_has_no_causality_parameter(self):
        self.assertNotIn('causality', inspect.signature(associative).parameters)
        self.assertNotIn('knowledge', inspect.signature(associative).parameters)

    def test_association_memory_zero_still_learns(self):
        self.assertGreater(associative(Stats(), .8, memory=0).mean, 0)

    def test_missing_result_does_not_mean_zero(self):
        s = Stats(.6, .1, 4)
        self.assertEqual(associative(s, None), s)
        self.assertLess(associative(s, 0).mean, s.mean)

    def test_association_no_perception_learning_dose_or_consciousness(self):
        s = Stats(.2, .1, 5)
        for kw in ({'quality':0}, {'learning':0}, {'dose':0}, {'conscious':False}):
            self.assertEqual(associative(s, .8, **kw), s)

    def test_association_variance_bounds(self):
        rng = random.Random(20260910)
        state = Stats()
        for _ in range(10000):
            state = associative(state, rng.uniform(-1,1), memory=rng.random(), quality=rng.random())
            self.assertTrue(-1 <= state.mean <= 1)
            self.assertTrue(0 <= state.variance <= 1 + 1e-12)

    def test_familiar_activity_pleasure_and_leisure(self):
        p = pleasantness(.1, .5, saturation=1)
        self.assertAlmostEqual(p, .36)
        self.assertAlmostEqual(leisure_rate(.5, p), .084)
        joy, interest = emotion_targets(p, novelty=0)
        self.assertGreater(interest, 0)
        self.assertGreater(reservoir(.5, .8*p, 0, .06, .1), .5)

    def test_pleasantness_does_not_depend_on_novelty_or_joy(self):
        params = inspect.signature(pleasantness).parameters
        self.assertNotIn('novelty', params)
        self.assertNotIn('joy', params)
        self.assertNotIn('interest', params)

    def test_pleasantness_requires_exposure(self):
        self.assertEqual(pleasantness(.5, .5, engagement=0), 0)

    def test_negative_effect_not_softened_by_satiation(self):
        self.assertEqual(pleasantness(-.3, saturation=0), pleasantness(-.3, saturation=1))
        self.assertAlmostEqual(pleasantness(.1,.5,costs=.8,saturation=1), -.44)

    def test_satiation_rises_and_recovers(self):
        self.assertGreater(satiation(.4, 1, 1), .4)
        self.assertLess(satiation(.4, 0, 1), .4)
        self.assertEqual(satiation(.4, 1, 0), .4)

    def test_satiation_constant_exposure_partition(self):
        self.assertAlmostEqual(satiation(.2,.6,1), satiation(satiation(.2,.6,.4),.6,.6))

    def test_leisure_constant_inputs_partition(self):
        self.assertAlmostEqual(reservoir(.5,.3,.1,.06,1),reservoir(reservoir(.5,.3,.1,.06,.4),.3,.1,.06,.6))
        self.assertEqual(reservoir(.5,.3,.1,.06,0),.5)

    def test_reservoir_bounds_and_passive_drift(self):
        self.assertAlmostEqual(reservoir(.5,0,0,.06,1),.44)
        for duration in [0,1,10,1000]:
            self.assertTrue(0 <= reservoir(.5,.3,.1,.06,duration) <= 1)

    def test_causal_learning_zero_causality(self):
        p,n = contrast_pair()
        s, used = causal_update(Stats(),p,n,set(),causality=0)
        self.assertEqual(s, Stats())
        self.assertEqual(used, set())

    def test_causal_unknown_absence_rejected(self):
        p,n = contrast_pair(known_absence=False)
        self.assertEqual(causal_update(Stats(),p,n,set())[0],Stats())

    def test_causal_no_comparable_conditions(self):
        p,n = contrast_pair()
        n = replace(n, context=frozenset({'night'}))
        self.assertEqual(causal_update(Stats(),p,n,set())[0],Stats())
        p = replace(p, context=frozenset())
        n = replace(n, context=frozenset())
        self.assertEqual(causal_update(Stats(),p,n,set())[0],Stats())

    def test_causal_missing_order_rejected(self):
        p,n = contrast_pair()
        self.assertEqual(causal_update(Stats(),replace(p,order_known=False),n,set())[0],Stats())

    def test_causal_missing_outcome_rejected(self):
        p,n = contrast_pair()
        self.assertEqual(causal_update(Stats(),p,replace(n,result=None),set())[0],Stats())

    def test_causal_comparison_changes_separate_record(self):
        p,n = contrast_pair()
        state,used=causal_update(Stats(),p,n,set(),causality=.5,knowledge=4)
        self.assertAlmostEqual(state.mean,.03)
        self.assertEqual(used,{'plus','minus'})

    def test_causal_reusing_evidence_does_not_add_support(self):
        p,n = contrast_pair()
        s,used=causal_update(Stats(),p,n,set())
        self.assertEqual(causal_update(s,p,n,used),(s,used))
        self.assertEqual(causal_update(s,replace(p,event_id='new'),n,used),(s,used))

    def test_causal_data_not_replaced_by_intelligence(self):
        p,n = contrast_pair(known_absence=False)
        s,_=causal_update(Stats(),p,n,set(),causality=1,knowledge=4)
        self.assertEqual(s.count,0)
        self.assertEqual(s.support,0)

    def test_property_channel_isolation(self):
        model=Reactions({('Anya','self'): .3, ('Anya','social'): .1})
        before=model.weights[('Anya','self')]
        model.update('1',{'Anya':1},{'social':.8})
        self.assertEqual(model.weights[('Anya','self')],before)
        self.assertGreater(model.weights[('Anya','social')],.1)
        self.assertNotIn(('Anya','activation'),model.weights)

    def test_property_order_invariance(self):
        a=Reactions({('a','social'):.1,('b','social'):.2})
        b=Reactions({('b','social'):.2,('a','social'):.1})
        a.update('1',{'a':.6,'b':.9},{'social':.8})
        b.update('1',{'b':.9,'a':.6},{'social':.8})
        self.assertEqual(a.weights,b.weights)

    def test_property_shared_error_not_full_reward_per_cue(self):
        m=Reactions({('a','social'):0,('b','social'):0})
        m.update('1',{'a':1,'b':1},{'social':1},plasticity=1)
        self.assertAlmostEqual(m.weights[('a','social')],.01)
        self.assertAlmostEqual(sum(m.weights.values()),.02)

    def test_property_observed_zero_extinguishes(self):
        m=Reactions({('a','pleasantness'):.6})
        m.update('1',{'a':1},{'pleasantness':0})
        self.assertLess(m.weights[('a','pleasantness')],.6)

    def test_property_missing_channel_not_extinguished(self):
        m=Reactions({('a','pleasantness'):.6})
        m.update('1',{'a':1},{})
        self.assertEqual(m.weights[('a','pleasantness')],.6)

    def test_property_self_response_is_not_teacher(self):
        experienced=pleasantness(0,learned=.5)
        teacher=pleasantness(0,learned=0)
        self.assertGreater(experienced,0)
        self.assertEqual(teacher,0)
        m=Reactions({('a','pleasantness'):1})
        m.update('1',{'a':1},{'pleasantness':teacher})
        self.assertLess(m.weights[('a','pleasantness')],1)

    def test_property_no_duplicate_episode(self):
        m=Reactions()
        m.update('1',{'a':1},{'social':.8})
        saved=dict(m.weights)
        report=m.update('1',{'a':1},{'social':.8})
        self.assertEqual(m.weights,saved)
        self.assertEqual(report,(0,0))

    def test_property_new_links_bounded_for_large_event(self):
        m=Reactions()
        report=m.update('1',{f'f{i:03}':1 for i in range(200)},{j:.8 for j in CHANNELS})
        self.assertEqual(report[0],4)
        self.assertEqual(len(m.weights),4)
        self.assertEqual(len({f for f,j in m.weights}),2)

    def test_property_limit_of_existing_lookups(self):
        m=Reactions({(f'f{i:03}',j):.1 for i in range(16) for j in CHANNELS})
        created, updated=m.update('1',{f'f{i:03}':1 for i in range(200)},{j:.8 for j in CHANNELS})
        self.assertEqual(created,0)
        self.assertEqual(updated,80)

    def test_property_global_cap_blocks_new_strong_records(self):
        m=Reactions({(f'old{i:03}','social'):.4 for i in range(512)})
        created,_=m.update('1',{'new':1},{'social':.8},day=40)
        self.assertEqual(created,0)
        self.assertEqual(len(m.weights),512)

    def test_property_global_cap_allows_bounded_weak_eviction(self):
        m=Reactions({(f'old{i:03}','social'):.001 for i in range(512)})
        created,_=m.update('1',{'new':1},{'social':.8},day=40)
        self.assertEqual(created,1)
        self.assertEqual(len(m.weights),512)
        self.assertIn(('new','social'),m.weights)

    def test_property_no_invented_channels(self):
        with self.assertRaises(ValueError): Reactions().update('1',{'a':1},{'unknown':.8})

    def test_property_effect_units_and_bounds(self):
        m=Reactions({('Anya','social'):.5,('Anya','self'):1/3})
        effects=m.effects({'Anya':1})
        self.assertAlmostEqual(effects['social'],.25)
        self.assertAlmostEqual(effects['self'],.1)

    def test_high_intelligence_no_domain_knowledge(self):
        self.assertEqual(new_reasoning_depth(1,0),0)
        self.assertEqual(new_reasoning_depth(.5,4),2)
        self.assertEqual(new_reasoning_depth(1,4),4)

    def test_knowledge_depth_requires_records(self):
        rules,cores=knowledge_catalog()
        self.assertEqual(knowledge_depth(rules,cores,{}),0)
        self.assertEqual(knowledge_depth(rules,cores,{r:.8 for r in rules}),4)

    def test_knowledge_missing_prerequisite_blocks_depth(self):
        rules,cores=knowledge_catalog()
        g={r:.8 for r in rules};g['r2']=.59
        self.assertEqual(knowledge_depth(rules,cores,g),1)

    def test_knowledge_content_required(self):
        self.assertEqual(learn_rule(0,content=False,causality=1),0)
        self.assertGreater(learn_rule(0,content=True,causality=1),0)

    def test_knowledge_unlearned_prerequisite_caps_partial_learning(self):
        self.assertEqual(learn_rule(.59,prerequisites=False),.59)
        self.assertEqual(learn_rule(.8,prerequisites=False),.8)
        self.assertGreater(learn_rule(.59,prerequisites=True),.6)

    def test_knowledge_curriculum_cycle_rejected(self):
        rules={'a':(1,('b',)),'b':(1,('a',))}
        cores=[{'a','b'}]*4
        with self.assertRaises(ValueError): knowledge_depth(rules,cores,{'a':1,'b':1})

    def test_knowledge_non_nested_core_rejected(self):
        rules,cores=knowledge_catalog()
        cores[1]={'r2'}
        with self.assertRaises(ValueError): knowledge_depth(rules,cores,{r:1 for r in rules})

    def test_knowledge_repeated_one_rule_not_four_levels(self):
        rules={'r1':(1,())}
        with self.assertRaises(ValueError): knowledge_depth(rules,[{'r1'}]*4,{'r1':1})

    def test_knowledge_advanced_rule_not_in_beginner_core(self):
        rules,cores=knowledge_catalog()
        cores[0]={'r4'}
        with self.assertRaises(ValueError): knowledge_depth(rules,cores,{r:1 for r in rules})

    def test_property_zero_quality_no_new_link(self):
        m=Reactions()
        created,updated=m.update('1',{'a':1},{'social':.8},qualities={'social':0})
        self.assertEqual((created,updated),(0,0))

    def test_prediction_no_double_count_same_context(self):
        self.assertEqual(prediction(.6,[(.8,.9,1,1)]),.6)
        self.assertEqual(prediction(.6,[(.8,.9,None,1)]),.6)
        self.assertEqual(prediction(.6,[]),.6)
        self.assertLess(prediction(.6,[(.8,.9,0,1)]),.6)

    def test_emotion_max_stays_max_not_saturated_sum(self):
        self.assertEqual(max([.2]*10),.2)
        self.assertAlmostEqual(1-math.prod(1-x for x in [.2]*10),.8926258176)

    def test_invalid_inputs(self):
        with self.assertRaises(ValueError): pleasantness(float('nan'))
        with self.assertRaises(ValueError): satiation(.5,.5,-1)
        with self.assertRaises(ValueError): new_reasoning_depth(1,5)
        with self.assertRaises(ValueError): associative(Stats(),.5,quality=2)

    def test_spec_status_and_supersession(self):
        path=Path(__file__).resolve().parents[3]/'docs'/'rules'/'behavior_0.3'/'Внутренняя_симуляция_человека_v0.2.md'
        text=path.read_text(encoding='utf-8')
        self.assertIn('**Требует решения**; не исключено архитектурно',text)
        self.assertIn('Сохраняется максимум по причинам',text)
        self.assertIn('**Причинного мышления в коэффициенте нет.**',text)
        self.assertIn('четыре новые пары за эпизод',text)

if __name__ == '__main__':
    unittest.main(verbosity=2)
