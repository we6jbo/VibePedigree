#!/usr/bin/env python3
"""VibePedigree local-only MPQ guard. No network, no file uploads.
Cryptography dependency: python-cryptography (AES-GCM).
"""
import os, sys, json, hashlib, pathlib, tempfile, stat, datetime, subprocess

HOME = pathlib.Path.home()
ROOT = HOME/'.VibePedigree'
SOURCE = HOME/'.local/share/vibepedigree/diablo-extracted'
PROJECT = HOME/'Projects/VibePedigree'
KEY = ROOT/'secrets.key'
SEALED = ROOT/'mpq-integrity.json.aesgcm'
REPORT = ROOT/'secrets-autorun-report.txt'
WARNING = ROOT/'VIBE_READ_THIS_SECURITY_WARNING.txt'
EXPECTED = {
 './DIABDAT.MPQ':'011bc6518e6166206231080a4440b373',
 './DRTL_IX86_108_109.mpq':'cb775def9f53d023d738a2694d6aedf4',
 './dx/Patch_rt.mpq':'d2488b30310c1d293eaf068a29709e65',
 './hellfire/hellfire.mpq':'c996bd970df13ea7aa5e2417f8e78b9f',
 './hellfire/hfmonk.mpq':'5a6b8f1ef6d505d469c31aef6e48e89d',
 './hellfire/hfmusic.mpq':'5f79b271b4a291fc8968df7e8aa80d52',
 './hellfire/hfvoice.mpq':'6ae6ce3e89ece92c1e3e912a91d0b186',
 './Patch_rt.mpq':'d2488b30310c1d293eaf068a29709e65',
}
MARKERS = ('.mpq','.exe','.iso','.bin','.cue')

def atomic_write(path, data, mode=0o600):
    fd, tmp = tempfile.mkstemp(prefix='.vp-tmp-',dir=str(path.parent))
    try:
        with os.fdopen(fd,'wb') as f:
            f.write(data);f.flush();os.fsync(f.fileno())
        os.chmod(tmp,mode);os.replace(tmp,path)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)

def crypto():
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    return AESGCM

def key_read(create=False):
    if KEY.is_symlink(): raise ValueError('Refusing symlink at key location')
    if KEY.exists():
        if KEY.stat().st_mode & 0o077: raise ValueError('Key permissions are too open; expected 0600')
        raw=KEY.read_bytes()
        if len(raw)!=32: raise ValueError('Invalid key length')
        return raw
    if not create: raise ValueError('Key missing — restore from local backup; cannot decrypt baseline')
    fd=os.open(KEY,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'wb') as f: f.write(os.urandom(32))
    return KEY.read_bytes()

def manifest():
    entries={}
    for rel, expected in EXPECTED.items():
        path=SOURCE/rel[2:]
        if not path.is_file() or path.is_symlink(): raise ValueError('Missing or unsafe game asset: '+rel)
        m=hashlib.md5(); sh=hashlib.sha256()
        with open(path,'rb') as f:
            for chunk in iter(lambda:f.read(1024*1024),b''): m.update(chunk);sh.update(chunk)
        if m.hexdigest()!=expected: raise ValueError('Checksum mismatch before baseline: '+rel)
        entries[rel]={'md5':m.hexdigest(),'sha256':sh.hexdigest(),'size':path.stat().st_size}
    return entries

def seal(payload,key):
    nonce=os.urandom(12)
    return b'VPG1'+nonce+crypto()(key).encrypt(nonce,json.dumps(payload,sort_keys=True).encode(),b'vibepedigree-mpq-manifest')

def unseal(blob,key):
    if not blob.startswith(b'VPG1'): raise ValueError('Invalid encrypted manifest header')
    return json.loads(crypto()(key).decrypt(blob[4:16],blob[16:],b'vibepedigree-mpq-manifest'))

def make_baseline():
    if SEALED.exists(): raise ValueError('Baseline already exists; refusing silent replacement')
    entries=manifest()
    key=key_read(create=True)
    atomic_write(SEALED,seal({'assets':entries,'created':datetime.datetime.now().isoformat()},key))
    return 'Created encrypted AES-256-GCM SHA256+MD5 manifest for %d MPQ files' % len(entries)

def scan():
    alerts=[]; warnings=[]; info=[]
    if not SOURCE.is_dir(): alerts.append('Game source directory missing: '+str(SOURCE))
    if not SEALED.exists(): alerts.append('Encrypted baseline missing: run secrets_guard.py init after inspecting source')
    if KEY.exists() and KEY.stat().st_mode & 0o077: alerts.append('Encryption key permissions too broad')
    if SEALED.exists():
        try:
            baseline=unseal(SEALED.read_bytes(),key_read())['assets']
            now=manifest()
            if set(baseline)!=set(now): alerts.append('Game asset file set changed')
            for rel in sorted(set(now)&set(baseline)):
                if now[rel]!=baseline[rel]: alerts.append('INTEGRITY MISMATCH '+rel)
            info.append('Integrity baseline checked: %s MPQ files' % len(now))
        except Exception as e: alerts.append('Baseline/integrity failed: '+str(e))
    # A local GOG installer in Downloads is an expected WARNING; MPQ in staging is CRITICAL.
    # Includes known release and source staging, not a claim of whole-disk scanning.
    watched=[PROJECT,HOME/'.local/share/flatpak-builder',HOME/'snap',HOME/'snapcraft',HOME/'Downloads']
    for base in watched:
        if not base.exists(): continue
        count=0
        for dirpath,dirs,files in os.walk(base,followlinks=False):
            dirs[:]=[d for d in dirs if d not in ('.cache','.git','node_modules','build-vibepedigree','build')]
            for filename in files:
                candidate=pathlib.Path(dirpath)/filename
                lower=filename.lower()
                if lower.endswith('.mpq'):
                    alerts.append('CRITICAL: PROPRIETARY MPQ OUTSIDE AUTHORIZED SOURCE: '+str(candidate))
                elif lower.startswith('setup_diablo_') and lower.endswith('.exe'):
                    if base == HOME/'Downloads':
                        warnings.append('AUTHORIZED LOCAL GOG INSTALLER IN DOWNLOADS (not publishable): '+str(candidate))
                    else:
                        alerts.append('CRITICAL: GOG INSTALLER IN PUBLISHING/PROJECT AREA: '+str(candidate))
                else: continue
                count+=1
                if count>=30: break
            if count>=30: break
    # Files staged in the git index are especially dangerous regardless of gitignore.
    if (PROJECT/'.git').exists():
        try:
            p=subprocess.run(['git','-C',str(PROJECT),'ls-files','-z'],capture_output=True,timeout=8,check=True)
            for name in p.stdout.decode('utf-8','replace').split('\0'):
                if name and (name.lower().endswith(('.mpq','.exe')) or 'diablo-extracted/' in name.lower()):
                    alerts.append('PROHIBITED GIT TRACKED FILE: '+name)
        except Exception as e: alerts.append('Could not verify git tracked paths: '+str(e))
    info.append('Watched project, downloads, snap, and local Flatpak staging. This is NOT a complete upload history or whole-disk scan.')
    return alerts,warnings,info

def main():
    if not ROOT.exists(): ROOT.mkdir(mode=0o700,parents=True)
    os.chmod(ROOT,0o700)
    mode=sys.argv[1] if len(sys.argv)>1 else 'check'
    if mode not in ('init','check'): raise SystemExit('usage: secrets_guard.py init|check')
    alerts=[]; warnings=[]; infos=[]
    try:
        crypto()
        if mode=='init': infos.append(make_baseline())
        alerts, warnings, extra=scan();infos+=extra
    except Exception as e: alerts.append('Crypto/setup failure: '+str(e))
    lines=['VibePedigree Secrets Autorun', 'Time: '+datetime.datetime.now().isoformat(), 'Status: '+('CRITICAL' if alerts else ('WARNING' if warnings else 'SAFE'))]
    lines+=['INFO: '+v for v in infos]
    lines+=['WARNING: '+v for v in warnings]
    lines+=['CRITICAL: '+v for v in alerts]
    lines+=['RESPONSE: Stop publication; locate reported path(s), check git index/build staging, compare against local original; do not delete or publish automatically.']
    lines+=['LIMITATION: Cannot guarantee never uploaded; no remote account or service is contacted.']
    content='\n'.join(lines)+'\n'
    atomic_write(REPORT,content.encode())
    atomic_write(WARNING,('VIBE SECURITY RULE: Never copy, commit, upload, attach, publish, or embed Diablo/Hellfire MPQ/installer/save files or decryption keys. Asset root: '+str(SOURCE)+'\n'+('CRITICAL: Review '+str(REPORT) if alerts else ('WARNING: Review '+str(REPORT) if warnings else 'Local checks passed; do not interpret as proof of no exfiltration.'))+'\n').encode())
    print(content)
    return 2 if alerts else (1 if warnings else 0)

if __name__=='__main__': sys.exit(main())
