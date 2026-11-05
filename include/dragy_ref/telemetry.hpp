// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dragy_ref {
constexpr uint32_t gps_week_ms = 604800000;
struct Sample {
    uint32_t tow_ms{}, time_accuracy_ns{}, hacc_mm{}, vacc_mm{}, sacc_mm_s{};
    int32_t nano{}, lon_e7{}, lat_e7{}, height_mm{}, msl_mm{}, speed_mm_s{}, heading_e5{};
    uint16_t year{};
    uint8_t month{}, day{}, hour{}, minute{}, second{}, valid{}, fix{}, flags{}, satellites{};
    bool position_valid() const;
    bool utc_valid() const;
};
bool decode_nav_pvt(const uint8_t* frame, size_t size, Sample& out);
uint32_t crc32(const uint8_t* bytes, size_t size);
// Bounded stream parser. No allocation and no dependence on BLE fragment boundaries.
class Parser {
public:
    using Handler = void (*)(const Sample&, const uint8_t*, void*);
    void feed(const uint8_t*, size_t, Handler, void*);
    void reset() { used_=0; }
    uint32_t frames{}, checksum_errors{}, discarded{};
private:
    std::array<uint8_t, 520> buffer_{};
    size_t used_{};
    void drop(size_t);
};
enum class Epoch { First, Forward, Duplicate, Backward, Reset, Invalid };
class Timebase {
public:
    Epoch accept(uint32_t tow);
    void reset() { initial_=false; elapsed_ms=0; delta_ms=0; }
    uint64_t elapsed_ms{};
    uint32_t delta_ms{};
private:
    uint32_t previous_{};
    bool initial_{};
};
struct Statistics {
    double distance_m{}, moving_s{}, peak_kmh{};
    void accept(const Sample&, Epoch, uint32_t dt);
    void break_continuity() { previous_valid_=false; }
    void reset() { *this = Statistics{}; }
private:
    double previous_mps_{};
    bool previous_valid_{};
};
}
