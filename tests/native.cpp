// SPDX-License-Identifier: MIT
// Synthetic protocol tests, not physical sensor measurements.
#include "CodonTraceSwarm.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
using namespace cts;
static int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";}}while(0)
static ChannelSpec channel(const char* name,ValueType type=ValueType::Scalar) {
 ChannelSpec c;c.name=name;c.quantity="synthetic_signal";c.unit="count";
 c.descriptor.type=type;c.descriptor.featureId="fixture";c.descriptor.configId="fixture_v1";c.descriptor.sourceUnit="count";
 if(type==ValueType::Vector){c.descriptor.axes[0]="x";c.descriptor.axes[1]="y";c.descriptor.axisCount=2;}
 if(type==ValueType::Waveform){c.descriptor.length=3;c.descriptor.samplePeriodUs=1000;}
 return c;
}
static SessionConfig session(const char* boot="boot_a") {SessionConfig s;s.swarmId="synthetic";s.nodeId="native";s.bootId=boot;return s;}
static const uint64_t utc=1800000000000ULL;
static char out[TxBytes];
static void lifecycle() {
 Device d;CHECK(d.hello(out,sizeof out)==Error::NotStarted);CHECK(d.begin(session())!=Error::Ok);
 auto c=channel("s");CHECK(d.define(c)==Error::Ok);CHECK(d.define(c)!=Error::Ok);
 auto bad=session();bad.wifi=false;CHECK(d.begin(bad)!=Error::Ok);bad=session();bad.nodeId="bad:id";CHECK(d.begin(bad)!=Error::Ok);
 CHECK(d.begin(session())==Error::Ok);CHECK(d.define(channel("late"))==Error::Frozen);
 CHECK(d.hello(out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\"wifi\":true")!=nullptr);CHECK(std::strstr(out,"\"operations\":[]")!=nullptr);
 CHECK(d.nextSequence()==1);CHECK(d.heartbeat(out,sizeof out)==Error::Ok);CHECK(d.nextSequence()==2);
 auto seq=d.nextSequence();char tiny[8];std::memset(tiny,'x',sizeof tiny);CHECK(d.hello(tiny,sizeof tiny)==Error::Capacity);CHECK(tiny[0]==0);CHECK(d.nextSequence()==seq);
 CHECK(d.hello(nullptr,0)!=Error::Ok);CHECK(d.nextSequence()==seq);
 d.end();CHECK(d.heartbeat(out,sizeof out)==Error::NotStarted);CHECK(d.begin(session())!=Error::Ok);CHECK(d.begin(session("boot_b"))==Error::Ok);CHECK(d.nextSequence()==0);
}
static void registry() {
 Device d;char name[]="owned",config[]="owned_cfg",unit[]="count";auto c=channel(name);c.descriptor.configId=config;c.descriptor.sourceUnit=unit;
 CHECK(d.define(c)==Error::Ok);name[0]='X';config[0]='X';unit[0]='X';CHECK(d.begin(session())==Error::Ok);CHECK(d.hello(out,sizeof out)==Error::Ok);
 CHECK(std::strstr(out,"owned")!=nullptr);CHECK(std::strstr(out,"owned_cfg")!=nullptr);CHECK(std::strstr(out,"Xwned")==nullptr);
 Device cap;char id[16];for(size_t i=0;i<MaxChannels;++i){std::snprintf(id,sizeof id,"c%u",unsigned(i));CHECK(cap.define(channel(id))==Error::Ok);}CHECK(cap.define(channel("extra"))==Error::Capacity);CHECK(cap.channelCount()==MaxChannels);
 Device bad;auto v=channel("bad",ValueType::Vector);v.descriptor.axisCount=0;CHECK(bad.define(v)!=Error::Ok);v=channel("bad",ValueType::Vector);v.descriptor.axes[1]="x";CHECK(bad.define(v)!=Error::Ok);
 v=channel("bad",ValueType::Waveform);v.descriptor.length=257;CHECK(bad.define(v)!=Error::Ok);v.descriptor.length=3;v.descriptor.samplePeriodUs=0;CHECK(bad.define(v)!=Error::Ok);
 v=channel("bad");v.descriptor.model=MeasurementModel::Derived;CHECK(bad.define(v)!=Error::Ok);v=channel("bad");v.unit="V";CHECK(bad.define(v)!=Error::Ok);
 v=channel("bad");v.descriptor.hasRange=true;v.descriptor.minimum=2;v.descriptor.maximum=1;CHECK(bad.define(v)!=Error::Ok);
 v=channel("bad");v.quantity="\xc0\xaf";CHECK(bad.define(v)!=Error::Ok);CHECK(bad.channelCount()==0);
}
static void samples(std::ostream* fixture) {
 Device d;CHECK(d.define(channel("s"))==Error::Ok);CHECK(d.define(channel("v",ValueType::Vector))==Error::Ok);CHECK(d.define(channel("w",ValueType::Waveform))==Error::Ok);CHECK(d.define(channel("b",ValueType::Boolean))==Error::Ok);CHECK(d.define(channel("c",ValueType::Category))==Error::Ok);
 CHECK(d.begin(session())==Error::Ok);CHECK(d.hello(out,sizeof out)==Error::Ok);if(fixture)*fixture<<out<<'\n';
 auto s=Sample::scalar(0,utc);CHECK(d.reading("s",s,out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\"value\":0")!=nullptr);CHECK(std::strstr(out,"\"valid\":true")!=nullptr);if(fixture)*fixture<<out<<'\n';
 s=Sample::invalid("sensor_timeout",utc);s.value=123;CHECK(d.reading("s",s,out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\"value\":null")!=nullptr);CHECK(std::strstr(out,"\"valid\":false")!=nullptr);if(fixture)*fixture<<out<<'\n';
 s=Sample::scalar(1,utc);s.count=2;s.values[0]=-1;s.values[1]=2.25;CHECK(d.reading("v",s,out,sizeof out)==Error::Ok);if(fixture)*fixture<<out<<'\n';s.count=1;CHECK(d.reading("v",s,out,sizeof out)==Error::InvalidSample);
 s.count=3;s.values[2]=0;CHECK(d.reading("w",s,out,sizeof out)==Error::Ok);if(fixture)*fixture<<out<<'\n';s.count=4;CHECK(d.reading("w",s,out,sizeof out)==Error::InvalidSample);
 s=Sample::scalar(0,utc);s.boolean=false;CHECK(d.reading("b",s,out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\"value\":false")!=nullptr);if(fixture)*fixture<<out<<'\n';
 s.category="SYNTHETIC \"quote\" \\ slash\nµ";CHECK(d.reading("c",s,out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\\\"quote\\\"")!=nullptr);CHECK(std::strstr(out,"\\n")!=nullptr || std::strstr(out,"\\u000a")!=nullptr);if(fixture)*fixture<<out<<'\n';
 s.category="\xed\xa0\x80";CHECK(d.reading("c",s,out,sizeof out)==Error::InvalidSample);
 s=Sample::scalar(std::numeric_limits<double>::quiet_NaN(),utc);CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);s.value=std::numeric_limits<double>::infinity();CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);
 s=Sample::scalar(1,utc);s.status="timeout";CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);s=Sample::invalid("ok",utc);CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);
 s=Sample::scalar(1,0);CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);s.utcMs=MaxWireInteger+1;CHECK(d.reading("s",s,out,sizeof out)==Error::InvalidSample);s.utcMs=MaxWireInteger;CHECK(d.reading("s",s,out,sizeof out)==Error::Ok);
 CHECK(d.reading("missing",s,out,sizeof out)!=Error::Ok);
}
static void normalization() {
 Device d;auto c=channel("affine");c.unit="V";auto& n=c.descriptor.normalization;n.method=Normalization::Method::Affine;n.scale=.5;n.offset=1;n.outputUnit="V";n.evidenceId="synthetic_calibration";c.descriptor.hasRange=true;c.descriptor.minimum=1;c.descriptor.maximum=3;
 CHECK(d.define(c)==Error::Ok);CHECK(d.begin(session())==Error::Ok);CHECK(d.hello(out,sizeof out)==Error::Ok);CHECK(d.reading("affine",Sample::scalar(4,utc),out,sizeof out)==Error::Ok);CHECK(std::strstr(out,"\"value\":4")!=nullptr);CHECK(d.reading("affine",Sample::scalar(4.1,utc),out,sizeof out)==Error::InvalidSample);
 Device t;Knot knots[]={Knot(0,0),Knot(10,100)};c=channel("table");c.unit="V";c.descriptor.normalization.method=Normalization::Method::Table;c.descriptor.normalization.outputUnit="V";c.descriptor.normalization.evidenceId="synthetic_table";c.descriptor.normalization.knots=knots;c.descriptor.normalization.knotCount=2;c.descriptor.hasRange=true;c.descriptor.minimum=0;c.descriptor.maximum=100;
 CHECK(t.define(c)==Error::Ok);knots[1].x=1;knots[1].y=1000;CHECK(t.begin(session())==Error::Ok);CHECK(t.hello(out,sizeof out)==Error::Ok);CHECK(t.reading("table",Sample::scalar(5,utc),out,sizeof out)==Error::Ok);CHECK(t.reading("table",Sample::scalar(11,utc),out,sizeof out)==Error::InvalidSample);CHECK(t.reading("table",Sample::scalar(-1,utc),out,sizeof out)==Error::InvalidSample);
 Device b;c=channel("bad");c.descriptor.normalization.method=Normalization::Method::Affine;c.descriptor.normalization.scale=0;CHECK(b.define(c)!=Error::Ok);
}
static void host_boundaries() {
 const std::string over96(97,'a'),over128(129,'a');
 Device d;auto c=channel("s");c.quantity=over96.c_str();CHECK(d.define(c)!=Error::Ok);
 c=channel("s");c.descriptor.modelId=over96.c_str();CHECK(d.define(c)!=Error::Ok);
 const char* whitespace[]={" ","\t\r\n","\xc2\xa0","\xe3\x80\x80"};
 for(auto p:whitespace){c=channel("s");c.quantity=p;CHECK(d.define(c)!=Error::Ok);c=channel("s");c.unit=p;c.descriptor.sourceUnit=p;CHECK(d.define(c)!=Error::Ok);}
 c=channel("s");c.descriptor.normalization.method=Normalization::Method::Affine;c.descriptor.normalization.outputUnit="count";c.descriptor.normalization.evidenceId="bad/id";CHECK(d.define(c)!=Error::Ok);c.descriptor.normalization.evidenceId="µ";CHECK(d.define(c)!=Error::Ok);
 CHECK(d.define(channel("c",ValueType::Category))==Error::Ok);auto cfg=session();cfg.processingVersion=over96.c_str();CHECK(d.begin(cfg)!=Error::Ok);CHECK(d.begin(session())==Error::Ok);CHECK(d.hello(out,sizeof out)==Error::Ok);
 auto sample=Sample::scalar(0,utc);sample.category=over128.c_str();CHECK(d.reading("c",sample,out,sizeof out)==Error::InvalidSample);
 for(auto p:whitespace){sample.category=p;CHECK(d.reading("c",sample,out,sizeof out)==Error::InvalidSample);sample=Sample::invalid(p,utc);CHECK(d.reading("c",sample,out,sizeof out)==Error::InvalidSample);sample=Sample::scalar(0,utc);}
}
int main(int argc,char**argv){std::ofstream fixture;if(argc>1){fixture.open(argv[1]);if(!fixture){std::cerr<<"Cannot open fixture output\n";return 2;}}lifecycle();registry();samples(argc>1?&fixture:nullptr);normalization();host_boundaries();std::cout<<"checks="<<checks<<" failures="<<failures<<" sizeof_device="<<sizeof(Device)<<"\n";return failures?1:0;}
