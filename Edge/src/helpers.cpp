#include <nvs_flash.h>
#include <WiFi.h>

#include "helpers.h"

#include "logger.h"

//==================================================
// Helper Functions
//==================================================
Config loadConfigFromNVS() {
    getPreferences().begin("config", true);
    const String ssid = getPreferences().getString("ssid", "");
    const String password = getPreferences().getString("password", "");
    const String reservoir_ip = getPreferences().getString("reservoir_ip", "");
    getPreferences().end();
    Config config;
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    strncpy(config.reservoir_ip, reservoir_ip.c_str(), sizeof(config.reservoir_ip) - 1);
    config.reservoir_ip[sizeof(config.reservoir_ip) - 1] = '\0';
    return config;
}

bool credentialsExist() {
    const Config config = loadConfigFromNVS();
    return strlen(config.ssid) > 0 && strlen(config.password) > 0;
}

void factoryReset() {
    log("Performing Factory Reset");
    getPreferences().begin("config", false);
    getPreferences().clear();
    getPreferences().end();
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK)
    {
        log("NVS erased.");
    }
    nvs_flash_init();
    ESP.restart();
}

void connectToWiFi() {
    auto config = loadConfigFromNVS();
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(config.ssid, config.password);
    log("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        log(".");
        delay(500);
    }
    log("Connected to WiFi");
    log("IP Address: " + WiFi.localIP().toString());
}