// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/drag.hpp"
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/rate.hpp"
#include "dragy_ref/timing.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
using Queue=dragy_ref::Ingress<>;
struct Context {dragy_ref::Decoder<Queue>* decoder{};bool strict25{};dragy_ref::drag::TimingGate25 qualified;dragy_ref::drag::Engine engine;dragy_ref::ble::CadenceVerifier rate;uint64_t frames{},last_arrival{};};
static void sample(const dragy_ref::Sample& s,uint64_t arrival,void* p) {
    auto& c=*static_cast<Context*>(p);++c.frames;c.last_arrival=arrival;c.rate.observe(s.tow_ms);if(c.strict25)c.qualified.accept(s,arrival,c.decoder->parser().checksum_errors+c.decoder->parser().length_errors);else c.engine.accept(s,arrival);
}
static void gap(void* p){auto& c=*static_cast<Context*>(p);c.engine.interrupt();c.qualified.interrupt();c.rate.reset();}
static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
int main(int argc,char** argv) {
    if(argc<2||argc>5){std::cerr<<"Usage: reference_replay INPUT.csv [--arm] [--expect-complete] [--require-25hz]\n";return 2;}
    bool arm=false,expect=false,strict=false;for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="--arm")arm=true;else if(a=="--expect-complete")expect=true;else if(a=="--require-25hz")strict=true;else return 2;}
    std::ifstream in(argv[1]);if(!in)return 2;
    Queue q;auto session=q.new_session();Context context;context.strict25=strict;if(arm){context.engine.arm();context.qualified.arm();}context.qualified.configure(false);
    dragy_ref::Decoder<Queue> decoder(q,sample,gap,&context);context.decoder=&decoder;
    std::string line;if(!std::getline(in,line)||line!="arrival_us,hex")return 2;
    uint64_t previous=0;bool first=true;
    while(std::getline(in,line)) {
        if(line.size()>1050)return 2;
        auto comma=line.find(',');if(comma==std::string::npos||comma==0)return 2;
        uint64_t arrival=0;
        for(size_t j=0;j<comma;++j){if(line[j]<'0'||line[j]>'9'||arrival>(UINT64_MAX-9)/10)return 2;arrival=arrival*10+unsigned(line[j]-'0');}
        if(!first&&arrival<previous)return 2;
        first=false;previous=arrival;
        auto n=line.size()-comma-1;if(!n||n%2||n>1024)return 2;
        std::array<uint8_t,512> b{};
        for(size_t j=0;j<n/2;++j){int a=hex(line[comma+1+j*2]),z=hex(line[comma+2+j*2]);if(a<0||z<0)return 2;b[j]=uint8_t(a*16+z);}
        context.engine.poll(arrival);context.qualified.poll(arrival);q.push(session,b.data(),n/2,arrival);decoder.drain();
    }
    if(!in.eof())return 2;
    bool complete=true;std::cout<<std::fixed<<std::setprecision(6);
    for(unsigned i=0;i<dragy_ref::drag::count;++i) {
        auto& r=(strict?context.qualified.view():context.engine.view()).channels[i].result;
        complete&=r.phase==dragy_ref::drag::Phase::Complete;
        std::cout<<"{\"test\":"<<i<<",\"phase\":\""<<dragy_ref::drag::phase_text(r.phase)<<"\",\"fault\":\""<<dragy_ref::drag::fault_text(r.fault)<<"\",\"elapsed_s\":"<<r.elapsed_s<<",\"distance_m\":"<<r.distance_m<<"}\n";
    }
    std::cerr<<"frames="<<context.frames<<" checksum_errors="<<decoder.parser().checksum_errors<<" length_errors="<<decoder.parser().length_errors<<" drops="<<q.drops()<<" cadence_25hz="<<context.rate.confirmed()<<'\n';
    return expect&&(!complete||q.drops()||decoder.parser().checksum_errors||decoder.parser().length_errors||context.frames==0)?1:0;
}
