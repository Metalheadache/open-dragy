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
#include "dragy_ref/drag.hpp"
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/rate.hpp"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gatt.h"
#include "os/os_mbuf.h"
#include <atomic>
#include <cstdio>
#include <cstring>
namespace {
using Queue=dragy_ref::Ingress<>;
Queue queue;
std::atomic<uint32_t> notification_ms{0},sample_ms{0},rate_epoch{0};
std::atomic<uint32_t> rate_token{0},rate_done{0};
std::atomic<bool> model_pro{false},rate_ok{false},accepting{false};
NimBLEClient* client{};
uint64_t now_us(){return uint64_t(esp_timer_get_time());}
void pause(unsigned ms){vTaskDelay(pdMS_TO_TICKS(ms));}
struct Consumer {
    dragy_ref::drag::Engine engine;
    dragy_ref::ble::CadenceVerifier cadence;
    uint32_t revision{},observed_rate_epoch{},printed{};
} consumer;
void sample(const dragy_ref::Sample& s,uint64_t arrival,void*) {
    sample_ms=uint32_t(arrival/1000);
    if(consumer.observed_rate_epoch!=rate_epoch.load()){
        consumer.observed_rate_epoch=rate_epoch.load();consumer.cadence.reset();consumer.printed=0;
    }
    if(consumer.cadence.observe(s.tow_ms)&&!consumer.printed++)puts("Receiver cadence: 25 consecutive 40 ms intervals");
    consumer.engine.accept(s,arrival);
    printf("GPS tow_ms=%lu lat=%.7f lon=%.7f raw_kmh=%.3f fix=%u sv=%u hacc_mm=%lu sacc_mm_s=%lu\n",
           (unsigned long)s.tow_ms,s.lat_e7/1e7,s.lon_e7/1e7,s.speed_mm_s*.0036,s.fix,s.satellites,
           (unsigned long)s.hacc_mm,(unsigned long)s.sacc_mm_s);
}
void gap(void*){consumer.engine.interrupt();consumer.cadence.reset();consumer.printed=0;}
void decode_task(void*) {
#ifdef CONFIG_REFERENCE_ARM_TESTS
    consumer.engine.arm();
#endif
    dragy_ref::Decoder<Queue> decoder(queue,sample,gap,nullptr);
    uint32_t reported_drops=0;
    for(;;) {
        decoder.drain();consumer.engine.poll(now_us());
        if(consumer.revision!=consumer.engine.view().revision) {
            consumer.revision=consumer.engine.view().revision;
            for(unsigned i=0;i<dragy_ref::drag::count;++i){auto& r=consumer.engine.view().channels[i].result;
                if(r.sequence)printf("TEST id=%u phase=%s fault=%s elapsed_s=%.6f distance_m=%.3f\n",i,
                    dragy_ref::drag::phase_text(r.phase),dragy_ref::drag::fault_text(r.fault),r.elapsed_s,r.distance_m);
            }
        }
        if(queue.drops()!=reported_drops){reported_drops=queue.drops();printf("Ingress drops=%lu; continuity broken\n",(unsigned long)reported_drops);}
        pause(5);
    }
}
class Events final:public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*,int) override {
        accepting=false;queue.new_session();++rate_token;
    }
} events;
struct Adapter {
    uint32_t session{};
    NimBLERemoteCharacteristic *challenge{},*stream{},*rate{};
    NimBLERemoteDescriptor* cccd{};
    void stage(dragy_ref::ble::Stage s){printf("BLE %s\n",dragy_ref::ble::name(s));}
    bool fail(const char* why){printf("BLE setup: %s\n",why);return false;}
    bool discover() {
        auto* service=client->getService(NimBLEUUID(uint16_t(0xfd00)));if(!service)return false;
        uint16_t previous=0;
        for(auto* c:service->getCharacteristics(true)) {
            if(c->getHandle()<=previous)return false;
            previous=c->getHandle();
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd01)))rate=c;
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd02)))stream=c;
            if(c->getUUID()==NimBLEUUID(uint16_t(0xfd03)))challenge=c;
        }
        if(!challenge||!stream||!challenge->canRead()||(!challenge->canWrite()&&!challenge->canWriteNoResponse())||!stream->canNotify())return false;
        cccd=stream->getDescriptor(NimBLEUUID(uint16_t(0x2902)));return client->isConnected();
    }
    bool has_cccd()const{return cccd!=nullptr;}
    bool read_challenge(std::array<uint8_t,2>& out){auto v=challenge->readValue();if(v.size()<2)return false;out={v[0],v[1]};return client->isConnected();}
    bool write_response(const std::array<uint8_t,4>& b){return challenge->writeValue(b.data(),b.size(),challenge->canWrite())&&client->isConnected();}
    bool subscribe() {
        accepting=true;const auto token=session;
        bool ok=stream->subscribe(true,[token](NimBLERemoteCharacteristic*,uint8_t* p,size_t n,bool){
            if(accepting){auto now=now_us();if(queue.push(token,p,n,now))notification_ms=uint32_t(now/1000);}
        },true);
        if(!ok)accepting=false;
        return ok&&client->isConnected();
    }
    bool read_cccd(uint16_t& out){auto v=cccd->readValue();if(v.size()!=2)return false;out=uint16_t(v[0])|(uint16_t(v[1])<<8);return client->isConnected();}
};
#ifdef CONFIG_REFERENCE_REQUEST_25HZ
int model_callback(uint16_t,const ble_gatt_error* error,ble_gatt_attr* attr,void* arg) {
    auto token=uint32_t(reinterpret_cast<uintptr_t>(arg));if(token!=rate_token.load()||rate_done.load()==token)return 0;
    uint8_t b[64];int n=attr&&attr->om?OS_MBUF_PKTLEN(attr->om):0;
    if(error&&error->status==0&&n>0&&n<=int(sizeof(b))&&os_mbuf_copydata(attr->om,0,n,b)==0){model_pro=model_pro.load()||dragy_ref::ble::pro_model(b,size_t(n));return 0;}
    if(!error||error->status!=BLE_HS_EDONE)model_pro=false;
    rate_done=token;return 0;
}
int write_callback(uint16_t,const ble_gatt_error* error,ble_gatt_attr*,void* arg) {
    auto token=uint32_t(reinterpret_cast<uintptr_t>(arg));if(token!=rate_token.load())return 0;
    rate_ok=error&&error->status==0;rate_done=token;return 0;
}
bool await_rate(uint32_t token) {
    auto deadline=now_us()+3000000;
    while(client->isConnected()&&rate_token.load()==token&&rate_done.load()!=token&&now_us()<deadline)pause(10);
    return client->isConnected()&&rate_token.load()==token&&rate_done.load()==token;
}
#endif
void request_rate(Adapter& io) {
#ifdef CONFIG_REFERENCE_REQUEST_25HZ
    if(!io.rate||!io.rate->canWrite())return;
    model_pro=false;auto token=++rate_token;ble_uuid16_t uuid=BLE_UUID16_INIT(0x2a24);
    int rc=ble_gattc_read_by_uuid(client->getConnHandle(),1,0xffff,&uuid.u,model_callback,reinterpret_cast<void*>(uintptr_t(token)));
    if(rc||!await_rate(token)||!model_pro){++rate_token;puts("25 Hz: model unavailable or unsupported; stream retained");return;}
    auto b=dragy_ref::ble::pro_rate_frame();token=++rate_token;rate_ok=false;++rate_epoch;
    rc=ble_gattc_write_flat(client->getConnHandle(),io.rate->getHandle(),b.data(),b.size(),write_callback,reinterpret_cast<void*>(uintptr_t(token)));
    bool ok=!rc&&await_rate(token)&&rate_ok;++rate_token;
    printf("25 Hz RAM request ATT_ok=%d; inspect receiver cadence separately\n",ok);
#else
    (void)io;
#endif
}
void ble_task(void*) {
    NimBLEDevice::init("Dragy engineering reference");NimBLEDevice::setMTU(247);
    auto* scan=NimBLEDevice::getScan();scan->setActiveScan(true);scan->setMaxResults(32);
    const std::string selected=CONFIG_REFERENCE_PEER_ADDRESS;
#ifdef CONFIG_REFERENCE_PEER_RANDOM
    const uint8_t selected_type=BLE_ADDR_RANDOM;
#else
    const uint8_t selected_type=BLE_ADDR_PUBLIC;
#endif
    client=NimBLEDevice::createClient();client->setConnectTimeout(5000);client->setClientCallbacks(&events,false);
    unsigned failures=0;
    for(;;) {
        accepting=false;auto session=queue.new_session();notification_ms=0;sample_ms=0;
        auto results=scan->getResults(3000);const NimBLEAdvertisedDevice* peer=nullptr;
        for(int i=0;i<results.getCount();++i) {
            auto* d=results.getDevice(i);auto name=d->getName();
            if(!d->isAdvertisingService(NimBLEUUID(uint16_t(0xfd00)))&&name.rfind("DRG",0)!=0&&name.find("Dragy")==std::string::npos)continue;
            for(char& c:name)if(c<32||c>126)c='?';
            printf("Candidate address=%s type=%u name=%s\n",d->getAddress().toString().c_str(),d->getAddress().getType(),name.c_str());
            if(!selected.empty()&&d->getAddress()==NimBLEAddress(selected,selected_type))peer=d;
        }
        if(!peer){scan->clearResults();pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        if(!client->connect(peer)){scan->clearResults();pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        scan->clearResults();Adapter io;io.session=session;
        if(!dragy_ref::ble::setup(io)){accepting=false;client->disconnect();while(client->isConnected())pause(20);pause(1000*dragy_ref::ble::backoff(failures++));continue;}
        failures=0;auto connected=now_us();bool seen=false,repaired=false;request_rate(io);
        while(client->isConnected()) {
            auto now=now_us();auto sample=sample_ms.load();seen|=sample!=0;
            // Reconstruct recent timestamps across 32-bit millisecond wrap.
            auto recent=[&](uint32_t ms)->uint64_t{return ms?now-uint64_t(uint32_t(uint32_t(now/1000)-ms))*1000:0;};
            if(dragy_ref::ble::reconnect_due(now,connected,recent(notification_ms.load()),recent(sample),seen)){puts("Stream recovery: reconnect");client->disconnect();break;}
            if(dragy_ref::ble::repair_due(now,connected,seen,repaired)) {
                repaired=true;accepting=false;io.session=queue.new_session();
                if(!dragy_ref::ble::setup(io)){client->disconnect();break;}
                request_rate(io);
            }
            pause(100);
        }
        accepting=false;++rate_token;queue.new_session();while(client->isConnected())pause(20);
        pause(1000*dragy_ref::ble::backoff(failures++));
    }
}
}
extern "C" void app_main() {
    // Never erase NVS automatically. Use a blank development board.
    ESP_ERROR_CHECK(nvs_flash_init());
    configASSERT(xTaskCreate(decode_task,"reference_decode",8192,nullptr,4,nullptr)==pdPASS);
    configASSERT(xTaskCreate(ble_task,"reference_ble",12288,nullptr,3,nullptr)==pdPASS);
}
