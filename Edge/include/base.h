#pragma once
#include <Arduino.h>
#include <Preferences.h>

//==================================================
// Modals
//==================================================
enum eEventType {
    EVT_NONE,
    EVT_DEV_RESET,
    EVT_DEV_CONFIG,
};

enum class eWsEventType {
    WsCmdUnknown,
    WsCmdHumanDetected,
    WsCmdAudioStream,
    WsCmdStartRecording,
    WsCmdStartRecordingAck,
    WsCmdEndRecording,
    WsCmdEndRecordingAck,
    WsHeartbeat,
    WsReboot,
    WsFactoryReset,
    WsStopHumanDetection,
    WsStartHumanDetection,
};

struct Config {
    char ssid[32];
    char password[64];
    char reservoir_ip[16];
};

struct LogMessage {
    char message[128];
    unsigned long timestamp;
};

struct ProvisionTaskCtx {
    eEventType eventType;
    Config config;
    bool performFactoryReset = false;
};

static Preferences& getPreferences() {
    static Preferences preferences;
    return preferences;
}

struct WsMessage {
    eWsEventType cmd;
    String data;
};