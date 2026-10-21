"""Exercise the real player initialization with fake hardware, including failures."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
mock = r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
using esp_err_t = int;
using i2c_port_t = int;
constexpr int ESP_OK=0, ESP_ERR_NO_MEM=1, I2C_NUM_0=0;
constexpr int OUTPUT=1, LOW=0, HIGH=1, pdTRUE=1, pdPASS=1;
constexpr int I2S_MODE_STD=0, I2S_DATA_BIT_WIDTH_16BIT=16;
constexpr int I2S_SLOT_MODE_MONO=1, I2S_STD_SLOT_LEFT=1;
inline bool clockOn=false, ampOn=false, codecAlive=false;
inline int failure=0, volume=0, creations=0, codecAddress=0;
inline const char *esp_err_to_name(int) { return "mock_error"; }
inline void pinMode(int,int) {}
inline void digitalWrite(int pin,int value) { assert(pin==15); ampOn=value; }
inline size_t strlcpy(char *d,const char *s,size_t n) { std::snprintf(d,n,"%s",s); return std::strlen(s); }
struct SerialMock { template<class... A> void printf(const char *,A...) {} void println(const char *) {} };
inline SerialMock Serial;
struct WireMock {
  int address=0;
  void beginTransmission(int a) { assert(clockOn); address=a; }
  int endTransmission() { return failure==1 || (failure==10 && address==0x18); }
};
inline WireMock Wire;
struct File {
  void close() {} explicit operator bool() const { return false; }
  size_t size() { return 0; } bool seek(size_t) { return false; }
  size_t read(uint8_t *,size_t) { return 0; }
};
struct SDMock { File open(const char *) { return {}; } };
inline SDMock SD_MMC;
struct I2SClass {
  void setPins(int,int,int,int,int) {}
  bool begin(int,int rate,int,int,int) { assert(rate==24000); return clockOn=failure!=2; }
  void end() { clockOn=false; } int lastError() { return 2; }
  size_t write(uint8_t *,size_t n) { return n; }
};
using QueueHandle_t = void *;
inline QueueHandle_t xQueueCreate(int,size_t) { return failure==8 ? nullptr : reinterpret_cast<void *>(1); }
inline void vQueueDelete(QueueHandle_t) {}
inline int xQueueReceive(QueueHandle_t,void *,int) { return 0; }
inline void xQueueOverwrite(QueueHandle_t,void *) {}
inline int xTaskCreatePinnedToCore(void (*)(void *),const char *,int,void *,int,void *,int) { return failure==9 ? 0 : pdPASS; }
'''
harness = r'''
#include "board/es8311.h"
#include "player.h"
extern "C" {
es8311_handle_t es8311_create(i2c_port_t,uint16_t address) {
  assert(address==(creations==0 ? 0x18 : 0x19));
  codecAddress=address;
  ++creations;
  assert(clockOn); codecAlive=failure!=3 && !(failure==11 && creations==2);
  return codecAlive ? reinterpret_cast<void *>(1) : nullptr;
}
void es8311_delete(es8311_handle_t) { codecAlive=false; }
esp_err_t es8311_init(es8311_handle_t,const es8311_clock_config_t *c,es8311_resolution_t,es8311_resolution_t) {
  assert(clockOn && c->mclk_frequency==6144000 && c->sample_frequency==24000);
  return failure==4 || ((failure==10 || failure==11) && codecAddress==0x18);
}
esp_err_t es8311_voice_volume_set(es8311_handle_t,int v,int *) { volume=v; return failure==5; }
esp_err_t es8311_microphone_config(es8311_handle_t,bool digital) { assert(!digital); return failure==6; }
esp_err_t es8311_voice_mute(es8311_handle_t,bool muted) { assert(!muted); return failure==7; }
}
int main(int argc,char **) {
  failure=argc-1;
  bool ok=initAudio();
  assert(ok==(failure==0 || failure==1 || failure==10)); assert(!codecAlive);
  assert(ampOn==ok && clockOn==ok);
  if(ok) {
    assert(volume==75); assert((volume*256/100)-1==0xBF);
    assert(std::strcmp(audioDiagnostic(),"OK")==0);
    assert(initAudio()); assert(creations==(failure==10 ? 2 : 1));
  }
  else assert(std::strcmp(audioDiagnostic(),"OK")!=0);
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp/'mock.h').write_text(mock)
    for name in ('Arduino.h', 'ESP_I2S.h', 'SD_MMC.h', 'Wire.h', 'esp_types.h',
                 'esp_err.h', 'driver/i2c.h', 'freertos/FreeRTOS.h', 'freertos/queue.h'):
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp/'test.cpp').write_text(harness)
    binary = tmp/'test_audio_init'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(tmp), '-I'+str(root/'include'), '-I'+str(root/'src'),
                    str(root/'src/player.cpp'), str(tmp/'test.cpp'), '-o', str(binary)], check=True)
    for failure in range(12):
        subprocess.run([str(binary)] + ['failure']*failure, check=True)
print('Audio init: rejected empty probe, both addresses, repeated init and 9 failure paths OK (mock hardware)')
