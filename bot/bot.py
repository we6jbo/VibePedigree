#!/usr/bin/env python3
"""VibePedigree local read-only process watcher. Started/stopped by Qt.
Does not execute Vibe, inspect memory, modify game or transmit data.
"""
import os
import signal
import time
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path.home() / '.local/share/vibepedigree'
ROOT.mkdir(parents=True, exist_ok=True)
ROOT.chmod(0o700)
OUTPUT = ROOT / 'vibe_bot_info.md'
ACTIVE = True

def shutdown(_sig, _frame):
    global ACTIVE
    ACTIVE = False

signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT, shutdown)

def game_processes():
    found = []
    for node in Path('/proc').iterdir():
        if not node.name.isdigit() or int(node.name) == os.getpid():
            continue
        try:
            raw=(node/'cmdline').read_bytes().replace(b'\0',b' ').decode('utf-8','replace')
        except (OSError, PermissionError):
            continue
        # Only count the dedicated v23 executable; avoid unrelated games and flatpaks.
        if '/.Diablo/build-edward-v23-debug/devilutionx' in raw and not any(x in raw for x in ('/bin/bash -c', 'python3 -c')):
            found.append(node.name)
    return found

def write_record():
    pids=game_processes()
    timestamp=datetime.now(timezone.utc).isoformat(timespec='seconds')
    content=("# VibePedigree local bot observation\n\n"
             "This file is locally generated. It is not a Mistral Vibe analysis.\n"
             "No game memory, inventory, or character saves were inspected.\n\n"
             f"- Last checked (UTC): {timestamp}\n"
             f"- Custom Edward v23 Diablo active: {'yes' if pids else 'no'}\n"
             f"- Matching process count: {len(pids)}\n"
             "- Monitoring mode: process presence only\n"
             "- No external transmission\n")
    tmp=ROOT / f'.vibe_bot_info.{os.getpid()}.tmp'
    try:
        with open(tmp,'w',encoding='utf-8') as f:
            os.chmod(tmp,0o600)
            f.write(content)
        os.replace(tmp,OUTPUT)
    finally:
        try: tmp.unlink(missing_ok=True)
        except OSError: pass

while ACTIVE:
    try: write_record()
    except OSError as exc: print(f'Bot write warning: {exc}',flush=True)
    for _ in range(20):
        if not ACTIVE: break
        time.sleep(0.5)
