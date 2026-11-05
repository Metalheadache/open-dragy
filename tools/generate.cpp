// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "synthetic.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"Usage: reference_generate OUTPUT.csv\n";return 2;}
    std::ofstream out(argv[1]);if(!out)return 2;
    out<<"arrival_us,hex\n";
    for(uint32_t ms=0;ms<45000;ms+=40) {
        auto b=synthetic::frame(100000+ms,int32_t(std::llround(synthetic::speed(ms)*1000)));
        uint64_t arrival=10000000+uint64_t(ms)*1000+(ms/40%3)*2000;
        for(auto pair:{std::pair<size_t,size_t>{0,17},{17,41},{58,42}}) {
            out<<std::dec<<arrival<<','<<std::hex<<std::setfill('0');
            for(size_t j=pair.first;j<pair.first+pair.second;++j)out<<std::setw(2)<<unsigned(b[j]);
            out<<'\n';
        }
    }
    return out?0:2;
}
