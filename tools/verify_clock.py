"""Actual-host ingestion of C++ monitored-clock acquisition fixtures (synthetic)."""
import argparse,json,subprocess,sys,types
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('host',type=Path);p.add_argument('probe',type=Path);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
import os
# CTS_HOST_REV: Genesis revision under test (default: the pinned contract host).
pin=os.environ.get('CTS_HOST_REV','3e6686d72834221f3720fe54314aa84d2ea0ec05')
assert subprocess.check_output(['git','-C',str(a.host),'rev-parse','HEAD'],text=True).strip()==pin
for name,suffix in [('codontrace',''),('codontrace.swarm','swarm'),('codontrace.genesis','genesis')]:
 m=types.ModuleType(name);m.__path__=[str((a.host/'src/codontrace'/suffix).resolve())];sys.modules[name]=m
from codontrace.swarm.protocol import decode
from codontrace.swarm.runtime import SwarmCoordinator
result=subprocess.run([str(a.probe.resolve())],text=True,capture_output=True,check=True,timeout=10)
rows=[decode(x) for x in result.stdout.splitlines()];assert len(rows)==3
assert [r['seq'] for r in rows]==[0,1,2]
assert rows[1]['payload']['sample_ms']==1800000000000
assert rows[1]['payload']['value']==0 and rows[1]['payload']['valid'] is True
assert rows[2]['payload']['sample_ms']==1800000001100
assert rows[2]['payload']['value'] is None and rows[2]['payload']['valid'] is False
host=SwarmCoordinator('lab')
for row,when in zip(rows,[1800000000000,1800000000100,1800000001100]):
 assert host.ingest(row,now_ms=when)['accepted']
assert 'stale rejected=1' in result.stderr
a.evidence.write_text(json.dumps({'synthetic':True,'host_commit':pin,'messages':rows,'probe_log':result.stderr},indent=2)+'\n')
print('PASS: C++ monitored acquisition -> real host: 100ms queued zero retains acquisition UTC; stale input consumes no seq; fresh invalid/null accepted.')
