"""One-time import of user-approved source; no credentials leave GitHub Actions."""
from pathlib import Path, PurePosixPath
import base64
import hashlib
import io
import json
import lzma
import re
import shutil
import tarfile
import urllib.request

ROOT = Path.cwd().resolve()
STAGING = ROOT / '.import'
BOOST_URL = 'https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.bz2'
BOOST_SHA256 = '6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def destination(name):
    rel = PurePosixPath(name)
    if rel.is_absolute() or '..' in rel.parts or not rel.parts:
        raise ValueError('Unsafe path: ' + name)
    if rel.parts[0] in ('.git', '.import'):
        raise ValueError('Protected path: ' + name)
    dest = (ROOT / str(rel)).resolve()
    if not dest.is_relative_to(ROOT):
        raise ValueError('Escaping path: ' + name)
    return dest

manifest = json.loads((STAGING / 'READY.json').read_text())
assert manifest['format'] == 1 and manifest['compression'] == 'xz'
assert manifest['boost_url'] == BOOST_URL
assert manifest['boost_sha256'] == BOOST_SHA256
parts = []
seen = set()
for entry in manifest['parts']:
    name = entry['file']
    if not re.fullmatch(r'part-[0-9]{3}\.b64', name) or name in seen:
        raise ValueError('Invalid or duplicated part')
    seen.add(name)
    text = (STAGING / name).read_text(encoding='ascii').strip()
    if len(text) != entry['chars'] or digest(text.encode('ascii')) != entry['sha256']:
        raise ValueError('Part checksum mismatch: ' + name)
    parts.append(text)
encoded = ''.join(parts)
if len(encoded) > 8_000_000:
    raise ValueError('Payload too large')
archive = base64.b64decode(encoded, validate=True)
assert len(archive) == manifest['archive_bytes']
assert digest(archive) == manifest['archive_sha256']
# Bound decompression before parsing or executing any imported source.
decoder = lzma.LZMADecompressor()
raw = decoder.decompress(archive, max_length=30_000_001)
if len(raw) > 30_000_000 or not decoder.eof or decoder.unused_data:
    raise ValueError('Invalid or oversized source archive')
written = []
with tarfile.open(fileobj=io.BytesIO(raw), mode='r:') as tar:
    members = tar.getmembers()
    if len(members) > 3000 or sum(m.size for m in members) > 30_000_000:
        raise ValueError('Invalid source member count/size')
    for member in members:
        if not member.isfile():
            raise ValueError('Only regular source files are accepted')
        dest = destination(member.name)
        if member.name in written:
            raise ValueError('Duplicated source member')
        written.append(member.name)
        dest.parent.mkdir(parents=True, exist_ok=True)
        stream = tar.extractfile(member)
        if stream is None:
            raise ValueError('Missing archive content')
        dest.write_bytes(stream.read())
        dest.chmod(0o755 if member.mode & 0o111 else 0o644)

checks = {}
for line in (ROOT / 'SHA256SUMS').read_text().splitlines():
    sha, sep, name = line.partition('  ')
    if not sep or not re.fullmatch(r'[0-9a-f]{64}', sha) or name in checks:
        raise ValueError('Invalid checksum manifest')
    destination(name)
    checks[name] = sha
if len(checks) + 1 != manifest['total_files']:
    raise ValueError('File count mismatch')
needed = {name for name in checks if name.startswith('third_party/')}
expected_tar = {}
for name in needed:
    if name.startswith('third_party/boost/'):
        remote = 'boost_1_83_0/' + name[len('third_party/'):]
    elif name == 'third_party/BOOST_LICENSE_1_0.txt':
        remote = 'boost_1_83_0/LICENSE_1_0.txt'
    else:
        raise ValueError('Unexpected third-party component: ' + name)
    expected_tar[remote] = name
# Restore exact upstream headers instead of transmitting hundreds of unchanged
# third-party files. Every header is checked against the original local release.
boost_path = STAGING / 'boost.tar.bz2'
with urllib.request.urlopen(BOOST_URL, timeout=90) as source, boost_path.open('wb') as sink:
    total = 0
    while True:
        block = source.read(1024 * 1024)
        if not block:
            break
        total += len(block)
        if total > 200_000_000:
            raise ValueError('Unexpected Boost download size')
        sink.write(block)
if digest(boost_path.read_bytes()) != BOOST_SHA256:
    raise ValueError('Boost distribution checksum mismatch')
found = set()
with tarfile.open(boost_path, mode='r:bz2') as tar:
    for member in tar:
        name = expected_tar.get(member.name)
        if name is None:
            continue
        if not member.isfile():
            raise ValueError('Non-regular third-party source')
        stream = tar.extractfile(member)
        if stream is None:
            raise ValueError('Missing third-party source')
        content = stream.read()
        target = destination(name)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        target.chmod(0o644)
        found.add(name)
if found != needed:
    raise ValueError('Missing upstream headers: ' + repr(sorted(needed - found)))
errors = []
for name, sha in checks.items():
    target = destination(name)
    if not target.is_file() or digest(target.read_bytes()) != sha:
        errors.append(name)
if errors:
    raise ValueError('Source checksum mismatch: ' + repr(errors))
print('Verified', len(checks) + 1, 'project files; imported source payload', manifest['archive_sha256'])
boost_path.unlink()
