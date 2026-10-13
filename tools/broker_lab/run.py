"""Live loopback TLS/ACL/QoS1 test with real C++ nodes and pinned Genesis.
Requires test-only requirements.txt, OpenSSL and a built cts_broker_node.
Ephemeral credentials are deleted, never written to output evidence.
"""
import argparse,contextlib,importlib.metadata,json,os,queue,secrets,selectors,socket,ssl,subprocess,sys,tempfile,threading,time,types
from pathlib import Path
import paho.mqtt.client as mqtt
from argon2 import PasswordHasher
PIN='74c7b3f15c2d093da03e392094814d63a4997c2c'
T=1800000000000

def line(proc,timeout=10):
    with selectors.DefaultSelector() as sel:
        sel.register(proc.stdout,selectors.EVENT_READ)
        if not sel.select(timeout):raise TimeoutError('process output deadline')
        result=proc.stdout.readline()
    if not result:raise RuntimeError('process exited before reply')
    return result

def stop(proc):
    if proc.poll() is None:
        proc.terminate()
        try:proc.wait(timeout=5)
        except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)

def openssl(*args):
    subprocess.run(['openssl',*map(str,args)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True,timeout=20)

class Client:
    def __init__(self,name,password,ca,port,credentials=True):
        self.messages=queue.Queue();self.subs=queue.Queue();self.connected=threading.Event();self.reason=None;self.disconnected=threading.Event()
        self.client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='cts-lab-'+name,protocol=mqtt.MQTTv311,reconnect_on_failure=False)
        if credentials:self.client.username_pw_set(name,password)
        ctx=ssl.create_default_context(cafile=str(ca));ctx.minimum_version=ssl.TLSVersion.TLSv1_2
        self.client.tls_set_context(ctx)
        self.client.on_connect=self.on_connect
        self.client.on_disconnect=lambda c,u,flags,reason,props:self.disconnected.set()
        self.client.on_message=lambda c,u,m:self.messages.put((m.topic,bytes(m.payload),bool(m.retain),m.qos))
        self.client.on_subscribe=lambda c,u,mid,reasons,props:self.subs.put((mid,reasons))
        self.client.connect('localhost',port,keepalive=10);self.client.loop_start()
    def on_connect(self,c,u,flags,reason,props):self.reason=reason;self.connected.set()
    def ready(self):
        if not self.connected.wait(5) or self.reason.is_failure:raise AssertionError('valid MQTT login failed')
    def subscribe(self,topic):
        rc,mid=self.client.subscribe(topic,qos=1);assert rc==mqtt.MQTT_ERR_SUCCESS
        got,reasons=self.subs.get(timeout=5);assert got==mid
        return not any(r.is_failure for r in reasons)
    def publish(self,topic,data,retain=False):
        info=self.client.publish(topic,data,qos=1,retain=retain)
        assert info.rc==mqtt.MQTT_ERR_SUCCESS
        info.wait_for_publish(timeout=5);assert info.is_published()
    def receive(self,timeout=5):return self.messages.get(timeout=timeout)
    def close(self):self.client.disconnect();self.client.loop_stop()

class Node:
    def __init__(self,binary,name):
        self.proc=subprocess.Popen([str(binary),name],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
        self.errors=[]
        self.stderr_thread=threading.Thread(target=lambda:self.errors.extend(self.proc.stderr),daemon=True);self.stderr_thread.start()
        try:self.hello=json.loads(line(self.proc))
        except BaseException:self.close();raise
    def call(self,text):
        self.proc.stdin.write(text+'\n');self.proc.stdin.flush();return json.loads(line(self.proc))
    def delivery(self,message):
        topic,payload,retained,qos=message
        assert qos==1
        return self.call(topic+'\t'+str(int(retained))+'\t'+payload.decode())
    def close(self):
        stop(self.proc);self.stderr_thread.join(timeout=2)
        assert not self.stderr_thread.is_alive(), "node diagnostics reader did not stop"
        assert not any("runtime error:" in x or "ERROR: AddressSanitizer" in x or "DEADLYSIGNAL" in x for x in self.errors), "C++ sanitizer diagnostic"

def main():
    p=argparse.ArgumentParser();p.add_argument('--host',type=Path,required=True);p.add_argument('--node',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.host=a.host.resolve();a.node=a.node.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
    assert importlib.metadata.version('amqtt')=='0.12.1'
    assert importlib.metadata.version('paho-mqtt')=='2.1.0'
    assert subprocess.check_output(['git','-C',str(a.host),'rev-parse','HEAD'],text=True).strip()==PIN
    for name,suffix in [('codontrace',''),('codontrace.swarm','swarm'),('codontrace.genesis','genesis')]:
        m=types.ModuleType(name);m.__path__=[str(a.host/'src/codontrace'/suffix)];sys.modules[name]=m
    from codontrace.swarm.runtime import SwarmCoordinator
    from codontrace.swarm.protocol import canonical
    results=[];transcript=[]
    def passed(name):results.append({'test':name,'status':'PASS'});print('PASS:',name,flush=True)
    with tempfile.TemporaryDirectory(prefix='cts-tls-',dir=a.output.parent) as temp,contextlib.ExitStack() as stack:
        work=Path(temp);ca=work/'ca.crt';key=work/'ca.key';cert=work/'server.crt';serverkey=work/'server.key'
        openssl('req','-x509','-newkey','rsa:2048','-nodes','-keyout',key,'-out',ca,'-days','1','-subj','/CN=CTS-EPHEMERAL-TEST-CA','-addext','basicConstraints=critical,CA:TRUE')
        openssl('req','-newkey','rsa:2048','-nodes','-keyout',serverkey,'-out',work/'server.csr','-subj','/CN=localhost')
        (work/'server.ext').write_text('subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\nkeyUsage=digitalSignature,keyEncipherment\n')
        openssl('x509','-req','-in',work/'server.csr','-CA',ca,'-CAkey',key,'-CAcreateserial','-out',cert,'-days','1','-extfile',work/'server.ext')
        passwords={n:secrets.token_urlsafe(24) for n in ['controller','nodeA','nodeB']}
        hasher=PasswordHasher();(work/'passwords').write_text(''.join(n+':'+hasher.hash(pw)+'\n' for n,pw in passwords.items()))
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        up=lambda n:f'genesis/swarm/lab/{n}/up'
        down=lambda n:f'genesis/swarm/lab/{n}/down'
        config={'listeners':{'default':{'type':'tcp','bind':f'127.0.0.1:{port}','ssl':True,'cafile':str(ca),'certfile':str(cert),'keyfile':str(serverkey)}},'plugins':{
          'amqtt.plugins.authentication.FileAuthPlugin':{'password_file':str(work/'passwords')},
          'amqtt.plugins.topic_checking.TopicAccessControlListPlugin':{
             'acl':{'controller':[up('nodeA'),up('nodeB')],'nodeA':[down('nodeA')],'nodeB':[down('nodeB')]},
             'publish_acl':{'controller':[down('nodeA'),down('nodeB')],'nodeA':[up('nodeA')],'nodeB':[up('nodeB')]}}}}
        cfg=work/'broker.json';cfg.write_text(json.dumps(config))
        log=stack.enter_context((a.output/'broker.log').open('w'))
        def start_broker():
            proc=subprocess.Popen([sys.executable,str(Path(__file__).with_name('broker.py')),str(cfg)],stdout=subprocess.PIPE,stderr=log,text=True,bufsize=1)
            stack.callback(stop,proc);assert line(proc).strip()=='READY';return proc
        broker=start_broker()
        for kind,ctx,hostname in [('untrusted_ca',ssl.create_default_context(),'localhost'),('wrong_hostname',ssl.create_default_context(cafile=str(ca)),'127.0.0.1')]:
            try:
                with socket.create_connection(('127.0.0.1',port),timeout=3) as raw:
                    with ctx.wrap_socket(raw,server_hostname=hostname):raise AssertionError('invalid TLS peer accepted')
            except ssl.SSLCertVerificationError:passed(kind+'_rejected')
        bad=Client('nodeA','intentionally-wrong',ca,port);stack.callback(bad.close)
        assert bad.disconnected.wait(5), 'broker did not explicitly close invalid login'
        assert not bad.connected.is_set() or bad.reason.is_failure
        bad.close();passed('wrong_password_rejected_with_disconnect')
        anonymous=Client('anonymous-test',None,ca,port,credentials=False);stack.callback(anonymous.close)
        assert anonymous.disconnected.wait(5), 'broker did not explicitly close anonymous login'
        assert not anonymous.connected.is_set() or anonymous.reason.is_failure
        anonymous.close();passed('anonymous_login_rejected_with_disconnect')
        def clients():
            group={}
            for n in passwords:
                c=Client(n,passwords[n],ca,port);stack.callback(c.close);c.ready();group[n]=c
            for n in ['nodeA','nodeB']:
                assert group[n].subscribe(down(n));assert group['controller'].subscribe(up(n))
            return group
        peers=clients();tls_versions=sorted({c.client.socket().version() for c in peers.values()});assert all(v in ('TLSv1.2','TLSv1.3') for v in tls_versions)
        passed('two_nodes_verified_tls_qos1_subscriptions')
        assert not peers['nodeA'].subscribe(down('nodeB'));passed('cross_node_subscription_denied_by_broker')
        # Unauthorized publishing must not reach controller; use an allowed marker
        # from the SAME connection to establish processing order before checking.
        peers['nodeA'].publish(up('nodeB'),'FORBIDDEN')
        peers['nodeA'].publish(up('nodeA'),'ORDER_MARKER')
        topic,payload,retain,qos=peers['controller'].receive()
        assert topic==up('nodeA') and payload==b'ORDER_MARKER'
        passed('cross_node_publish_denied_by_broker')
        nodes={}
        for n in ['nodeA','nodeB']:
            node=Node(a.node,n);stack.callback(node.close);nodes[n]=node
        host=SwarmCoordinator('lab')
        def ingest_one(expected):
            topic,data,retained,qos=peers['controller'].receive();assert topic==up(expected) and not retained and qos==1
            packet=json.loads(data);assert host.ingest(packet,now_ms=T)['accepted'];transcript.append(packet);return packet
        for n in nodes:
            peers[n].publish(up(n),canonical(nodes[n].hello));ingest_one(n)
        passed('cpp_hello_over_tls_into_actual_host')
        def execute(n,fail=False):
            if fail:assert nodes[n].call('@fail')['configured']
            cmd=host.command(n,'sample',{'channel':'s'},now_ms=T);peers['controller'].publish(down(n),canonical(cmd))
            result=nodes[n].delivery(peers[n].receive());assert result['reply']['payload']['ok'] is (not fail)
            if result['reading']:
                peers[n].publish(up(n),canonical(result['reading']));reading=ingest_one(n);assert reading['payload']['value']==42.5
            peers[n].publish(up(n),canonical(result['reply']));ingest_one(n)
            assert nodes[n].call('@published:'+str(result['reply']['seq']))['confirmed']
            peers['controller'].publish(down(n),canonical(cmd));duplicate=nodes[n].delivery(peers[n].receive())
            assert duplicate['duplicate'] and duplicate['calls']==result['calls']
            expected='rejected' if fail else 'acknowledged'
            assert host.snapshot(now_ms=T)['state']['commands'][cmd['command_id']]['status']==expected
            return cmd
        cmd=execute('nodeA');execute('nodeB',True);passed('two_cpp_nodes_host_success_failure_and_duplicate_suppression')
        # Stored retained messages carry retain=True on a new subscription.
        peers['nodeA'].close()
        peers['controller'].publish(down('nodeA'),canonical(cmd),retain=True)
        peers['nodeA']=Client('nodeA',passwords['nodeA'],ca,port);stack.callback(peers['nodeA'].close);peers['nodeA'].ready();assert peers['nodeA'].subscribe(down('nodeA'))
        stored=peers['nodeA'].receive();assert stored[2] is True
        result=nodes['nodeA'].delivery(stored);assert result['ingress_rejected'] and result['calls']==1
        passed('broker_retained_delivery_rejected_by_cpp_inbox')
        # Stop all clients then kill/restart the real broker, preserving C++ nodes.
        for c in peers.values():c.close()
        stop(broker);broker=start_broker();peers=clients()
        for n in nodes:
            hello=nodes[n].call('@hello');peers[n].publish(up(n),canonical(hello));ingest_one(n)
        execute('nodeA');passed('broker_restart_fresh_hello_same_boot_and_next_command')
        state=host.snapshot(now_ms=T)['state']
        for node in nodes.values():node.close()
        report={'cpp_diagnostics':{n:node.errors for n,node in nodes.items()},'broker':'amqtt','broker_version':'0.12.1','client_version':'2.1.0','host_commit':PIN,'loopback_only':True,'synthetic_sensor':True,'physical_board_tested':False,'mqtt_version':'3.1.1','negotiated_tls':tls_versions,'qos':1,'results':results,'transcript':transcript,'host_command_statuses':{k:v['status'] for k,v in state['commands'].items()}}
        (a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(f'PASS: {len(results)} live broker checks; C++ sensor synthetic; no board qualification.',flush=True)
if __name__=='__main__':main()
