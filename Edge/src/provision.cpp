#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFi.h>

#include "provision.h"

#include "logger.h"
#include "constants.h"
#include "helpers.h"

static QueueHandle_t provisioningQueue;

void createProvisioningTask() {
    provisioningQueue = xQueueCreate(1, sizeof(ProvisionTaskCtx));
    xTaskCreatePinnedToCore(
        [](void*){ provisioningTask(); },
        "ProvisioningTask",
        4096,
        nullptr,
        1,
        nullptr,
        0);
}

void provisionQueueSend(eEventType eventType, const Config& config, bool performFactoryReset) {
    ProvisionTaskCtx ctx;
    ctx.eventType = eventType;
    ctx.config = config;
    ctx.performFactoryReset = performFactoryReset;
    xQueueSend(provisioningQueue, &ctx, portMAX_DELAY);
}

void provisioningTask() {
    while (true) {
        ProvisionTaskCtx receivedConfig;
        if (xQueueReceive(provisioningQueue, &receivedConfig, portMAX_DELAY)) {
            log("Received configuration from HTTP submit");
            if (receivedConfig.eventType == EVT_DEV_RESET) {
                factoryReset();
            }
            else if (receivedConfig.eventType == EVT_DEV_CONFIG) {
                log("Attempting to connect to WiFi with provided credentials");
                if (strlen(receivedConfig.config.ssid) > 0 && strlen(receivedConfig.config.password) > 0) {
                    log(String("SSID: ") + receivedConfig.config.ssid + ", Password: " + receivedConfig.config.password);
                    WiFi.mode(WIFI_STA);
                    WiFi.begin(receivedConfig.config.ssid, receivedConfig.config.password);
                    log("Attempting to connect to WiFi...");
                    unsigned long startAttemptTime = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
                        delay(500);
                        Serial.print(".");
                    }
                    if (WiFi.status() == WL_CONNECTED) {
                        log("\nConnected to WiFi!");
                        getPreferences().begin("config", false);
                        getPreferences().putString("ssid", String(receivedConfig.config.ssid));
                        getPreferences().putString("password", String(receivedConfig.config.password));
                        getPreferences().end();
                        log("Credentials saved to NVS");

                        // Trigger System Restart
                        log("Restarting system...");
                        ESP.restart();
                    } else {
                        log("\nFailed to connect to WiFi. Please try again.");
                    }
                }
            }
            else {
                log("Unknown event type received in provisioning task");
            }
        }
        
        // Delay to prevent task from consuming too much CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

