"""Real-host command generation -> actual C++ decoder; no device execution claim."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import types
p=argparse.ArgumentParser()
p.add_argument('host',type=Path)
p.add_argument('messages',type=Path)
p.add_argument('probe',type=Path)
p.add_argument('--evidence',type=Path)
a=p.parse_args()
pin='74c7b3f15c2d093da03e392094814d63a4997c2c'
assert subprocess.check_output(['git','-C',str(a.host),'rev-parse','HEAD'],text=True).strip()==pin
for name,suffix in [('codontrace',''),('codontrace.swarm','swarm'),('codontrace.genesis','genesis')]:
 m=types.ModuleType(name);m.__path__=[str((a.host/'src/codontrace'/suffix).resolve())];sys.modules[name]=m
from codontrace.swarm.runtime import SwarmCoordinator
from codontrace.swarm.protocol import canonical
hello=json.loads(a.messages.read_text().splitlines()[0])
# Controller fixture only: the production codec STILL advertises no operations.
# This synthetic host-side node advertises them to exercise actual command generation.
hello['payload']['operations']=['sample','raw','features','result','set_rate','pipeline','capsule']
host=SwarmCoordinator('synthetic');now=1800000000000
assert host.ingest(hello,now_ms=now)['accepted']
commands=[]
for op in hello['payload']['operations']:
 params={'channel':'s'}
 if op=='set_rate':params['hz']=2.5
 if op in ('pipeline','capsule'):
  params['pipeline']={'alpha':.25,'clip':5,'event_threshold':2}
  params['source_digest']='a'*64
 commands.append(host.command('native',op,params,now_ms=now))
result=subprocess.run([str(a.probe.resolve())],input='\n'.join(canonical(c) for c in commands)+'\n',capture_output=True,text=True,timeout=10,check=True)
rows=[json.loads(s) for s in result.stdout.splitlines()]
assert len(rows)==7
for cmd,row in zip(commands,rows):
 assert row['error']=='ok',row
 assert row['id']==cmd['command_id'] and row['operation']==cmd['operation'] and row['channel']=='s'
 if cmd['operation']=='set_rate':assert row['hz']==2.5
 if cmd['operation'] in ('pipeline','capsule'):
  assert row['alpha']==.25 and row['clip']==5 and row['event_threshold']==2
  assert row['source_digest']=='a'*64
if a.evidence:
 a.evidence.write_text(json.dumps({'synthetic_controller_fixture':True,'host_commit':pin,'execution_tested':False,'commands':commands,'decoded':rows},indent=2)+'\n')
print('PASS: 7 real host-generated command shapes decoded by C++; parameters and identities match. DECODE ONLY, no dispatch or firmware capability claim.')
