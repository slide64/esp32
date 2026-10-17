#pragma once
#include <Arduino.h>
#include <atomic>
bool initAudio();
const char *audioDiagnostic();
void playAudio(const char *path, bool loop = false);
void stopAudio();
extern std::atomic<unsigned> audioVolume;
extern std::atomic<bool> audioPlaying;
