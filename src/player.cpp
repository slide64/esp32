#include "player.h"
#include "wav.h"
#include "board/es8311.h"
#include <ESP_I2S.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
std::atomic<unsigned> audioVolume{30};
std::atomic<bool> audioPlaying{false};
struct Request { char path[64]; bool loop; };
static QueueHandle_t requests;
static I2SClass output;
static bool ready;
static char diagnostic[96]="initialisation_non_lancee";
const char *audioDiagnostic() { return diagnostic; }
static bool audioFailure(const char *stage, int error) {
  snprintf(diagnostic,sizeof(diagnostic),"%s (%s, %d)",stage,esp_err_to_name(error),error);
  Serial.printf("Audio : %s\n",diagnostic);
  return false;
}
static void audioTask(void *) {
  File file;
  WavInfo info;
  uint32_t remaining=0;
  bool repeat=false;
  int16_t samples[256];
  for (;;) {
    Request req{};
    if (xQueueReceive(requests,&req,0)==pdTRUE) {
      file.close(); audioPlaying=false; remaining=0; repeat=req.loop;
      if (req.path[0]) {
        file=SD_MMC.open(req.path);
        if (file && parseWav(file,info)) { remaining=info.bytes; audioPlaying=true; }
        else { Serial.println("Audio absent ou WAV incompatible"); file.close(); }
      }
    }
    if (file && remaining) {
      size_t want=remaining>sizeof(samples)?sizeof(samples):remaining;
      size_t n=file.read(reinterpret_cast<uint8_t*>(samples),want);
      if (!n || n%2) { file.close(); remaining=0; audioPlaying=false; continue; }
      remaining-=n;
      unsigned gain=audioVolume.load();
      for (size_t i=0;i<n/2;++i) samples[i]=int32_t(samples[i])*gain/100;
      output.write(reinterpret_cast<uint8_t*>(samples),n);
      if (!remaining) {
        if (repeat && file.seek(info.offset)) remaining=info.bytes;
        else { file.close(); audioPlaying=false; }
      }
    } else {
      memset(samples,0,sizeof(samples));
      output.write(reinterpret_cast<uint8_t*>(samples),sizeof(samples));
    }
  }
}
bool initAudio() {
  if (ready) return true;
  pinMode(15,OUTPUT); digitalWrite(15,LOW);
  // Supply MCLK before resetting/powering the codec's digital circuitry.
  output.setPins(48,38,47,-1,2);
  if (!output.begin(I2S_MODE_STD,24000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT))
    return audioFailure("Initialisation I2S",output.lastError());
  // ES8311 CE selects one of the two addresses documented by Espressif.
  uint8_t address=ES8311_ADDRESS_0;
  Wire.beginTransmission(address);
  uint8_t probe=Wire.endTransmission();
  if (probe) {
    address=ES8311_ADDRESS_1;
    Wire.beginTransmission(address);
    probe=Wire.endTransmission();
  }
  if (probe) {
    output.end();
    snprintf(diagnostic,sizeof(diagnostic),"ES8311 absent a 0x18/0x19 (I2C %u)",probe);
    Serial.printf("Audio : %s\n",diagnostic);
    return false;
  }
  auto codec=es8311_create(I2C_NUM_0,address);
  if (!codec) { output.end(); return audioFailure("Allocation codec",ESP_ERR_NO_MEM); }
  es8311_clock_config_t clock{};
  clock.mclk_from_mclk_pin=true;
  clock.mclk_frequency=24000*256;
  clock.sample_frequency=24000;
  esp_err_t error=es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16);
  if (error!=ESP_OK) { es8311_delete(codec); output.end(); return audioFailure("Initialisation ES8311",error); }
  // This API maps 75 to register 0xBF (0 dB), not to 75% amplitude.
  // Keep codec gain at unity; audioVolume already scales the PCM samples.
  error=es8311_voice_volume_set(codec,75,nullptr);
  if (error!=ESP_OK) { es8311_delete(codec); output.end(); return audioFailure("Volume ES8311",error); }
  error=es8311_microphone_config(codec,false);
  if (error!=ESP_OK) { es8311_delete(codec); output.end(); return audioFailure("Configuration ES8311",error); }
  error=es8311_voice_mute(codec,false);
  es8311_delete(codec);
  if (error!=ESP_OK) { output.end(); return audioFailure("Activation sortie ES8311",error); }
  requests=xQueueCreate(1,sizeof(Request));
  if (!requests) { output.end(); return audioFailure("File de commandes",ESP_ERR_NO_MEM); }
  if (xTaskCreatePinnedToCore(audioTask,"wav",6144,nullptr,2,nullptr,1)!=pdPASS) {
    vQueueDelete(requests); requests=nullptr; output.end();
    return audioFailure("Tache audio",ESP_ERR_NO_MEM);
  }
  strlcpy(diagnostic,"OK",sizeof(diagnostic));
  digitalWrite(15,HIGH); ready=true; return true;
}
void playAudio(const char *path,bool loop) {
  if (!ready) return;
  Request req{}; strlcpy(req.path,path,sizeof(req.path)); req.loop=loop;
  xQueueOverwrite(requests,&req);
}
void stopAudio() { playAudio(""); }
