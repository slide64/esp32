"""Compile the real ES8311 driver and inject I2C transfer failures."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
mock = r'''
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
using esp_err_t = int;
using i2c_port_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_ARG=2;
constexpr unsigned BIT(unsigned bit) { return 1U << bit; }
inline void vTaskDelay(int) {}
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { (void)(tag); int err=(expr); if(err!=ESP_OK) return err; } while(0)
#define ESP_RETURN_ON_FALSE(cond, err, tag, ...) do { (void)(tag); if(!(cond)) return (err); } while(0)
#define ESP_ERROR_CHECK(expr) assert((expr)==ESP_OK)
struct WireMock {
  uint8_t regs[256]{};
  int reg=0, value=0, count=0, nackReg=-1;
  bool shortRead=false, missingByte=false;
  void beginTransmission(uint8_t address) { assert(address==0x18); count=0; }
  size_t write(uint8_t byte) { if(count++==0) reg=byte; else value=byte; return 1; }
  uint8_t endTransmission(bool=true) {
    if(reg==nackReg) return 2;
    if(count==2) regs[reg]=value;
    return 0;
  }
  size_t requestFrom(uint8_t,uint8_t len) { return shortRead ? 0 : len; }
  int read() { return missingByte ? -1 : regs[reg++]; }
};
inline WireMock Wire;
'''
harness = r'''
#include "board/es8311.h"
int main() {
  auto codec=es8311_create(0,0x18); assert(codec);
  es8311_clock_config_t clock{};
  clock.mclk_from_mclk_pin=true;
  clock.mclk_frequency=6144000; clock.sample_frequency=24000;
  assert(es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)==ESP_OK);
  assert(es8311_voice_volume_set(codec,75,nullptr)==ESP_OK);
  assert(Wire.regs[0x32]==0xBF); // unity gain, tested through the real driver
  Wire.regs[0x31]=0x60;
  assert(es8311_voice_mute(codec,false)==ESP_OK);
  assert((Wire.regs[0x31]&0x60)==0);
  Wire.shortRead=true;
  assert(es8311_sample_frequency_config(codec,6144000,24000)==ESP_FAIL);
  assert(std::strstr(es8311_last_i2c_error(),"R 0x18 reg0x02 octets0/1"));
  Wire.shortRead=false; Wire.missingByte=true;
  assert(es8311_sample_frequency_config(codec,6144000,24000)==ESP_FAIL);
  assert(std::strstr(es8311_last_i2c_error(),"octet manquant"));
  Wire.missingByte=false; Wire.nackReg=0x17; Wire.regs[0x14]=0;
  assert(es8311_microphone_config(codec,false)==ESP_FAIL);
  assert(std::strstr(es8311_last_i2c_error(),"W 0x18 reg0x17 code2"));
  assert(Wire.regs[0x14]==0); // do not continue after failed ADC write
  Wire.nackReg=-1;
  assert(es8311_microphone_config(codec,false)==ESP_OK);
  es8311_delete(codec);
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    (tmp/'mock.h').write_text(mock)
    for name in ('esp_types.h', 'esp_err.h', 'driver/i2c.h', 'driver/gpio.h',
                 'esp_log.h', 'esp_check.h', 'Wire.h'):
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "mock.h"\n')
    (tmp/'test.cpp').write_text(harness)
    binary = tmp/'test_codec_i2c'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-sign-compare', '-I'+str(tmp), '-I'+str(root/'src'),
                    str(root/'src/board/es8311.cpp'), str(tmp/'test.cpp'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('ES8311: gain, unmute, short/missing reads and failed writes OK (mock I2C)')
