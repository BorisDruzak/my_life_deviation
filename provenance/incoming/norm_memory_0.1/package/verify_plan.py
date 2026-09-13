"""Verify the planning package and its equation reference, not the C++ game."""
from pathlib import Path
import datetime, hashlib, json, re, subprocess, sys, time, zipfile

root=Path(__file__).resolve().parent
data=json.loads((root/'NORM_MEMORY_0_1_TEST_MATRIX.json').read_text())
assert data['case_count']==87
assert [x['id'] for x in data['cases']]==[f'NM-{i:03}' for i in range(1,88)]
assert all(x['status']=='planned_not_run_against_cpp' for x in data['cases'])
plan=(root/'NORM_MEMORY_0_1_IMPLEMENTATION_PLAN_RU.md').read_text()
stages=re.findall(r'^## (S\d+)\.',plan,re.M)
assert stages==[f'S{i}' for i in range(12)],stages
for f in root.glob('*.md'):
    assert sum(line.startswith('```') for line in f.read_text().splitlines())%2==0,f
for stage in range(1,10):
    ids=[x['id'] for x in data['cases'] if x['stage']==f'S{stage}']
    assert ids
    if stage!=1:
        assert f'{ids[0]}..{ids[-1]}' in plan,(stage,ids)
profile=json.loads((root/'NORM_MEMORY_0_1_PROFILE.json').read_text())
assert profile['status']=='proposed_not_loaded_by_0.13'
command=[sys.executable,'-m','unittest','-v','test_norm_math_reference']
start=time.monotonic()
proc=subprocess.run(command,cwd=root/'reference',capture_output=True,text=True,timeout=20)
log=proc.stdout+proc.stderr
(root/'evidence'/'REFERENCE_TESTS.log').write_text(log)
assert proc.returncode==0,log
assert 'Ran 36 tests' in log
# Confirm local source inspection made no modifications to the uploaded project.
archive=Path('/mnt/data/my_life_deviation_RECOVERY_0.13.0.zip')
source=Path('/mnt/data/norm_plan_source')
checked=0
if archive.is_file() and source.is_dir():
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if name.startswith('source/') and not name.endswith('/'):
                rel=name[len('source/'):]
                assert (source/rel).read_bytes()==z.read(name),rel
                checked+=1
result={
 'created_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'kind':'verification_of_plan_and_pure_formula_reference',
 'future_cpp_acceptance_cases':87,'implementation_stages':12,
 'math_reference_tests':36,'math_reference_returncode':proc.returncode,
 'command':command,'python':sys.version,'seconds':round(time.monotonic()-start,6),
 'cpp_tests_executed':False,'future_norm_worlds_executed':False,
 'inspected_source_files_unchanged':checked,
 'markdown_fences_balanced':True,'planned_test_ids_and_stage_ranges_match':True,
 'reference_scope':'selected equations, known-source revision and unique-consequence ledger; not all new runtime semantics',
 'package_status':'plan_not_patch'
}
(root/'evidence'/'PLAN_VERIFICATION.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
