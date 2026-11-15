// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/rate.hpp"
#include <atomic>
namespace dragy_ref::ble {
// One callback-task writer, one connection-task reader. Cancellation changes
// the expected token, never the writer's payload. An already-entered stale
// callback can publish only its old token; all payload fields remain race-free.
class OperationResult {
public:
    struct View {
        uint32_t token{},size{},total{};bool ok{};Receiver receiver{Receiver::Unknown};
        std::array<uint8_t,48> bytes{};
    };
    void publish(const View& v){
        ++serial_;token_=v.token;size_=v.size;total_=v.total;ok_=v.ok;receiver_=unsigned(v.receiver);
        for(size_t i=0;i<v.bytes.size();++i)bytes_[i]=v.bytes[i];
        ++serial_;
    }
    bool read(uint32_t expected,View& out)const{
        const auto serial=serial_.load();if(!expected||(serial&1))return false;
        View v;v.token=token_;v.size=size_;v.total=total_;v.ok=ok_;v.receiver=Receiver(receiver_.load());
        for(size_t i=0;i<v.bytes.size();++i)v.bytes[i]=uint8_t(bytes_[i].load());
        if(serial!=serial_.load()||v.token!=expected)return false;
        out=v;return true;
    }
private:
    std::atomic<uint32_t> serial_{0},token_{0},size_{0},total_{0},ok_{0},receiver_{0};
    std::array<std::atomic<uint32_t>,48> bytes_{};
};
}
