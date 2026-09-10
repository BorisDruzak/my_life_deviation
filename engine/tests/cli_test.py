"""End-to-end CLI checks. Python is test tooling only, not a runtime dependency."""
from pathlib import Path
import json, subprocess, sys, tempfile
exe=Path(sys.argv[1]).resolve(); root=Path(sys.argv[2]).resolve()
def call(*args, success=True):
    result=subprocess.run([str(exe),*map(str,args)],cwd=root,text=True,capture_output=True,timeout=120)
    if success and result.returncode: raise AssertionError(result.stderr)
    if not success and not result.returncode: raise AssertionError('invalid invocation accepted')
    return result
with tempfile.TemporaryDirectory() as td:
    td=Path(td); scenario=root/'examples'/'protocol.json'
    call('validate','--scenario',scenario)
    call('run','--scenario',scenario,'--minutes','25','--out',td/'full')
    call('run','--scenario',scenario,'--minutes','6','--out',td/'first')
    call('resume','--snapshot',td/'first/state.json','--scenario',scenario,'--minutes','19','--out',td/'second')
    a=json.loads((td/'full/state.json').read_text()); b=json.loads((td/'second/state.json').read_text())
    assert a==b,'CLI resume changed the deterministic result'
    call('run','--scenario',scenario,'--minutes','1','--out',td/'full',success=False)
    call('run','--scenario',scenario,'--minutes','-1','--out',td/'bad',success=False)
    result=call('view','--snapshot',td/'full/state.json','--actor','b')
    assert 'private_data' not in result.stdout
    hostile=td/'hostile.json'
    hostile.write_text(json.dumps({'world':{'places':{'room':{'id':'room'}},'actors':{'../../escaped':{'id':'../../escaped','account':'account','position':{'place':'room'}}},'accounts':{'account':10}}}))
    call('run','--scenario',hostile,'--minutes','0','--out',td/'safe',success=False)
    assert not (td/'escaped.json').exists(), 'actor ID escaped the views directory'
    call('frobnicate',success=False)
print('CLI: validate/run/resume/view and error checks passed')
