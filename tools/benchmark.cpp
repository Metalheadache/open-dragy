// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "synthetic.hpp"
#include "dragy_ref/ingress.hpp"
#include <chrono>
#include <iostream>
static void receive(const dragy_ref::Sample&,const uint8_t*,void* p){++*static_cast<uint64_t*>(p);}
int main() {
    constexpr unsigned samples=250000;uint64_t decoded=0;dragy_ref::Parser parser;
    auto b=synthetic::frame(100000);auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<samples;++i)parser.feed(b.data(),b.size(),receive,&decoded);
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"decoded_frames="<<decoded<<" seconds="<<seconds<<" frames_per_second="<<decoded/seconds
             <<" parser_bytes="<<sizeof(parser)<<" ingress_bytes="<<sizeof(dragy_ref::Ingress<>)
             <<" capacity_fragments=31 max_fragment_bytes=512\n";
    return decoded==samples?0:1;
}
