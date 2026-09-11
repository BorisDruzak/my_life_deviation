"""Copy the installed Boost.JSON header closure; no download is made at build time."""
from pathlib import Path
import re, shutil
src=Path('/usr/include'); dest=Path(__file__).resolve().parents[1]/'third_party'
queue=['boost/json.hpp','boost/json/src.hpp','boost/version.hpp']
# Conditional platform configuration and macro-selected headers.
for d in ('config','predef','json','mp11','describe'):
 queue += [str(p.relative_to(src)) for p in (src/'boost'/d).rglob('*') if p.is_file()]
seen=set()
while queue:
 rel=queue.pop()
 if rel in seen: continue
 p=src/rel
 if not p.is_file(): continue
 seen.add(rel); q=dest/rel; q.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(p,q)
 text=p.read_text(errors='replace')
 for inc in re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]',text):
  if inc.startswith('boost/'): queue.append(inc)
  elif (p.parent/inc).is_file(): queue.append(str((p.parent/inc).resolve().relative_to(src)))
print(f'Vendored {len(seen)} headers, {sum((dest/x).stat().st_size for x in seen)} bytes')
