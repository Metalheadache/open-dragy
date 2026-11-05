// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include "dragy_ref/telemetry.hpp"
namespace dragy_ref::drag {
constexpr unsigned count=7;
enum class Kind:uint8_t { Acceleration, Distance, Braking };
enum class Phase:uint8_t { Waiting, Running, Complete, Invalid };
enum class Fault:uint8_t { None, Fix, Accuracy, Satellites, Gap, Clock, Stale, Interrupted, Aborted, Timeout, Dynamics, Latency };
struct Definition {const char* label;Kind kind;double start_mps,end_mps,metres;};
extern const std::array<Definition,count> definitions;
const char* fault_text(Fault);
const char* phase_text(Phase);
struct Point {
    double tow_ms{},lat_deg{},lon_deg{},msl_m{};
};
struct Result {
    Phase phase{Phase::Waiting};Fault fault{Fault::None};uint32_t sequence{};
    double elapsed_s{},distance_m{};
    Point start{},finish{};
    uint32_t samples{},max_hacc_mm{},max_sacc_mm_s{};
    uint8_t min_satellites{255};
};
struct Channel {Result result{};};
struct View {bool armed{};uint32_t revision{};std::array<Channel,count> channels{};};
// Experimental policy v2: zero rollout; GNSS 3D, >=6 SV, hAcc <=5 m,
// sAcc <=0.5 m/s; <=200 ms interval and no missed established cadence.
// Standstill <=0.3 m/s for 1 s. Launch crosses 0.5 m/s with linear zero
// extrapolation <=1 s. Confirm >=max(1.389 m/s, 3*sAcc) for 300 ms
// within 3 s, retaining that original onset and raw distance. Braking zero
// extrapolation confirmed still for 0.5 s. No display-filter input.
class Engine {
public:
    void arm();void disarm();void accept(const Sample&,uint64_t arrival_us);
    void poll(uint64_t now_us);void interrupt(Fault=Fault::Interrupted);
    const View& view()const{return view_;}
private:
    struct Runtime {double start_ms{},stop_ms{-1},stop_distance{},last_decel{};bool eligible{};Point stop_point{};};
    // Accumulate the shared standing-start prefix privately, without a sample
    // queue or publishing results/PBs before movement is confirmed.
    struct Launch {Result result{};double start_ms{},detected_ms{},initial_mps{},confirm_ms{-1};uint64_t confirm_arrival_us{};bool pending{};};
    View view_{};std::array<Runtime,count> runs_{};
    Launch launch_{};
    Timebase time_{};Sample previous_{};double previous_ms_{},still_since_{-1},last_still_ms_{},cadence_{};
    uint64_t arrival_{},host_anchor_us_{};double gps_anchor_ms_{};bool previous_good_{},stationary_{};
    void reset_continuity();void invalidate(Fault);void finish(unsigned,double,double,const Point&);
    void update_launch(const Sample&,uint64_t,double,double,double,double,double,bool);
};
}
