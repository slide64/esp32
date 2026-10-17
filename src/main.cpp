#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SD_MMC.h>
#include "settings.h"
#include "screen.h"
#include "player.h"
#include "board/I2C_Driver.h"
#include "board/TCA9554PWR.h"

static WiFiClient transport;
static PubSubClient mqtt(transport);
static String base=String("halloween/")+DEVICE_ID;
static String monster="pumpkin", track="silence";
static bool enabled=true, sdReady=false, audioReady=false;
static bool validMonster(const String &s) { return s=="pumpkin" || s=="demon" || s=="zombie"; }
static void state() {
  String json=String("{\"monster\":\"")+monster+"\",\"track\":\""+track+
    "\",\"power\":\""+(enabled?"ON":"OFF")+"\",\"volume\":"+audioVolume.load()+
    ",\"playing\":"+(audioPlaying.load()?"true":"false")+
    ",\"sd\":"+(sdReady?"true":"false")+",\"audio\":"+(audioReady?"true":"false")+"}";
  mqtt.publish((base+"/state").c_str(),json.c_str(),true);
}
static void discoveryEntity(const char *kind,const char *id,const char *name,const String &fields) {
  String unique=String(DEVICE_ID)+"_"+id;
  String json=String("{\"name\":\"")+name+"\",\"unique_id\":\""+unique+
    "\",\"availability_topic\":\""+base+"/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
    "\"device\":{\"identifiers\":[\""+DEVICE_ID+"\"],\"name\":\"Monstre Halloween\",\"manufacturer\":\"Waveshare\","
    "\"model\":\"ESP32-S3-Touch-LCD-1.85C-BOX\"},"+fields+"}";
  String topic=String("homeassistant/")+kind+"/"+unique+"/config";
  if (!mqtt.publish(topic.c_str(),json.c_str(),true)) Serial.println("Publication discovery echouee");
}
static void discover() {
  String common=String("\"state_topic\":\"")+base+"/state\",";
  discoveryEntity("select","monster","Monstre",common+"\"value_template\":\"{{ value_json.monster }}\",\"command_topic\":\""+base+"/monster/set\",\"options\":[\"pumpkin\",\"demon\",\"zombie\"]");
  discoveryEntity("select","track","Son",common+"\"value_template\":\"{{ value_json.track }}\",\"command_topic\":\""+base+"/track/set\",\"options\":[\"silence\",\"ambiance\",\"voix\"]");
  discoveryEntity("switch","power","Actif",common+"\"value_template\":\"{{ value_json.power }}\",\"command_topic\":\""+base+"/power/set\"");
  discoveryEntity("number","volume","Volume",common+"\"value_template\":\"{{ value_json.volume }}\",\"command_topic\":\""+base+"/volume/set\",\"min\":0,\"max\":100,\"step\":1,\"mode\":\"slider\",\"unit_of_measurement\":\"%\"");
  discoveryEntity("button","scare","Faire peur",String("\"command_topic\":\"")+base+"/scare/set\",\"payload_press\":\"PRESS\"");
  discoveryEntity("button","stop","Arreter le son",String("\"command_topic\":\"")+base+"/stop/set\",\"payload_press\":\"PRESS\"");
  discoveryEntity("binary_sensor","playing","Lecture audio",common+"\"value_template\":\"{{ 'ON' if value_json.playing else 'OFF' }}\"");
  discoveryEntity("binary_sensor","sd","Carte microSD",common+"\"value_template\":\"{{ 'ON' if value_json.sd else 'OFF' }}\",\"entity_category\":\"diagnostic\"");
  discoveryEntity("binary_sensor","audio","Audio disponible",common+"\"value_template\":\"{{ 'ON' if value_json.audio else 'OFF' }}\",\"entity_category\":\"diagnostic\"");
}
static void startTrack() {
  if (!enabled || track=="silence") { stopAudio(); return; }
  if (track=="ambiance") playAudio("/audio/ambiance.wav",true);
  else playAudio((String("/audio/")+monster+".wav").c_str());
}
static void message(char *topic,uint8_t *bytes,unsigned length) {
  if (length>40) return;
  String value; value.reserve(length);
  for (unsigned i=0;i<length;++i) { if (!bytes[i]) return; value+=char(bytes[i]); }
  String t(topic);
  if (t=="homeassistant/status") { if (value=="online") { discover(); state(); } return; }
  if (t==base+"/monster/set" && validMonster(value)) {
    monster=value; showMonster(monster.c_str());
  } else if (t==base+"/volume/set") {
    if (value.isEmpty()) return;
    for (unsigned i=0;i<value.length();++i) if (value[i]<'0' || value[i]>'9') return;
    if (value.length()>3 || value.toInt()>100) return;
    audioVolume=value.toInt();
  } else if (t==base+"/power/set" && (value=="ON" || value=="OFF")) {
    enabled=value=="ON"; screenPower(enabled);
    if (!enabled) { track="silence"; stopAudio(); }
  } else if (t==base+"/track/set" && (value=="ambiance" || value=="voix" || value=="silence")) {
    track=value; startTrack();
  } else if (t==base+"/scare/set" && value=="PRESS") {
    enabled=true; screenPower(true); showMonster(monster.c_str()); track="voix"; startTrack();
  } else if (t==base+"/stop/set" && value=="PRESS") {
    track="silence"; stopAudio();
  } else return;
  state();
}
void setup() {
  Serial.begin(115200); delay(200);
  if (!psramFound()) { Serial.println("PSRAM absente : verifier carte et reglage OPI"); while (true) delay(1000); }
  heap_caps_malloc_extmem_enable(4096);
  I2C_Init(); TCA9554PWR_Init(0x00);
  if (!initScreen()) { Serial.println("Initialisation ecran impossible"); while (true) delay(1000); }
  SD_MMC.setPins(14,17,16);
  sdReady=SD_MMC.begin("/sdcard",true) && SD_MMC.cardType()!=CARD_NONE;
  showMonster(monster.c_str());
  audioReady=initAudio();
  Serial.printf("microSD=%s audio=%s\n",sdReady?"OK":"absente",audioReady?"OK":"echec");
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
  if (strlen(WIFI_SSID)) WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  else Serial.println("Mode autonome : renseigner include/config.local.h pour WiFi/MQTT");
  mqtt.setServer(MQTT_HOST,MQTT_PORT); mqtt.setBufferSize(1536);
  mqtt.setCallback(message); mqtt.setSocketTimeout(1); mqtt.setKeepAlive(20);
}
void loop() {
  static uint32_t lastConnect=0, lastState=0;
  screenLoop();
  if (WiFi.status()==WL_CONNECTED && strlen(MQTT_HOST)) {
    if (!mqtt.connected() && millis()-lastConnect>5000) {
      lastConnect=millis();
      if (mqtt.connect(DEVICE_ID,MQTT_USER,MQTT_PASSWORD,(base+"/availability").c_str(),1,true,"offline")) {
        mqtt.subscribe((base+"/+/set").c_str()); mqtt.subscribe("homeassistant/status");
        discover(); state(); mqtt.publish((base+"/availability").c_str(),"online",true);
      } else Serial.printf("MQTT connexion echouee (%d)\n",mqtt.state());
    }
    mqtt.loop();
    if (mqtt.connected() && millis()-lastState>1000) { lastState=millis(); state(); }
  }
  delay(5);
}
