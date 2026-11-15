// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/operation.hpp"
#include <cassert>
#include <thread>
#include <cstdio>
using namespace dragy_ref::ble;
int main(){
    OperationResult box;OperationResult::View out,value;value.token=1;value.ok=true;value.receiver=Receiver::Legacy;value.size=48;value.total=99;value.bytes.fill(0xa5);
    assert(!box.read(1,out));box.publish(value);assert(box.read(1,out));assert(out.total==99&&out.bytes[47]==0xa5&&out.receiver==Receiver::Legacy);
    // A callback already past its token check can finish after cancellation.
    // It must not become the result of the new operation or race its reader.
    value.token=1;box.publish(value);assert(!box.read(2,out));
    value.token=2;value.receiver=Receiver::Pro;box.publish(value);assert(box.read(2,out)&&out.receiver==Receiver::Pro);
    std::atomic<uint32_t> advertised{0};std::atomic<bool> done{false};
    std::thread writer([&]{for(uint32_t i=3;i<50000;++i){
        OperationResult::View v;v.token=i;v.size=48;v.total=i+48;v.ok=i%2;v.receiver=Receiver::Pro;v.bytes.fill(uint8_t(i));
        box.publish(v);advertised=i;
    }done=true;});
    while(!done){auto token=advertised.load();if(box.read(token,out)){
        assert(out.token==token&&out.size==48&&out.total==token+48&&out.ok==bool(token%2));
        for(auto byte:out.bytes)assert(byte==uint8_t(token));
    }}
    writer.join();assert(box.read(49999,out)&&out.total==50047);
    puts("PASS token-bound identity results, already-entered stale callbacks and concurrent coherent payload publication");
}
