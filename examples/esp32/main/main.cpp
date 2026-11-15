// SPDX-License-Identifier: MPL-2.0 AND Apache-2.0
// Copyright 2026 Yizhan Wu (local contributions)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Retained upstream portions also carry Apache-2.0 terms; see NOTICE.
// Standalone adapter; only serial telemetry, optional timing and BLE recovery.
// Observed protocol credit: Jarel Remick / DragyDash ESP32 (Apache-2.0).
#include "NimBLEDevice.h"
#include "dragy_ref/ble_session.hpp"
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/receiver.hpp"
#include "dragy_ref/rate_setup.hpp"
#include "dragy_ref/operation.hpp"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gatt.h"
#include "os/os_mbuf.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <utility>
namespace {
using Queue=dragy_ref::Ingress<>;
using Receiver=dragy_ref::ble::Receiver;
Queue queue;
dragy_ref::BoundaryControl gate_control,reply_control;
std::atomic<uint32_t> notification_ms{0},sample_ms{0},operation_token{0};
std::atomic<bool> accepting{false},consumer_ready{false};
dragy_ref::ble::OperationResult operation_result;
NimBLEClient* client{};
uint64_t now_us(){return uint64_t(esp_timer_get_time());}
void pause(unsigned ms){vTaskDelay(pdMS_TO_TICKS(ms));}
struct SharedStatus {
    struct View {uint32_t gate{},clean{},delta{},ms{},request{};bool ready{};dragy_ref::ble::Reply reply{};};
    std::atomic<uint32_t> serial{0},gate{0},clean{0},delta{0},ms{0},request{0},reply{0},ready{0};
    void publish(View v){++serial;gate=v.gate;clean=v.clean;delta=v.delta;ms=v.ms;request=v.request;reply=unsigned(v.reply);ready=v.ready;++serial;}
    bool read(View& v)const{
        auto a=serial.load();if(a&1)return false;
        View next{gate.load(),clean.load(),delta.load(),ms.load(),request.load(),bool(ready.load()),dragy_ref::ble::Reply(reply.load())};
        if(a!=serial.load())return false;
        v=next;return true;
    }
} shared;
// The SDK invokes this listener before the characteristic callback and frees
// the borrowed chain after it. Compare at most the first 128 notifications.
std::atomic<uint16_t> stream_handle{0};
std::atomic<uint32_t> audited{0},compared{0},chained{0},mismatches{0},copy_failures{0},notify_count{0};
const os_mbuf* audit_packet{};uint32_t audit_length{},audit_callback{};
int gap_observer(ble_gap_event* event,void*){
    if(event->type==BLE_GAP_EVENT_NOTIFY_RX&&accepting&&event->notify_rx.attr_handle==stream_handle.load()&&audited<128){
        auto* packet=event->notify_rx.om;auto n=unsigned(OS_MBUF_PKTLEN(packet));++audited;
        if(unsigned(packet->om_len)<n)++chained;
        if(n<=512){audit_packet=packet;audit_length=n;audit_callback=notify_count.load()+1;}
        else {++copy_failures;audit_packet=nullptr;}
    }
    return 0;
}
void audit_notification(const uint8_t* p,size_t n){
    auto serial=++notify_count;auto* packet=audit_packet;audit_packet=nullptr;
    if(!packet||serial!=audit_callback)return;
    ++compared;bool mismatch=n!=audit_length;uint8_t scratch[32];
    for(size_t offset=0;offset<n&&offset<512;){auto count=std::min(sizeof(scratch),n-offset);
        if(os_mbuf_copydata(packet,int(offset),int(count),scratch)){++copy_failures;break;}
        if(memcmp(scratch,p+offset,count))mismatch=true;
        offset+=count;
    }
    if(mismatch)++mismatches;
}
void serial_sample(const dragy_ref::Sample& s,uint64_t arrival,void*){
    static uint32_t printed_ms=0;const auto ms=uint32_t(arrival/1000);sample_ms=ms;
    // Presentation is throttled; receiver parsing/timing remain full-rate.
    if(uint32_t(ms-printed_ms)<200)return;
    printed_ms=ms;
    printf("GPS tow_ms=%lu lat=%.7f lon=%.7f raw_kmh=%.3f fix=%u sv=%u hacc_mm=%lu sacc_mm_s=%lu\n",
           (unsigned long)s.tow_ms,s.lat_e7/1e7,s.lon_e7/1e7,s.speed_mm_s*.0036,s.fix,s.satellites,(unsigned long)s.hacc_mm,(unsigned long)s.sacc_mm_s);
}
void decode_task(void*){
    dragy_ref::ReceiverDecoder<Queue> consumer(queue,gate_control,reply_control,serial_sample);uint32_t revision=0;
#ifdef CONFIG_REFERENCE_ARM_TESTS
    consumer.timing.arm();
#endif
    // Subscription cannot start until this consumer can actually drain.
    consumer_ready=true;uint32_t printed=0;
    for(;;){
        auto now=now_us();consumer.poll(now);
        shared.publish({consumer.gate_revision(),consumer.timing.clean_intervals(),consumer.last_delta(),uint32_t(consumer.last_sample_us()/1000),
                        consumer.request(),consumer.timing.ready(now),consumer.reply()});
        if(revision!=consumer.timing.view().revision){
            revision=consumer.timing.view().revision;
            for(unsigned i=0;i<dragy_ref::drag::count;++i){auto& r=consumer.timing.view().channels[i].result;
                if(r.sequence)printf("TEST id=%u phase=%s fault=%s elapsed_s=%.6f distance_m=%.3f\n",i,dragy_ref::drag::phase_text(r.phase),dragy_ref::drag::fault_text(r.fault),r.elapsed_s,r.distance_m);
            }
        }
        if(uint32_t(now/1000)-printed>=2000){
            printed=uint32_t(now/1000);const auto& p=consumer.parser();const auto& q=consumer.quality;
            printf("PROTOCOL pvt=%lu other=%lu crc=%lu length=%lu drops=%lu ack=%lu nak=%lu malformed_ack=%lu\n",(unsigned long)p.frames,(unsigned long)p.other_frames,(unsigned long)p.checksum_errors,(unsigned long)p.length_errors,(unsigned long)queue.drops(),(unsigned long)consumer.replies.acks,(unsigned long)consumer.replies.naks,(unsigned long)consumer.replies.malformed);
            printf("QUALITY fix=%lu position=%lu speed=%lu hacc=%lu utc=%lu sacc=%lu; overlapping reasons\n",(unsigned long)q.fix,(unsigned long)q.position,(unsigned long)q.speed,(unsigned long)q.accuracy,(unsigned long)q.utc,(unsigned long)q.speed_accuracy);
            printf("STARTUP clean40=%u ready=%d gap_pending=%d bridged=%lu rejected=%lu\n",consumer.timing.clean_intervals(),consumer.timing.ready(now),consumer.timing.awaiting_confirmation(),(unsigned long)consumer.timing.bridged,(unsigned long)consumer.timing.rejected);
            printf("INGRESS audited=%lu compared=%lu chained=%lu mismatches=%lu copy_failures=%lu\n",(unsigned long)audited.load(),(unsigned long)compared.load(),(unsigned long)chained.load(),(unsigned long)mismatches.load(),(unsigned long)copy_failures.load());
        }
        pause(5);
    }
}
class Events final:public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*,int) override {accepting=false;queue.new_session();++operation_token;}
} events;
struct Adapter {
    uint32_t session{};
    NimBLERemoteCharacteristic *challenge{},*stream{},*rate{},*status{};
    NimBLERemoteDescriptor* cccd{};
    void stage(dragy_ref::ble::Stage s){printf("BLE %s\n",dragy_ref::ble::name(s));}
    bool fail(const char* why){printf("BLE setup: %s\n",why);return false;}
    bool discover() {
        challenge=stream=rate=status=nullptr;cccd=nullptr;
        auto* service=client->getService(NimBLEUUID(uint16_t(0xfd00)));if(!service)return false;
        uint16_t previous=0;
        for(auto* c:service->getCharacteristics(true)) {
            if(c->getHandle()<=previous)return false;
            previous=c->getHandle();
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd01)))rate=c;
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd02)))stream=c;
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd03)))challenge=c;
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd04)))status=c;
        }
        if(!challenge||!stream||!challenge->canRead()||(!challenge->canWrite()&&!challenge->canWriteNoResponse())||!stream->canNotify())return false;
        for(const auto& item:{std::pair<const char*,NimBLERemoteCharacteristic*>{"FD01",rate},{"FD02",stream},{"FD03",challenge},{"FD04",status}}){
            auto* c=item.second;printf("GATT %s present=%d handle=%u read=%d write=%d write_nr=%d notify=%d\n",item.first,c!=nullptr,c?c->getHandle():0,c&&c->canRead(),c&&c->canWrite(),c&&c->canWriteNoResponse(),c&&c->canNotify());
        }
        cccd=stream->getDescriptor(NimBLEUUID(uint16_t(0x2902)));return client->isConnected();
    }
    bool has_cccd()const{return cccd!=nullptr;}
    bool read_challenge(std::array<uint8_t,2>& out){auto v=challenge->readValue();if(v.size()<2)return false;out={v[0],v[1]};return client->isConnected();}
    bool write_response(const std::array<uint8_t,4>& b){return challenge->writeValue(b.data(),b.size(),challenge->canWrite())&&client->isConnected();}
    bool subscribe() {
        stream_handle=stream->getHandle();accepting=true;const auto token=session;
        bool ok=stream->subscribe(true,[token](NimBLERemoteCharacteristic*,uint8_t* p,size_t n,bool){
            audit_notification(p,n);
            if(accepting){auto now=now_us();if(queue.push(token,p,n,now))notification_ms=uint32_t(now/1000);}
        },true);
        if(!ok)accepting=false;
        return ok&&client->isConnected();
    }
    bool read_cccd(uint16_t& out){auto v=cccd->readValue();if(v.size()!=2)return false;out=uint16_t(v[0])|(uint16_t(v[1])<<8);return client->isConnected();}
};
// Scratch and completed-token state are used only by the serialized host task.
uint32_t completed_token{},identity_token{};
dragy_ref::ble::OperationResult::View identity;
int identity_callback(uint16_t,const ble_gatt_error* error,ble_gatt_attr* attr,void* arg){
    auto token=uint32_t(reinterpret_cast<uintptr_t>(arg));if(token!=operation_token.load()||completed_token==token)return 0;
    if(identity_token!=token){identity={};identity.token=token;identity_token=token;}
    const int n=attr&&attr->om?OS_MBUF_PKTLEN(attr->om):0;const int shown=std::min(n,48);
    if(error&&error->status==0&&n>0&&os_mbuf_copydata(attr->om,0,shown,identity.bytes.data())==0){
        identity.size=uint32_t(shown);identity.total=uint32_t(n);
        auto found=dragy_ref::ble::receiver_model(std::string_view(reinterpret_cast<const char*>(identity.bytes.data()),size_t(shown)));
        if(found==Receiver::Legacy||(identity.receiver!=Receiver::Legacy&&found!=Receiver::Unknown))identity.receiver=found;
        return 0;
    }
    identity.ok=error&&error->status==BLE_HS_EDONE;completed_token=token;operation_result.publish(identity);return 0;
}
int write_callback(uint16_t,const ble_gatt_error* error,ble_gatt_attr*,void* arg){
    auto token=uint32_t(reinterpret_cast<uintptr_t>(arg));if(token!=operation_token.load()||completed_token==token)return 0;
    dragy_ref::ble::OperationResult::View result;result.token=token;result.ok=error&&error->status==0;
    completed_token=token;operation_result.publish(result);return 0;
}
struct RateIO {
    NimBLERemoteCharacteristic* tx{};bool identity_read{},model_read{},identity_logged{};
    dragy_ref::ble::OperationResult::View completed;
    uint32_t next_token(){identity_logged=false;return ++operation_token;}
    void invalidate(){++operation_token;}
    void gate(bool pending){gate_control.publish(queue.boundary(),pending);}
    void watch(uint32_t token){reply_control.publish(queue.boundary(),token);}
    bool read(uint32_t token,uint16_t uuid){
        if(token!=operation_token.load()||!client->isConnected())return false;
        identity_read=true;ble_uuid16_t u=BLE_UUID16_INIT(uuid);
        return ble_gattc_read_by_uuid(client->getConnHandle(),1,0xffff,&u.u,identity_callback,reinterpret_cast<void*>(uintptr_t(token)))==0;
    }
    bool model(uint32_t token){model_read=true;return read(token,0x2a24);}
    bool firmware(uint32_t token){model_read=false;return read(token,0x2a26);}
    bool write(uint32_t token,const std::array<uint8_t,18>& bytes){
        identity_read=false;return token==operation_token.load()&&client->isConnected()&&tx&&ble_gattc_write_flat(client->getConnHandle(),tx->getHandle(),bytes.data(),bytes.size(),write_callback,reinterpret_cast<void*>(uintptr_t(token)))==0;
    }
    dragy_ref::ble::Completion completion(uint32_t token){
        if(token!=operation_token.load()||!operation_result.read(token,completed))return dragy_ref::ble::Completion::Pending;
        if(identity_read&&!identity_logged){
            identity_logged=true;char text[49]{};
            for(uint32_t i=0;i<completed.size;++i)text[i]=completed.bytes[i]>=32&&completed.bytes[i]<127?char(completed.bytes[i]):'?';
            printf("IDENTITY %s length=%lu truncated=%d text=%s hex=",model_read?"model_2A24":"firmware_2A26",(unsigned long)completed.total,completed.total>completed.size,text);
            for(uint32_t i=0;i<completed.size;++i)printf("%02x",completed.bytes[i]);
            puts("");
        }
        return completed.ok?dragy_ref::ble::Completion::Success:dragy_ref::ble::Completion::Failure;
    }
    Receiver model_receiver(){return completed.receiver;}
    bool writable()const{return tx&&tx->canWrite();}
    bool status(SharedStatus::View& v)const{return shared.read(v)&&v.gate==gate_control.revision();}
    bool clean25(uint64_t now)const{SharedStatus::View v;return status(v)&&v.clean>=8&&v.delta==40&&v.ms&&uint32_t(now/1000)-v.ms<=250;}
    bool timing_ready(uint64_t now)const{SharedStatus::View v;dragy_ref::BoundaryControl::View g;return gate_control.read(g)&&!g.value&&status(v)&&v.ready&&v.ms&&uint32_t(now/1000)-v.ms<=250;}
    dragy_ref::ble::Reply reply(uint32_t token)const{SharedStatus::View v;return shared.read(v)&&v.request==token?v.reply:dragy_ref::ble::Reply::None;}
    void log(const char* s){printf("RATE %s\n",s);}
    void profile(Receiver r,bool hint){printf("RATE profile=%s provisional=%d source=%s\n",dragy_ref::ble::receiver_name(r),r!=Receiver::Pro,hint?"same-peer advertisement":"model");}
};
void ble_task(void*){
    while(!consumer_ready)pause(5);
    NimBLEDevice::init("Dragy engineering reference");NimBLEDevice::setMTU(247);
    printf("Host byte audit enabled=%d\n",NimBLEDevice::setCustomGapHandler(gap_observer));
    auto* scan=NimBLEDevice::getScan();scan->setActiveScan(true);scan->setMaxResults(32);
    const std::string selected=CONFIG_REFERENCE_PEER_ADDRESS;
#ifdef CONFIG_REFERENCE_PEER_RANDOM
    const uint8_t selected_type=BLE_ADDR_RANDOM;
#else
    const uint8_t selected_type=BLE_ADDR_PUBLIC;
#endif
#ifdef CONFIG_REFERENCE_REQUEST_25HZ
    constexpr bool request=true;
#else
    constexpr bool request=false;
#endif
#ifdef CONFIG_REFERENCE_PROVISIONAL_RECEIVERS
    constexpr bool provisional=true;
#else
    constexpr bool provisional=false;
#endif
    client=NimBLEDevice::createClient();client->setConnectTimeout(5000);client->setClientCallbacks(&events,false);
    RateIO rate_io;dragy_ref::ble::RateSetup<RateIO> rate(rate_io);unsigned failures=0;
    for(;;){
        accepting=false;auto session=queue.new_session();rate.cancel();notification_ms=0;sample_ms=0;
        audited=compared=chained=mismatches=copy_failures=0;
        auto results=scan->getResults(3000);const NimBLEAdvertisedDevice* peer=nullptr;Receiver hint=Receiver::Unknown;
        for(int i=0;i<results.getCount();++i){
            auto* d=results.getDevice(i);auto name=d->getName();
            if(!dragy_ref::ble::dragy_candidate(name,d->isAdvertisingService(NimBLEUUID(uint16_t(0xfd00)))))continue;
            if(!selected.empty()&&d->getAddress()==NimBLEAddress(selected,selected_type)){peer=d;hint=dragy_ref::ble::receiver_model(name);}
            for(char& c:name)if(c<32||c>126)c='?';
            printf("Candidate address=%s type=%u name=%s\n",d->getAddress().toString().c_str(),d->getAddress().getType(),name.c_str());
        }
        if(!peer){scan->clearResults();pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        if(!client->connect(peer)){scan->clearResults();pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        scan->clearResults();Adapter io;io.session=session;
        if(!dragy_ref::ble::setup(io)){accepting=false;client->disconnect();while(client->isConnected())pause(20);pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        failures=0;auto connected=now_us();bool seen=false,repaired=false;rate_io.tx=io.rate;rate.start(connected,hint,request,provisional,true);
        while(client->isConnected()){
            auto now=now_us();rate.poll(now);auto sample=sample_ms.load();seen|=sample!=0;
            auto recent=[&](uint32_t ms)->uint64_t{return ms?now-uint64_t(uint32_t(uint32_t(now/1000)-ms))*1000:0;};
            if(dragy_ref::ble::reconnect_due(now,connected,recent(notification_ms.load()),recent(sample),seen)){puts("Stream recovery: reconnect");client->disconnect();break;}
            if(dragy_ref::ble::repair_due(now,connected,seen,repaired)){
                if(rate.att_owned()){puts("Stream recovery: disconnect to retire optional ATT operation");client->disconnect();break;}
                repaired=true;accepting=false;rate.cancel();io.session=queue.new_session();
                if(!dragy_ref::ble::setup(io)){client->disconnect();break;}
                rate_io.tx=io.rate;rate.start(now_us(),hint,request,provisional,false);
            }
            pause(20);
        }
        accepting=false;rate.cancel();queue.new_session();while(client->isConnected())pause(20);
        pause(1000*dragy_ref::ble::backoff(failures++));
    }
}
}
extern "C" void app_main(){
    // Never erase NVS automatically. Use a blank development board.
    ESP_ERROR_CHECK(nvs_flash_init());
    configASSERT(xTaskCreate(decode_task,"reference_decode",8192,nullptr,4,nullptr)==pdPASS);
    configASSERT(xTaskCreate(ble_task,"reference_ble",12288,nullptr,3,nullptr)==pdPASS);
}
