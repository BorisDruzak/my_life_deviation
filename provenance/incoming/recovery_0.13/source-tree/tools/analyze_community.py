#!/usr/bin/env python3
"""Audit serialized community measurements. Model realism is separate from invariants."""
from __future__ import annotations
import argparse, collections, json, math, statistics
from pathlib import Path

def distribution(values: list[float]) -> dict:
    if not values: return {'n':0,'mean':None,'median':None,'min':None,'max':None}
    return {'n':len(values),'mean':statistics.fmean(values),'median':statistics.median(values),'min':min(values),'max':max(values)}

def audit(summary:dict,community:dict,initial_cash:float=120.) -> dict:
    if not community.get('enabled'): raise ValueError('Community profile is not enabled')
    actors=community['actors'];duration=float(community['ms']);errors=[]
    if duration<=0 or not actors: raise ValueError('Nonempty simulation and positive duration required')
    if len(actors)!=summary['population']:errors.append('population_mismatch')
    all_time=sum(sum(a['time_ms'].values()) for a in actors);person_days=all_time/86400000
    if person_days<=0: raise ValueError('No accounted person-time')
    cash_residual=[];time_residual=[]
    for a in actors:
        if any(not math.isfinite(float(x)) or x<0 for x in a['time_ms'].values()):errors.append(f'bad_time:{a["id"]}')
        tm=sum(a['time_ms'].values());delta=tm-duration;time_residual.append(delta)
        if a['alive'] and abs(delta)>1001:errors.append(f'time_coverage:{a["id"]}')
        if tm>duration+1001:errors.append(f'time_overlap:{a["id"]}')
        delta=initial_cash+a['earned']-sum(a[k] for k in ('food_spent','leisure_spent','study_spent','rent_paid'))-a['money']
        cash_residual.append(delta)
        if not math.isfinite(delta) or abs(delta)>1e-5:errors.append(f'cash_ledger:{a["id"]}')
        if a['money']<-1e-8:errors.append(f'negative_cash:{a["id"]}')
    by_time={k:distribution([a['time_ms'][k]/sum(a['time_ms'].values())*24 for a in actors if sum(a['time_ms'].values())>0]) for k in actors[0]['time_ms']}
    warnings=[]
    if summary['alive']!=summary['population']: warnings.append('deaths')
    if summary['critical_actor_seconds']>0: warnings.append('critical_needs_nonzero')
    sleep=by_time['sleep']['mean'];broad=by_time['social']['mean']+by_time['leisure']['mean']
    if not 7<=sleep<=10:warnings.append('sleep_outside_authored_7_10h')
    if not 3<=broad<=6:warnings.append('social_plus_leisure_outside_authored_3_6h')
    jobs=collections.Counter(a['job'] for a in actors)
    if duration<=7*86400000 and jobs[3]>0:warnings.append('accelerated_qualification_not_realistic_career_duration')
    received=sum(a['received'] for a in actors);forgotten=sum(a['forgotten'] for a in actors)
    return {'scenario':summary['scenario'],'seed':summary['seed'],'population':len(actors),'days':duration/86400000,'alive':summary['alive'],
      'person_days':person_days,'critical_actor_seconds':summary['critical_actor_seconds'],'max_damage':summary['max_damage'],
      'invariant_errors':errors,'pilot_warnings':warnings,'time_hours_per_person_day':by_time,
      'max_abs_time_residual_ms':max(map(abs,time_residual)),'max_abs_cash_residual':max(map(abs,cash_residual)),
      'jobs':dict(sorted(jobs.items())),'cash':distribution([a['money'] for a in actors]),'rent_debt':sum(a['rent_debt'] for a in actors),
      'promotions':sum(a['promotions'] for a in actors),'studies':sum(a['study_sessions'] for a in actors),
      'leisure_paid':sum(a['leisure_spent'] for a in actors),'food_paid':sum(a['food_spent'] for a in actors),
      'utterances':community['utterances'],'deliveries':community['deliveries'],'group_deliveries':community['group_deliveries'],
      'speech_minutes_per_person_day':community['speech_seconds']/60/person_days,
      'third_party_minutes_per_person_day':community['third_party_seconds']/60/person_days,'disclosures':community['disclosures'],
      'memory_count':distribution([a['memory_count'] for a in actors]),'received_total':received,'forgotten_total':forgotten,
      'location_erased':sum(m['location']==0 for a in actors for m in a['memories']),
      'time_erased':sum(m['occurred_at']==-1 for a in actors for m in a['memories']),
      'sources_erased':sum(m['cited_source']==0 for a in actors for m in a['memories']),
      'honesty_other_known':sum(o['honesty'] is not None and o['subject']!=a['id'] for a in actors for o in a['opinions']),
      'helpfulness_known':sum(o['helpfulness'] is not None for a in actors for o in a['opinions']),
      'hash':summary['hash']}

def main():
 p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 mp=a.directory/'manifest.json'
 if mp.exists():
  manifest=json.loads(mp.read_text());
  if not manifest['finished']:raise SystemExit('Refusing to label incomplete checkpoint series final')
  folder=a.directory/f'day-{len(manifest["runs"]):02d}'
 else:folder=a.directory
 s=json.loads((folder/'summary.json').read_text());c=json.loads((folder/'community.json').read_text());result=audit(s,c)
 if mp.exists():
  daily=[];previous={}
  for run in manifest['runs']:
   today=json.loads((a.directory/f'day-{run["day"]:02d}'/'community.json').read_text())
   for actor in today['actors']:
    work=actor['time_ms']['work']-previous.get(actor['id'],0);previous[actor['id']]=actor['time_ms']['work']
    if work>1000:daily.append(work/3600000)
  result['hours_on_days_with_any_work']=distribution(daily)
  result['total_wall_seconds']=sum(r['wall_seconds'] for r in manifest['runs'])
 a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({k:result[k] for k in ('seed','alive','days','invariant_errors','pilot_warnings')},ensure_ascii=False))
 if result['invariant_errors']:raise SystemExit(1)
if __name__=='__main__':main()
