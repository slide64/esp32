#include "player.h"
#include "wav.h"
#include "board/es8311.h"
#include <ESP_I2S.h>
#include <SD_MMC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
std::atomic<unsigned> audioVolume{30};
std::atomic<bool> audioPlaying{false};
struct Request { char path[64]; bool loop; };
static QueueHandle_t requests;
static I2SClass output;
static bool ready;
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
  pinMode(15,OUTPUT); digitalWrite(15,LOW);
  auto codec=es8311_create(I2C_NUM_0,ES8311_ADDRRES_0);
  es8311_clock_config_t clock{};
  clock.mclk_from_mclk_pin=true;
  clock.mclk_frequency=24000*256;
  clock.sample_frequency=24000;
  if (!codec || es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)!=ESP_OK ||
      es8311_voice_volume_set(codec,65,nullptr)!=ESP_OK ||
      es8311_microphone_config(codec,false)!=ESP_OK) return false;
  output.setPins(48,38,47,-1,2);
  if (!output.begin(I2S_MODE_STD,24000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT)) return false;
  requests=xQueueCreate(1,sizeof(Request));
  if (!requests || xTaskCreatePinnedToCore(audioTask,"wav",6144,nullptr,2,nullptr,1)!=pdPASS) return false;
  digitalWrite(15,HIGH); ready=true; return true;
}
void playAudio(const char *path,bool loop) {
  if (!ready) return;
  Request req{}; strlcpy(req.path,path,sizeof(req.path)); req.loop=loop;
  xQueueOverwrite(requests,&req);
}
void stopAudio() { playAudio(""); }
