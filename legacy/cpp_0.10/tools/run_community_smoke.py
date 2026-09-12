#!/usr/bin/env python3
"""Run a finite list of reproducible real CLI checks, never in the background."""
from pathlib import Path
import subprocess,json,time,hashlib
root=Path(__file__).resolve().parents[1];binary=root/'build/life_sim';out=root/'reports/community10/release/smoke';out.mkdir(parents=True,exist_ok=True)
sha=hashlib.sha256(binary.read_bytes()).hexdigest()
cases=[('indexed16hour',['--community','--seed','42','--population','16','--seconds','3600']),('reference16hour',['--community','--seed','42','--population','16','--seconds','3600','--reference']),('workers4hour',['--community','--seed','42','--population','16','--seconds','3600','--workers','4']),('scale128hour',['--community','--seed','42','--population','128','--seconds','3600']),('scarcity16day',['--community','--seed','42','--population','16','--days','1','--scenario','scarcity']),('closed8day',['--community','--seed','42','--population','8','--days','1','--scenario','closed-road'])]
for name,args in cases:
 d=out/name;d.mkdir(exist_ok=True);cmd=[str(binary),*args,'--summary',str(d/'summary.json'),'--community-state',str(d/'community.json'),'--actors',str(d/'actors.csv')];t=time.perf_counter()
 with (d/'command.log').open('w') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60,check=False)
 (d/'command.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'wall_seconds':time.perf_counter()-t,'binary_sha256':sha},indent=2))
 if p.returncode:raise SystemExit('Failed '+name)
 s=json.loads((d/'summary.json').read_text());print(name,s['alive'],s['critical_actor_seconds'],s['hash'],flush=True)
