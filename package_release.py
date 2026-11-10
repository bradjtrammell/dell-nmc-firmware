#!/usr/bin/env python3
"""Archive exact v0.5.10 firmware and readable source, excluding proprietary inputs."""
from pathlib import Path
from hashlib import sha256
from zipfile import ZipFile, ZIP_DEFLATED
import json

ROOT=Path(__file__).resolve().parent
DEST=Path('/mnt/data/H910P_UI_v0.5.10_Engineering_Package.zip')
BIN=Path('/mnt/data/H910P_Custom_UI_v0.5.10.bin')
files=[]
for top in ('source','tools','docs','notification-relay'):
  for path in (ROOT/top).rglob('*'):
    if path.is_file() and '__pycache__' not in path.parts and path.suffix not in ('.pyc',): files.append((path.relative_to(ROOT).as_posix(),path))
for path in ROOT.glob('*'):
  if path.is_file() and path.name not in ('compact_changes.py','package_release.py','build-build060.log','SHA256SUMS.txt'):
    files.append((path.name,path))
for path in (ROOT/'build').glob('*'):
  if path.is_file() and (path.name.endswith(('.json','.log')) and not path.name.startswith('nav-test')):
    files.append(('build/'+path.name,path))
for path in (ROOT/'build/served-native-ui').rglob('*'):
  if path.is_file():files.append((path.relative_to(ROOT).as_posix(),path))
files=[('H910P_Custom_UI_v0.5.10.bin',BIN)]+files
files.sort(key=lambda p:p[0])
checks=''.join(f'{sha256(path.read_bytes()).hexdigest()}  {name}\n' for name,path in files)
(ROOT/'SHA256SUMS.txt').write_text(checks)
with ZipFile(DEST,'w',compression=ZIP_DEFLATED,compresslevel=9) as z:
  for name,path in files:z.write(path,name)
  z.writestr('SHA256SUMS.txt',checks)
with ZipFile(DEST) as z:
  for name,path in files:
    if z.read(name)!=path.read_bytes():raise AssertionError('ZIP mismatch: '+name)
  if z.read('SHA256SUMS.txt').decode()!=checks:raise AssertionError('checksum manifest mismatch')
report=json.loads((ROOT/'build/VALIDATION.json').read_text())
assert BIN.stat().st_size==report['custom_bytes'] and sha256(BIN.read_bytes()).hexdigest()==report['custom_sha256']
assert report['firmware_within_factory_image_size'] and report['embedded_ui_round_trip_exact']
print(json.dumps({'bin_path':str(BIN),'bin_bytes':BIN.stat().st_size,'bin_sha256':sha256(BIN.read_bytes()).hexdigest(),'factory_bytes_remaining':1517029-BIN.stat().st_size,'zip_path':str(DEST),'zip_bytes':DEST.stat().st_size,'zip_sha256':sha256(DEST.read_bytes()).hexdigest(),'files':len(files)+1},indent=2))
