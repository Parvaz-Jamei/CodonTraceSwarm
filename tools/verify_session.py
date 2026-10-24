"""Actual host -> C++ synthetic sample callback -> actual host reading/reply.
No broker, authentication, physical board or measured sensor is represented.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import types
p=argparse.ArgumentParser()
p.add_argument('host',type=Path)
p.add_argument('probe',type=Path)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args()
import os
# CTS_HOST_REV: Genesis revision under test (default: the pinned contract host).
pin=os.environ.get('CTS_HOST_REV','3e6686d72834221f3720fe54314aa84d2ea0ec05')
assert subprocess.check_output(['git','-C',str(a.host),'rev-parse','HEAD'],text=True).strip()==pin
for name,suffix in [('codontrace',''),('codontrace.swarm','swarm'),('codontrace.genesis','genesis')]:
 m=types.ModuleType(name);m.__path__=[str((a.host/'src/codontrace'/suffix).resolve())];sys.modules[name]=m
from codontrace.swarm.runtime import SwarmCoordinator
from codontrace.swarm.protocol import canonical, decode
hello=decode(subprocess.check_output([str(a.probe.resolve()),'--hello'],text=True,timeout=10))
assert hello['payload']['operations']==['sample']
host=SwarmCoordinator('lab');now=1800000000000
assert host.ingest(hello,now_ms=now)['accepted']
commands=[host.command('node','sample',{'channel':'s'},now_ms=now) for _ in range(4)]
wire='\n'.join([canonical(commands[0]),'@fail',canonical(commands[1]),'@success','@expire',canonical(commands[2]),canonical(commands[3])])+'\n'
run=subprocess.run([str(a.probe.resolve())],input=wire,text=True,capture_output=True,timeout=10,check=True)
lines=[json.loads(x) for x in run.stdout.splitlines()]
assert lines[0]==hello
rows=lines[1:];assert len(rows)==4
assert [r['calls'] for r in rows]==[1,2,2,3]
assert all(r['retry_identical'] and r['duplicate_suppressed'] for r in rows)
assert [r['reply']['payload']['ok'] for r in rows]==[True,False,False,True]
for i,row in enumerate(rows):
 reply=decode(row['reply'])
 assert reply['payload']['command_id']==commands[i]['command_id']
 if row['reading']:
  reading=decode(row['reading'])
  assert reading['payload']['value']==42.5 and reading['payload']['valid'] is True
  assert host.ingest(reading,now_ms=now)['accepted']
 if i<3:
  assert host.ingest(reply,now_ms=now)['accepted']
  assert host.ingest(reply,now_ms=now)['accepted'] is False # identical wire retry
 else:
  try:
   host.ingest(reply,now_ms=now+5000)
  except ValueError as error:
   assert 'non-pending' in str(error)
  else:
   raise AssertionError('Expired host command accepted a late positive reply')
state=host.snapshot(now_ms=now+5000)['state']
statuses=[state['commands'][c['command_id']]['status'] for c in commands]
assert statuses==['acknowledged','rejected','rejected','expired'],statuses
assert state['frames']['node/s']['value']==42.5
# Explicit unsupported operation: real hello was not rewritten for this test.
try:
 host.command('node','raw',{'channel':'s'},now_ms=now+5000)
except ValueError as error:
 assert 'not advertised' in str(error)
else:
 raise AssertionError('Unadvertised operation accepted')
a.evidence.write_text(json.dumps({'host_commit':pin,'synthetic_sensor':True,'broker_tested':False,'hello':hello,'commands':commands,'results':rows,'host_statuses':statuses},indent=2)+'\n')
print('PASS: real-host round trip: success, callback failure, expiry without callback, duplicate retry, and late host rejection; synthetic sensor=42.5; no broker/hardware.')
