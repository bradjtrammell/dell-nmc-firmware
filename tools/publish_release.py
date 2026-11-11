#!/usr/bin/env python3
"""Verify, then publish a standalone H910P GitHub BIN asset using authenticated gh CLI."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from release_preflight import verify

REPOSITORY='bradjtrammell/dell-nmc-firmware'

def plan(firmware:Path,expected_sha256:str,channel:str,notes:Path,hardware_approved:bool=False) -> dict:
    if channel not in ('main','dev'):
        raise ValueError('Channel must be main or dev')
    result=verify(firmware,expected_sha256)
    version=result['version'];beta='-beta.' in version
    if (channel=='dev')!=beta:
        raise ValueError('Beta version filenames must target dev; stable versions must target main')
    if channel=='main' and not hardware_approved:
        raise ValueError('Stable release requires explicit hardware approval')
    if not notes.is_file() or not notes.read_text(encoding='utf-8').strip():
        raise ValueError('Release notes must be a nonempty local file')
    return {**result,'channel':channel,'tag':'v'+version,'repository':REPOSITORY,
            'notes':str(notes.resolve()),'asset':str(firmware.resolve()),
            'prerelease':beta}

def publish(spec:dict) -> None:
    if not shutil.which('gh'):
        raise RuntimeError('GitHub CLI (gh) is required')
    subprocess.run(['gh','auth','status'],check=True)
    existing=subprocess.run(['gh','release','view',spec['tag'],'--repo',REPOSITORY],capture_output=True,text=True)
    if existing.returncode==0:
        raise RuntimeError('Release tag already exists: '+spec['tag'])
    cmd=['gh','release','create',spec['tag'],spec['asset'],'--repo',REPOSITORY,
         '--target',spec['channel'],'--title','Dell H910P '+spec['tag'],
         '--notes-file',spec['notes']]
    if spec['prerelease']:cmd.append('--prerelease')
    subprocess.run(cmd,check=True)
    subprocess.run(['gh','release','view',spec['tag'],'--repo',REPOSITORY,'--json','assets',
                    '--jq','.assets[].name'],check=True)

def main(argv=None) -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware',type=Path)
    parser.add_argument('--sha256',required=True)
    parser.add_argument('--channel',choices=['main','dev'],required=True)
    parser.add_argument('--notes',type=Path,required=True)
    parser.add_argument('--hardware-approved',action='store_true')
    parser.add_argument('--dry-run',action='store_true')
    a=parser.parse_args(argv)
    try:
        spec=plan(a.firmware,a.sha256,a.channel,a.notes,a.hardware_approved)
        if a.dry_run:print(json.dumps(spec,indent=2));return 0
        publish(spec)
        print('Verified GitHub Release:',spec['tag'])
        return 0
    except (ValueError,OSError,subprocess.CalledProcessError,RuntimeError) as exc:
        print('Release failed:',str(exc),file=sys.stderr)
        return 1
if __name__=='__main__':sys.exit(main())
