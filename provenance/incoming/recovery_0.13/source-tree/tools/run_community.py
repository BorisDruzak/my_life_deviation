#!/usr/bin/env python3
"""Reproducible daily checkpoints. No background jobs; every command is awaited."""
from __future__ import annotations
import argparse, hashlib, json, subprocess, time
from pathlib import Path

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--seed',type=int,default=42);p.add_argument('--population',type=int,default=16);p.add_argument('--days',type=int,default=7)
    p.add_argument('--steps',type=int,default=366);p.add_argument('--scenario',choices=['normal','scarcity','closed-road'],default='normal');p.add_argument('--timeout',type=int,default=120)
    a=p.parse_args()
    if not 1<=a.days<=366 or not 1<=a.steps<=366: p.error('days/steps outside range')
    a.binary=a.binary.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    mp=a.out/'manifest.json';signature={'binary_sha256':digest(a.binary),'seed':a.seed,'population':a.population,'days':a.days,'scenario':a.scenario}
    report=json.loads(mp.read_text()) if mp.exists() else {'signature':signature,'runs':[],'finished':False}
    if report['signature']!=signature:raise SystemExit('Refusing to combine checkpoints from different binaries/settings; use a new directory.')
    start=len(report['runs'])
    for day in range(start+1,min(a.days,start+a.steps)+1):
        dest=a.out/f'day-{day:02d}';dest.mkdir(exist_ok=True)
        cmd=[str(a.binary)]
        if day==1:cmd+=['--community','--seed',str(a.seed),'--population',str(a.population),'--scenario',a.scenario]
        else:cmd+=['--load',str(a.out/f'day-{day-1:02d}'/'world.save')]
        cmd+=['--days','1','--save',str(dest/'world.save'),'--summary',str(dest/'summary.json'),'--community-state',str(dest/'community.json'),'--actors',str(dest/'actors.csv')]
        begun=time.perf_counter()
        with (dest/'command.log').open('w') as out:
            proc=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,timeout=a.timeout,check=False)
        entry={'day':day,'command':cmd,'exit_code':proc.returncode,'wall_seconds':time.perf_counter()-begun}
        if proc.returncode: (dest/'run.json').write_text(json.dumps(entry,ensure_ascii=False,indent=2));raise SystemExit(f'day {day} failed: inspect command.log')
        s=json.loads((dest/'summary.json').read_text());entry.update({'alive':s['alive'],'critical_actor_seconds':s['critical_actor_seconds'],'state_hash':s['hash']})
        (dest/'run.json').write_text(json.dumps(entry,ensure_ascii=False,indent=2));report['runs'].append(entry);report['finished']=day==a.days
        mp.write_text(json.dumps(report,ensure_ascii=False,indent=2));print(f'day={day} alive={s["alive"]} critical={s["critical_actor_seconds"]} wall={entry["wall_seconds"]:.3f}',flush=True)
    if report['finished']:print('FINISHED',report['runs'][-1]['state_hash'],flush=True)
if __name__=='__main__':main()
