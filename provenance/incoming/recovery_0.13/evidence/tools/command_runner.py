import json,subprocess,sys,time,pathlib,hashlib
label=sys.argv[1]; directory=pathlib.Path('/mnt/data/recovery13_evidence')/label; directory.mkdir(parents=True,exist_ok=True)
commands=json.loads(sys.argv[2]); records=[]
for i,c in enumerate(commands):
 t=time.time()
 binary=pathlib.Path(c[0]);digest=hashlib.sha256(binary.read_bytes()).hexdigest() if binary.is_file() else None
 with (directory/f'{i}.log').open('w') as f:
  r=subprocess.run(c,stdout=f,stderr=subprocess.STDOUT,cwd='/mnt/data/recovery13')
 records.append({'command':c,'binary_sha256':digest,'returncode':r.returncode,'start_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime(t)),'seconds':time.time()-t,'log':f'{i}.log'})
 (directory/'execution.json').write_text(json.dumps(records,indent=2))
 if r.returncode:break
(directory/'done').write_text(str(records[-1]['returncode']))
