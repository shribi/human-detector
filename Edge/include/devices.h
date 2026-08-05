#pragma once 

void initI2S();
void initRadar();

void createRadarTask();
void createI2STask();

void registerAudioStreamCallback(void (*onAudioStream)(int16_t*, size_t, size_t));
void registerRadarCallback(void (*onPresenceDetected)());

void startHumanDetection();
void stopHumanDetection();

void stopRecordingI2S();
void startRecordingI2S();