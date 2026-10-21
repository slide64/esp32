// Standalone Waveshare 1.85C hardware test; see README for the Upload target.
// Pinout: ESP32-S3-Touch-LCD-1.85C-V2 schematic, U9/U10 and GPIO table.
// Initialization follows Waveshare Arduino example 03_audio_out_no_tf.
#ifdef AUDIO_SELFTEST
#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <cmath>
#include "board/es8311.h"

static constexpr int sdaPin=11, sclPin=10, ampPin=15;
static constexpr uint32_t sampleRate=24000;
static I2SClass output;
static char status[240]="initialisation";
static char bus[32]{};
static bool playing;
static bool i2sStarted;
static int16_t toneSamples[240], silence[240]{};

static void scanKnownDevices() {
  size_t used=0;
  const uint8_t devices[]={0x15,0x18,0x19,0x20,0x40,0x51};
  for (uint8_t address : devices) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission()==0)
      used+=snprintf(bus+used,sizeof(bus)-used," %02X",address);
  }
  if (!used) strlcpy(bus," aucun",sizeof(bus));
}

static bool startI2S() {
  if (i2sStarted) return true;
  output.setPins(48,38,47,-1,2);
  i2sStarted=output.begin(I2S_MODE_STD,sampleRate,I2S_DATA_BIT_WIDTH_16BIT,
                          I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT);
  if (!i2sStarted)
    snprintf(status,sizeof(status),"I2S echec (%d)",output.lastError());
  return i2sStarted;
}

static bool configureCodec() {
  auto codec=es8311_create(I2C_NUM_0,ES8311_ADDRESS_0);
  if (!codec) {
    strlcpy(status,"allocation codec impossible",sizeof(status));
    return false;
  }
  es8311_clock_config_t clock{};
  clock.mclk_from_mclk_pin=true;
  clock.mclk_frequency=sampleRate*256;
  clock.sample_frequency=sampleRate;
  esp_err_t error=es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16);
  const char *stage="initialisation";
  if (error==ESP_OK) {
    stage="volume";
    error=es8311_voice_volume_set(codec,75,nullptr); // 0 dB; tone is attenuated in PCM
  }
  if (error==ESP_OK) {
    stage="microphone";
    error=es8311_microphone_config(codec,false);
  }
  if (error==ESP_OK) {
    stage="unmute";
    error=es8311_voice_mute(codec,false);
  }
  es8311_delete(codec);
  if (error!=ESP_OK) {
    snprintf(status,sizeof(status),"%s: %s (%s, %d)",stage,
             es8311_last_i2c_error(),esp_err_to_name(error),error);
    return false;
  }
  return true;
}

void setup() {
  pinMode(ampPin,OUTPUT); digitalWrite(ampPin,LOW);
  Serial.begin(115200);
  const uint32_t started=millis();
  while (!Serial && millis()-started<3000) delay(10);
  if (!Wire.begin(sdaPin,sclPin,100000)) {
    strlcpy(status,"Wire.begin(11,10,100000) a echoue",sizeof(status));
    return;
  }
  delay(100);
  scanKnownDevices();
  Serial.printf("audio-selftest-1 | I2C ACK:%s\n",bus);

  // First attempt: same codec-before-I2S order as the official example.
  if (!configureCodec()) {
    Serial.printf("Sans MCLK: %s\n",status);
    // Some codec revisions need MCLK during configuration: try that explicitly.
    if (!startI2S() || !configureCodec()) return;
  } else if (!startI2S()) return;

  for (size_t i=0;i<240;++i)
    toneSamples[i]=static_cast<int16_t>(2500*std::sin(2*3.141592653589793*600*i/sampleRate));
  strlcpy(status,"OK - bip 600 Hz, 0.5 s toutes les 2 s",sizeof(status));
  digitalWrite(ampPin,HIGH);
  playing=true;
}

void loop() {
  static uint32_t lastReport=0;
  if (millis()-lastReport>=3000) {
    lastReport=millis();
    Serial.printf("audio-selftest-1 | I2C ACK:%s | audio=%s\n",bus,status);
  }
  if (!playing) { delay(20); return; }
  const auto *samples=millis()%2000<500 ? toneSamples : silence;
  const size_t n=output.write(reinterpret_cast<const uint8_t*>(samples),sizeof(toneSamples));
  if (n!=sizeof(toneSamples)) {
    digitalWrite(ampPin,LOW);
    playing=false;
    snprintf(status,sizeof(status),"ecriture I2S incomplete %u/%u (%d)",
             unsigned(n),unsigned(sizeof(toneSamples)),output.lastError());
  }
}
#endif
